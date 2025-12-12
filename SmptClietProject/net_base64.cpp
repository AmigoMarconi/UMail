#include "base64.h"

std::string base64Encode(const std::string& input)
{
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string output;
    unsigned int value = 0;
    int bits = -6;

    for (unsigned char c : input)
    {
        value = (value << 8) | c;
        bits += 8;
        while (bits >= 0)
        {
            output.push_back(table[(value >> bits) & 0x3F]);
            bits -= 6;
        }
    }

    if (bits > -6)
    {
        output.push_back(table[((value << 8) >> (bits + 8)) & 0x3F]);
    }

    while (output.size() % 4 != 0)
    {
        output.push_back('=');
    }

    return output;
}
