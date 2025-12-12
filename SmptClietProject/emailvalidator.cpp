#include "emailvalidator.h"
#include <algorithm>
#include <cctype>
#include <sstream>

bool EmailValidator::isValidEmail(const std::string& email) {
    std::string clean = cleanEmail(email);

    // 1. Проверяем наличие @
    size_t atPos = clean.find('@');
    if (atPos == std::string::npos ||
        atPos == 0 ||
        atPos == clean.length() - 1) {
        return false;
    }

    std::string localPart = clean.substr(0, atPos);
    std::string domain = clean.substr(atPos + 1);

    // 2. Проверяем local part
    if (localPart.empty() || localPart.length() > 64) {
        return false;
    }

    // Проверяем точки в local part
    if (localPart[0] == '.' || localPart[localPart.length() - 1] == '.') {
        return false;
    }

    if (localPart.find("..") != std::string::npos) {
        return false;
    }

    // Проверяем допустимые символы в local part
    for (char c : localPart) {
        if (!(std::isalnum(c) || c == '!' || c == '#' || c == '$' || c == '%' ||
            c == '&' || c == '\'' || c == '*' || c == '+' || c == '-' ||
            c == '/' || c == '=' || c == '?' || c == '^' || c == '_' ||
            c == '`' || c == '{' || c == '|' || c == '}' || c == '~' || c == '.')) {
            return false;
        }
    }

    // 3. Проверяем домен
    return isValidDomain(domain);
}

std::vector<std::string> EmailValidator::parseEmailList(const std::string& emailList) {
    std::vector<std::string> result;
    std::stringstream ss(emailList);
    std::string item;

    // Разделяем по запятой
    while (std::getline(ss, item, ',')) {
        // Убираем пробелы
        size_t start = item.find_first_not_of(" \t\n\r");
        size_t end = item.find_last_not_of(" \t\n\r");

        if (start != std::string::npos && end != std::string::npos) {
            std::string email = item.substr(start, end - start + 1);
            if (isValidEmail(email)) {
                result.push_back(email);
            }
        }
    }

    return result;
}

std::string EmailValidator::cleanEmail(const std::string& email) {
    std::string result = email;

    // Убираем пробелы по краям
    size_t start = result.find_first_not_of(" \t\n\r");
    size_t end = result.find_last_not_of(" \t\n\r");

    if (start != std::string::npos && end != std::string::npos) {
        result = result.substr(start, end - start + 1);
    }

    return result;
}

std::string EmailValidator::extractDomain(const std::string& email) {
    size_t atPos = email.find('@');
    if (atPos != std::string::npos && atPos < email.length() - 1) {
        return email.substr(atPos + 1);
    }
    return "";
}

bool EmailValidator::isValidDomain(const std::string& domain) {
    if (domain.empty() || domain.length() > 255) {
        return false;
    }

    // Проверяем точки
    if (domain[0] == '.' || domain[domain.length() - 1] == '.') {
        return false;
    }

    // Должна быть хотя бы одна точка
    if (domain.find('.') == std::string::npos) {
        return false;
    }

    // Разбиваем на части
    std::stringstream ss(domain);
    std::string part;

    while (std::getline(ss, part, '.')) {
        if (part.empty() || part.length() > 63) {
            return false;
        }

        if (part[0] == '-' || part[part.length() - 1] == '-') {
            return false;
        }

        // Проверяем символы в каждой части домена
        for (char c : part) {
            if (!(std::isalnum(c) || c == '-')) {
                return false;
            }
        }
    }

    return true;
}

std::string EmailValidator::formatEmailList(const std::vector<std::string>& emails) {
    std::stringstream result;
    for (size_t i = 0; i < emails.size(); i++) {
        if (i > 0) result << ", ";
        result << emails[i];
    }
    return result.str();
}