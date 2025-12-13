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