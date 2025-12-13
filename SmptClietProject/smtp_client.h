#ifndef SMTP_CLIENT_H
#define SMTP_CLIENT_H

#include <string>
#include <vector>

// Подключаем Boost.Asio.
#include <boost/asio.hpp>

class SmtpClient
{
public:
    // Коды ошибок, чтобы понять, на каком этапе всё сломалось.
    enum class SmtpError
    {
        None,              // Ошибок нет
        ResolveError,      // Не удалось найти сервер (DNS/адрес)
        ConnectError,      // Не удалось подключиться к серверу
        GreetingError,     // Неверный приветственный код (ожидали 220)
        HeloError,         // Ошибка при выполнении HELO
        AuthError,         // Ошибка авторизации AUTH LOGIN
        MailFromError,     // Ошибка при MAIL FROM
        RcptToError,       // Ошибка при RCPT TO
        DataError,         // Ошибка при DATA/отправке тела письма
        QuitError,         // Ошибка при QUIT
        NetworkIOError,    // Общая ошибка ввода/вывода в сети
        UnexpectedReply    // Сервер ответил не тем кодом, который ожидали
    };

    // server  - доменное имя или IP SMTP-сервера
    // port    - порт SMTP (обычно 25, 587 и т.п.)
    SmtpClient(const std::string& server, unsigned short port);

    // Запрещаем копирование, чтобы не было путаницы
    SmtpClient(const SmtpClient&) = delete;
    SmtpClient& operator=(const SmtpClient&) = delete;

    // Подключиться к серверу и выполнить HELO
    bool connect();

    // Авторизация по схеме AUTH LOGIN
    bool authenticateLogin(const std::string& user,
        const std::string& password);

    // Отправка письма
    bool sendMail(const std::string& from,
        const std::vector<std::string>& to,
        const std::string& rawMessage);

    // Корректно завершить сессию и закрыть соединение.
    void disconnect();

    // Состояние
    bool isConnected() const;
    bool isAuthenticated() const;

    // Информация об ошибках
    SmtpError lastError() const;
    const std::string& lastErrorMessage() const;
    const std::string& lastServerReply() const;

private:
    // Вспомогательные методы
    bool sendLine(const std::string& line);
    bool readReply(int& code);
    bool expectCode(int expectedCode);

private:
    std::string server_;
    unsigned short port_;

    // === Поля Boost.Asio ===
    // io_context
    boost::asio::io_context ioContext_;
    // TCP-сокет, привязанный к этому io_context
    boost::asio::ip::tcp::socket socket_;

    bool connected_;
    bool authenticated_;

    SmtpError lastError_;
    std::string lastErrorMessage_;
    std::string lastReplyLine_;
};

#endif
