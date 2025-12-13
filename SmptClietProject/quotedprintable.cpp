#include "quotedprintable.h"
#include <iomanip>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <locale>
#include <codecvt>

const std::string QuotedPrintable::HEX_CHARS = "0123456789ABCDEF";

std::string QuotedPrintable::encodeText(const std::string& utf8_text, size_t lineLength) {
    std::stringstream result;
    size_t linePos = 0;

    // Проходим по КАЖДОМУ БАЙТУ UTF-8 строки
    for (size_t i = 0; i < utf8_text.length(); i++) {
        unsigned char c = utf8_text[i];
        std::string encodedChar;

        // Обработка конца строки
        if (c == '\n' || c == '\r') {
            if (c == '\n') {
                result << "\r\n";  // В email всегда \r\n
                linePos = 0;
            }
            continue;
        }

        // Кодируем если нужно
        if (needsEncoding(c)) {
            // Кодируем как =XX
            std::stringstream hex;
            hex << '=' << std::uppercase << std::setfill('0') << std::setw(2)
                << std::hex << static_cast<int>(c);
            encodedChar = hex.str();
        }
        else {
            encodedChar = c;
        }

        // Проверяем длину строки (макс 76 символов)
        if (linePos + encodedChar.length() > lineLength - 1) {
            result << "=\r\n";  // мягкий перенос строки
            linePos = 0;
        }

        result << encodedChar;
        linePos += encodedChar.length();
    }

    return result.str();
}

//ФУНКЦИЯ для заголовков
std::string QuotedPrintable::encodeHeader(const std::string& utf8_text,
    const std::string& charset) {

    std::stringstream result;
    result << "=?" << charset << "?Q?";

    // Кодируем КАЖДЫЙ БАЙТ UTF-8
    for (size_t i = 0; i < utf8_text.length(); i++) {
        unsigned char c = utf8_text[i];

        if (c == ' ') {
            result << '_';
        }
        else if (needsEncoding(c) || c == '?' || c == '=' || c == '_' || c == '.') {
            // ВСЕ специальные символы кодируем
            std::stringstream hex;
            hex << '=' << std::uppercase << std::setfill('0') << std::setw(2)
                << std::hex << static_cast<int>(c);
            result << hex.str();
        }
        else if (c >= 33 && c <= 126) {
            // Печатные ASCII символы оставляем как есть
            result << c;
        }
        else {
            // Все остальные (включая UTF-8 байты) кодируем
            std::stringstream hex;
            hex << '=' << std::uppercase << std::setfill('0') << std::setw(2)
                << std::hex << static_cast<int>(c);
            result << hex.str();
        }
    }

    result << "?=";
    return result.str();
}


std::string QuotedPrintable::decode(const std::string& encoded) {
    std::stringstream result;
    size_t i = 0;

    while (i < encoded.length()) {
        if (encoded[i] == '=') {
            if (i + 2 < encoded.length() &&
                encoded[i + 1] == '\r' && encoded[i + 2] == '\n') {
                i += 3; // пропускаем =\r\n
                continue;
            }

            // Декодируем =XX
            if (i + 2 < encoded.length() &&
                isHexDigit(encoded[i + 1]) && isHexDigit(encoded[i + 2])) {
                char c = hexToChar(encoded[i + 1], encoded[i + 2]);
                result << c;
                i += 3;
            }
            else {
                // Некорректная последовательность
                result << encoded[i];
                i++;
            }
        }
        else if (encoded[i] == '_') {
            result << ' ';
            i++;
        }
        else {
            result << encoded[i];
            i++;
        }
    }

    return result.str();
}

bool QuotedPrintable::needsEncoding(unsigned char c) {
    // Кодируем всё, кроме печатных ASCII символов 33-126
    // НЕ кодируем: пробел, табуляция (но они обрабатываются отдельно)
    // = всегда кодируется
    return (c < 33 || c > 126 || c == '=');
}

bool QuotedPrintable::isHexDigit(char c) {
    return (c >= '0' && c <= '9') ||
        (c >= 'A' && c <= 'F') ||
        (c >= 'a' && c <= 'f');
}

char QuotedPrintable::hexToChar(char hex1, char hex2) {
    int value = 0;

    // Первая шестнадцатеричная цифра
    if (hex1 >= '0' && hex1 <= '9')
        value += (hex1 - '0') * 16;
    else if (hex1 >= 'A' && hex1 <= 'F')
        value += (hex1 - 'A' + 10) * 16;
    else if (hex1 >= 'a' && hex1 <= 'f')
        value += (hex1 - 'a' + 10) * 16;

    // Вторая шестнадцатеричная цифра
    if (hex2 >= '0' && hex2 <= '9')
        value += (hex2 - '0');
    else if (hex2 >= 'A' && hex2 <= 'F')
        value += (hex2 - 'A' + 10);
    else if (hex2 >= 'a' && hex2 <= 'f')
        value += (hex2 - 'a' + 10);

    return static_cast<char>(value);
}