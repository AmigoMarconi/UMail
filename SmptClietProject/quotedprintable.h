#pragma once
#include <string>
#include <vector>

class QuotedPrintable {
public:
    // Кодировать текст (для email body)
    static std::string encodeText(const std::string& text,
        size_t lineLength = 76);

    // Кодировать для заголовков (Subject и т.д.)
    static std::string encodeHeader(const std::string& text,
        const std::string& charset = "utf-8");

    // Декодировать
    static std::string decode(const std::string& encoded);

    // Нужно ли кодировать этот символ?
    static bool needsEncoding(unsigned char c);

private:
    static const std::string HEX_CHARS;
    static char hexToChar(char hex1, char hex2);
    static bool isHexDigit(char c);
};