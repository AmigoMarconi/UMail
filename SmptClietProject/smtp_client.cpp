#include "smtp_client.h"

#include "base64.h"

#include <cctype>   // для std::isdigit
#include <string>   // на всякий случай

// Чтобы короче писать tcp::resolver, tcp::socket
using boost::asio::ip::tcp;

// Конструктор: сохраняем сервер и порт, создаём io_context и сокет.
SmtpClient::SmtpClient(const std::string& server, unsigned short port)
    : server_(server),
    port_(port),
    ioContext_(),
    socket_(ioContext_),
    connected_(false),
    authenticated_(false),
    lastError_(SmtpError::None),
    lastErrorMessage_(),
    lastReplyLine_()
{
}

// Реальный connect() c Boost.Asio:
// 1) resolve(host, port)
// 2) connect()
// 3) читаем код 220
// 4) отправляем EHLO localhost и ждём 250
bool SmtpClient::connect()
{
    // Сброс состояния
    lastError_ = SmtpError::None;
    lastErrorMessage_.clear();
    lastReplyLine_.clear();
    authenticated_ = false;

    boost::system::error_code ec;

    // --- 1. Разрешаем адрес (DNS / IP) ---
    tcp::resolver resolver(ioContext_);
    auto endpoints = resolver.resolve(server_, std::to_string(port_), ec);
    if (ec)
    {
        lastError_ = SmtpError::ResolveError;
        lastErrorMessage_ = "Resolve failed: " + ec.message();
        return false;
    }

    // --- 2. Подключаемся к одному из эндпоинтов ---
    boost::asio::connect(socket_, endpoints, ec);
    if (ec)
    {
        lastError_ = SmtpError::ConnectError;
        lastErrorMessage_ = "Connect failed: " + ec.message();
        return false;
    }

    // --- 3. Читаем приветствие сервера (ожидаем код 220) ---
    int code = 0;
    if (!readReply(code))
    {
        if (lastError_ == SmtpError::None)
        {
            lastError_ = SmtpError::GreetingError;
            lastErrorMessage_ = "Failed to read greeting from server";
        }
        boost::system::error_code closeEc;
        socket_.close(closeEc);
        return false;
    }

    if (code != 220)
    {
        lastError_ = SmtpError::GreetingError;
        lastErrorMessage_ =
            "Server greeting code is " + std::to_string(code) +
            ", expected 220";
        boost::system::error_code closeEc;
        socket_.close(closeEc);
        return false;
    }

    // --- 4. Отправляем EHLO localhost ---
    if (!sendLine("EHLO localhost"))
    {
        boost::system::error_code closeEc;
        socket_.close(closeEc);
        return false;
    }

    // --- 5. Читаем ответ на EHLO, ожидаем код 250 ---
    int heloCode = 0;
    if (!readReply(heloCode))
    {
        if (lastError_ == SmtpError::None)
        {
            lastError_ = SmtpError::HeloError;
            lastErrorMessage_ = "Failed to read EHLO reply";
        }
        boost::system::error_code closeEc;
        socket_.close(closeEc);
        return false;
    }

    if (heloCode != 250)
    {
        lastError_ = SmtpError::HeloError;
        lastErrorMessage_ =
            "Server EHLO code is " + std::to_string(heloCode) +
            ", expected 250";
        boost::system::error_code closeEc;
        socket_.close(closeEc);
        return false;
    }

    connected_ = true;
    return true;
}

// Пока авторизация — простая заглушка.

bool SmtpClient::authenticateLogin(const std::string& user,
    const std::string& password)
{
    if (!connected_)
    {
        lastError_ = SmtpError::ConnectError;
        lastErrorMessage_ = "Not connected";
        return false;
    }

    // 1. AUTH LOGIN
    if (!sendLine("AUTH LOGIN"))
    {
        lastError_ = SmtpError::AuthError;
        if (lastErrorMessage_.empty())
            lastErrorMessage_ = "Failed to send AUTH LOGIN";
        return false;
    }

    int code = 0;
    if (!readReply(code) || code != 334)
    {
        lastError_ = SmtpError::AuthError;
        if (lastErrorMessage_.empty())
        {
            lastErrorMessage_ =
                "AUTH LOGIN: server replied " + std::to_string(code) +
                ", expected 334";
        }
        return false;
    }

    // 2. base64(login)
    std::string user64 = Base64::encode(user);
    if (!sendLine(user64))
    {
        lastError_ = SmtpError::AuthError;
        if (lastErrorMessage_.empty())
            lastErrorMessage_ = "Failed to send base64 user";
        return false;
    }

    if (!readReply(code) || code != 334)
    {
        lastError_ = SmtpError::AuthError;
        if (lastErrorMessage_.empty())
        {
            lastErrorMessage_ =
                "AUTH LOGIN user: server replied " + std::to_string(code) +
                ", expected 334";
        }
        return false;
    }

    // 3. base64(password)
    std::string pass64 = Base64::encode(password);
    if (!sendLine(pass64))
    {
        lastError_ = SmtpError::AuthError;
        if (lastErrorMessage_.empty())
            lastErrorMessage_ = "Failed to send base64 password";
        return false;
    }

    if (!readReply(code) || code != 235)
    {
        lastError_ = SmtpError::AuthError;
        if (lastErrorMessage_.empty())
        {
            lastErrorMessage_ =
                "AUTH LOGIN password: server replied " + std::to_string(code) +
                ", expected 235";
        }
        return false;
    }

    authenticated_ = true;
    lastError_ = SmtpError::None;
    lastErrorMessage_.clear();
    return true;
}


// Заглушка отправки письма — настоящую реализацию

bool SmtpClient::sendMail(const std::string& from,
    const std::vector<std::string>& to,
    const std::string& rawMessage)
{
    if (!connected_)
    {
        lastError_ = SmtpError::ConnectError;
        lastErrorMessage_ = "Not connected";
        return false;
    }

    // Авторизация может быть не обязательной (локальный сервер smtp4dev).
    // Поэтому не прерываемся, если authenticated_ == false.
    // Если сервер требует AUTH, он сам вернёт ошибку на MAIL FROM / RCPT TO.

    // --- MAIL FROM ---
    std::string mailFromCmd = "MAIL FROM:<" + from + ">";

    if (!sendLine(mailFromCmd))
    {
        lastError_ = SmtpError::MailFromError;
        if (lastErrorMessage_.empty())
            lastErrorMessage_ = "Failed to send MAIL FROM";
        return false;
    }

    int code = 0;
    if (!readReply(code) || code != 250)
    {
        lastError_ = SmtpError::MailFromError;
        if (lastErrorMessage_.empty())
        {
            lastErrorMessage_ =
                "MAIL FROM: server replied " + std::to_string(code) +
                ", expected 250";
        }
        return false;
    }

    // --- RCPT TO для каждого получателя ---
    if (to.empty())
    {
        lastError_ = SmtpError::RcptToError;
        lastErrorMessage_ = "Recipients list is empty";
        return false;
    }

    for (const std::string& addr : to)
    {
        std::string rcptCmd = "RCPT TO:<" + addr + ">";
        if (!sendLine(rcptCmd))
        {
            lastError_ = SmtpError::RcptToError;
            if (lastErrorMessage_.empty())
                lastErrorMessage_ = "Failed to send RCPT TO";
            return false;
        }

        code = 0;
        if (!readReply(code) || (code != 250 && code != 251))
        {
            lastError_ = SmtpError::RcptToError;
            if (lastErrorMessage_.empty())
            {
                lastErrorMessage_ =
                    "RCPT TO: server replied " + std::to_string(code) +
                    ", expected 250 or 251";
            }
            return false;
        }
    }

    // --- DATA ---
    if (!sendLine("DATA"))
    {
        lastError_ = SmtpError::DataError;
        if (lastErrorMessage_.empty())
            lastErrorMessage_ = "Failed to send DATA";
        return false;
    }

    code = 0;
    if (!readReply(code) || code != 354)
    {
        lastError_ = SmtpError::DataError;
        if (lastErrorMessage_.empty())
        {
            lastErrorMessage_ =
                "DATA: server replied " + std::to_string(code) +
                ", expected 354";
        }
        return false;
    }

    // --- Отправляем само письмо ---
    std::string dataToSend = rawMessage;
    // Убеждаемся, что письмо заканчивается на \r\n
    if (dataToSend.size() < 2 ||
        dataToSend[dataToSend.size() - 2] != '\r' ||
        dataToSend[dataToSend.size() - 1] != '\n')
    {
        dataToSend += "\r\n";
    }

    boost::system::error_code ec;
    boost::asio::write(socket_, boost::asio::buffer(dataToSend), ec);
    if (ec)
    {
        lastError_ = SmtpError::NetworkIOError;
        lastErrorMessage_ = "Write mail data failed: " + ec.message();
        return false;
    }

    // --- Завершающая точка ---
    if (!sendLine("."))
    {
        lastError_ = SmtpError::DataError;
        if (lastErrorMessage_.empty())
            lastErrorMessage_ = "Failed to send final dot";
        return false;
    }

    code = 0;
    if (!readReply(code) || code != 250)
    {
        lastError_ = SmtpError::DataError;
        if (lastErrorMessage_.empty())
        {
            lastErrorMessage_ =
                "After DATA: server replied " + std::to_string(code) +
                ", expected 250";
        }
        return false;
    }

    lastError_ = SmtpError::None;
    lastErrorMessage_.clear();
    return true;
}


void SmtpClient::disconnect()
{
    // Если соединение ещё активно, попробуем вежливо завершить сессию.
    if (socket_.is_open() && connected_)
    {
        // Отправляем QUIT, но не считаем ошибку фатальной,
        // если вдруг сервер уже закрыл соединение.
        sendLine("QUIT");

        int code = 0;
        readReply(code); // обычно 221, но, если нет — просто игнорируем
    }

    // В любом случае закрываем сокет.
    boost::system::error_code ec;
    if (socket_.is_open())
    {
        socket_.close(ec);
    }

    connected_ = false;
    authenticated_ = false;
}


bool SmtpClient::isConnected() const
{
    return connected_;
}

bool SmtpClient::isAuthenticated() const
{
    return authenticated_;
}

SmtpClient::SmtpError SmtpClient::lastError() const
{
    return lastError_;
}

const std::string& SmtpClient::lastErrorMessage() const
{
    return lastErrorMessage_;
}

const std::string& SmtpClient::lastServerReply() const
{
    return lastReplyLine_;
}

// --- Вспомогательные методы ---

// Отправить строку + CRLF в сокет.
bool SmtpClient::sendLine(const std::string& line)
{
    if (!socket_.is_open())
    {
        lastError_ = SmtpError::NetworkIOError;
        lastErrorMessage_ = "Socket is not open";
        return false;
    }

    std::string data = line + "\r\n";

    boost::system::error_code ec;
    boost::asio::write(socket_, boost::asio::buffer(data), ec);
    if (ec)
    {
        lastError_ = SmtpError::NetworkIOError;
        lastErrorMessage_ = "Write failed: " + ec.message();
        return false;
    }

    return true;
}

// Читаем одну строку ответа сервера до '\n',
// сохраняем её в lastReplyLine_ и выдёргиваем
// первые три символа как числовой код.
bool SmtpClient::readReply(int& code)
{
    code = 0;
    lastReplyLine_.clear();

    if (!socket_.is_open())
    {
        lastError_ = SmtpError::NetworkIOError;
        lastErrorMessage_ = "Socket is not open";
        return false;
    }

    boost::asio::streambuf buffer;
    boost::system::error_code ec;

    std::size_t n = boost::asio::read_until(socket_, buffer, "\n", ec);
    (void)n; // просто чтобы не ругался компилятор

    if (ec)
    {
        lastError_ = SmtpError::NetworkIOError;
        lastErrorMessage_ = "Read failed: " + ec.message();
        return false;
    }

    std::istream is(&buffer);
    std::getline(is, lastReplyLine_);

    // Убираем '\r' в конце, если он есть
    if (!lastReplyLine_.empty() && lastReplyLine_.back() == '\r')
    {
        lastReplyLine_.pop_back();
    }

    // Должно быть минимум 3 символа с цифрами
    if (lastReplyLine_.size() < 3 ||
        !std::isdigit(static_cast<unsigned char>(lastReplyLine_[0])) ||
        !std::isdigit(static_cast<unsigned char>(lastReplyLine_[1])) ||
        !std::isdigit(static_cast<unsigned char>(lastReplyLine_[2])))
    {
        lastError_ = SmtpError::UnexpectedReply;
        lastErrorMessage_ = "Invalid reply from server: " + lastReplyLine_;
        return false;
    }

    code = (lastReplyLine_[0] - '0') * 100 +
        (lastReplyLine_[1] - '0') * 10 +
        (lastReplyLine_[2] - '0');

    return true;
}

// Ожидаем конкретный код ответа от сервера.
bool SmtpClient::expectCode(int expectedCode)
{
    int code = 0;
    if (!readReply(code))
        return false;

    if (code != expectedCode)
    {
        lastError_ = SmtpError::UnexpectedReply;
        lastErrorMessage_ =
            "Expected reply code " + std::to_string(expectedCode) +
            ", but got " + std::to_string(code);
        return false;
    }

    return true;
}
