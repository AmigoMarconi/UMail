/*#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <windows.h>

#include "mail_service.h"

// Чтобы u8"..." работало и в C++17, и в C++20
static std::string u8str(const char* s) {
    return std::string(s);
}
#if defined(__cpp_char8_t)
static std::string u8str(const char8_t* s) {
    return std::string(reinterpret_cast<const char*>(s));
}
#endif

static bool readFileBytes(const std::filesystem::path& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    out.resize((size_t)size);
    if (size > 0) f.read(reinterpret_cast<char*>(out.data()), size);
    return true;
}

int main() {
    // UTF-8 в консоли (чтобы русский в выводе читался)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // --- SMTP настройки для smtp4dev ---
    SmtpSettings smtp;
    smtp.server = "localhost";
    smtp.port = 25;
    smtp.useAuth = false; // smtp4dev обычно без авторизации

    // --- Письмо ---
    MailDraft d;
    d.from = "user@example.com";
    d.to = { "recipient@example.com" };
    d.subject = u8str(u8"Тест тема");
    d.body = u8str(u8"Привет!\r\nЭто письмо с вложением.\r\n");

    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

    // 1) JPG
    {
        Attachment a;
        a.filename = "erdemka.jpg";
        a.mimeType = "image/jpeg";

        auto p = exeDir / "erdemka.jpg";
        if (!readFileBytes(p, a.data)) {
            std::cout << u8str(u8"Не удалось прочитать файл: ") << p.string() << "\n";
            return 0;
        }
        d.attachments.push_back(a);
    }

    // 2) TXT
    {
        Attachment a;
        a.filename = "test.txt";
        a.mimeType = "text/plain";

        auto p = exeDir / "test.txt";
        if (!readFileBytes(p, a.data)) {
            std::cout << u8str(u8"Не удалось прочитать файл: ") << p.string() << "\n";
            return 0;
        }
        d.attachments.push_back(a);
    }

    // 3) PDF
    {
        Attachment a;
        a.filename = "doc.pdf";
        a.mimeType = "application/pdf";

        auto p = exeDir / "doc.pdf";
        if (!readFileBytes(p, a.data)) {
            std::cout << u8str(u8"Не удалось прочитать файл: ") << p.string() << "\n";
            return 0;
        }
        d.attachments.push_back(a);
    }

    std::string err;
    if (MailService::send(smtp, d, err)) {
        std::cout << "Send: OK\n";
    }
    else {
        std::cout << "Send: FAIL\n" << err << "\n";
    }

    return 0;
}
*/
#include <iostream>
#include <string>
#include <windows.h>
#include <vector>
#include "mail_service.h"

int main() {
    // UTF-8 в консоли
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "=== Тест отправки через smtp4dev ===\n\n";

    // 1. Проверяем, запущен ли smtp4dev (опционально)
    std::cout << "Проверяем соединение с smtp4dev (localhost:25)...\n";

    // Настройки для smtp4dev
    SmtpSettings smtp;
    smtp.server = "localhost";
    smtp.port = 25;
    smtp.useAuth = false; // smtp4dev без авторизации

    // Простейшее письмо БЕЗ ВЛОЖЕНИЙ
    MailDraft draft;
    draft.from = "test@example.com";
    draft.to = { "test@example.com" };
    draft.subject = "Тест из консоли";
    draft.body = "Привет!\r\nЭто тестовое письмо для проверки работы с smtp4dev.\r\n";

    std::cout << "Отправляем тестовое письмо...\n\n";

    std::string error;
    bool success = MailService::send(smtp, draft, error);

    if (success) {
        std::cout << "УСПЕШНО!\n";
        std::cout << "Письмо отправлено через smtp4dev.\n";
        std::cout << "Откройте http://localhost:5000 для просмотра письма.\n";
    }
    else {
        std::cout << "ОШИБКА!\n";
        std::cout << error << "\n\n";
        std::cout << "Возможные причины:\n";
        std::cout << "1. smtp4dev не запущен\n";
        std::cout << "2. Порт 25 занят\n";
        std::cout << "3. Проблемы с сетью\n";
    }

    std::cout << "\nНажмите Enter для выхода...";
    std::cin.get();
    return 0;
}