#include "mail_service.h"

#include "smtp_client.h"        // твой сетевой модуль
#include "emailvalidator.h"     // из UMail
#include "quotedprintable.h"    // из UMail
#include <fstream>
#include <cctype>
#include "base64.h"

static bool readFileBytes(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    out.resize((size_t)size);
    if (size > 0) f.read((char*)out.data(), size);
    return true;
}

static std::string wrap76(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i += 76) {
        out += s.substr(i, 76);
        out += "\r\n";
    }
    return out;
}

// Простая base64 для байтов (чтобы вложения работали 100%)
// (не зависит от того, какой base64 у помощника)
static std::string base64EncodeBytes(const std::vector<unsigned char>& data) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i < data.size()) {
        unsigned int a = data[i++];
        unsigned int b = (i < data.size()) ? data[i++] : 0;
        unsigned int c = (i < data.size()) ? data[i++] : 0;

        unsigned int triple = (a << 16) | (b << 8) | c;

        out.push_back(T[(triple >> 18) & 0x3F]);
        out.push_back(T[(triple >> 12) & 0x3F]);

        out.push_back((i - 1 <= data.size()) ? T[(triple >> 6) & 0x3F] : '=');
        out.push_back((i <= data.size()) ? T[triple & 0x3F] : '=');
    }

    // поправка '=' при нехватке байтов
    size_t mod = data.size() % 3;
    if (mod == 1) { out[out.size() - 1] = '='; out[out.size() - 2] = '='; }
    if (mod == 2) { out[out.size() - 1] = '='; }

    return out;
}


static std::string joinToHeader(const std::vector<std::string>& to) {
    std::string s;
    for (size_t i = 0; i < to.size(); ++i) {
        if (i) s += ", ";
        s += to[i];
    }
    return s;
}

bool MailService::hasNonAscii(const std::string& s) {
    for (unsigned char c : s) {
        if (c >= 128) return true; // русские/utf-8 байты
    }
    return false;
}

static std::string wrapBase64_76(const std::string& b64) {
    std::string out;
    for (size_t i = 0; i < b64.size(); i += 76) {
        out.append(b64.substr(i, 76));
        out += "\r\n";
    }
    return out;
}

std::string MailService::buildRawMessage(const MailDraft& draft) {
    // --- SUBJECT (RFC2047) ---
    std::string subjectUtf8 = draft.subject;   // должно быть UTF-8
    std::string subjectHeader = subjectUtf8;
    if (hasNonAscii(subjectUtf8)) {
        subjectHeader = "=?UTF-8?B?" + Base64::encode(subjectUtf8) + "?=";
    }

    // --- BODY (quoted-printable если есть не-ASCII) ---
    std::string bodyUtf8 = draft.body;         // должно быть UTF-8
    std::string bodyOut = bodyUtf8;
    std::string bodyCte = "7bit";
    if (hasNonAscii(bodyUtf8)) {
        bodyOut = QuotedPrintable::encodeText(bodyUtf8);
        bodyCte = "quoted-printable";
    }

    // --- Если вложений нет: обычный text/plain ---
    if (draft.attachments.empty()) {
        std::string msg;
        msg += "From: " + draft.from + "\r\n";
        msg += "To: " + joinToHeader(draft.to) + "\r\n";
        msg += "Subject: " + subjectHeader + "\r\n";
        msg += "MIME-Version: 1.0\r\n";
        msg += "Content-Type: text/plain; charset=\"utf-8\"\r\n";
        msg += "Content-Transfer-Encoding: " + bodyCte + "\r\n";
        msg += "\r\n";
        msg += bodyOut;
        if (msg.size() < 2 || msg.substr(msg.size() - 2) != "\r\n") msg += "\r\n";
        return msg;
    }

    // --- Если есть вложения: multipart/mixed ---
    const std::string boundary = "----=_UMailBoundary_42A7F0C1";

    std::string msg;
    msg += "From: " + draft.from + "\r\n";
    msg += "To: " + joinToHeader(draft.to) + "\r\n";
    msg += "Subject: " + subjectHeader + "\r\n";
    msg += "MIME-Version: 1.0\r\n";
    msg += "Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n";
    msg += "\r\n";

    // 1) Часть с текстом письма
    msg += "--" + boundary + "\r\n";
    msg += "Content-Type: text/plain; charset=\"utf-8\"\r\n";
    msg += "Content-Transfer-Encoding: " + bodyCte + "\r\n";
    msg += "\r\n";
    msg += bodyOut;
    if (msg.size() < 2 || msg.substr(msg.size() - 2) != "\r\n") msg += "\r\n";

    // 2) Части-вложения
    for (const auto& a : draft.attachments) {
        std::string bin(reinterpret_cast<const char*>(a.data.data()), a.data.size());
        std::string b64 = Base64::encode(bin);

        // --- ИСПРАВЛЕНИЕ: кодируем русское имя файла для email ---
        std::string encodedFilename;

        // Проверяем, есть ли не-ASCII символы
        bool hasNonAscii = false;
        for (unsigned char c : a.filename) {
            if (c >= 128) {
                hasNonAscii = true;
                break;
            }
        }

        if (hasNonAscii) {
            // Русское имя - кодируем в Base64 для заголовка (RFC 2047)
            // Формат: =?UTF-8?B?BASE64_STRING?=
            encodedFilename = "=?UTF-8?B?" + Base64::encode(a.filename) + "?=";
        }
        else {
            // Английское имя - оставляем как есть
            encodedFilename = a.filename;
        }

        msg += "--" + boundary + "\r\n";
        msg += "Content-Type: " + a.mimeType + "; name=\"" + encodedFilename + "\"\r\n";
        msg += "Content-Transfer-Encoding: base64\r\n";
        msg += "Content-Disposition: attachment; filename=\"" + encodedFilename + "\"\r\n";
        msg += "\r\n";
        msg += wrapBase64_76(b64);
    }

    // конец multipart
    msg += "--" + boundary + "--\r\n";
    return msg;
}


bool MailService::send(const SmtpSettings& smtp, const MailDraft& draft, std::string& errorOut) {
    errorOut.clear();

    // 1) Валидация адресов (UMail)
    if (!EmailValidator::isValidEmail(draft.from)) {
        errorOut = "Неверный адрес отправителя (From).";
        return false;
    }
    if (draft.to.empty()) {
        errorOut = "Список получателей пуст.";
        return false;
    }
    for (const auto& r : draft.to) {
        if (!EmailValidator::isValidEmail(r)) {
            errorOut = "Неверный адрес получателя: " + r;
            return false;
        }
    }

    // 2) Собираем готовое письмо
    std::string raw = buildRawMessage(draft);

    // 3) Отправка по сети (твой модуль)
    SmtpClient client(smtp.server, smtp.port);

    if (!client.connect()) {
        errorOut = "Connect failed: " + client.lastErrorMessage();
        return false;
    }

    if (smtp.useAuth) {
        if (!client.authenticateLogin(smtp.user, smtp.password)) {
            errorOut = "Auth failed: " + client.lastErrorMessage();
            client.disconnect();
            return false;
        }
    }

    if (!client.sendMail(draft.from, draft.to, raw)) {
        errorOut = "Send failed: " + client.lastErrorMessage();
        client.disconnect();
        return false;
    }

    client.disconnect();
    return true;
}
