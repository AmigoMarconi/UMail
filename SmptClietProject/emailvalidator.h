#pragma once
#include <string>
#include <vector>
#include <regex>

class EmailValidator {
public:
    // Основная проверка email
    static bool isValidEmail(const std::string& email);

    // Разобрать список email через запятую
    static std::vector<std::string> parseEmailList(const std::string& emailList);

    // Очистить email (убрать пробелы)
    static std::string cleanEmail(const std::string& email);

    // Извлечь домен из email
    static std::string extractDomain(const std::string& email);

    // Проверить валидность домена
    static bool isValidDomain(const std::string& domain);

    // Форматировать список email обратно в строку
    static std::string formatEmailList(const std::vector<std::string>& emails);
};