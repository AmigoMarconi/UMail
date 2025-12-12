#pragma once
#include "string"
#include "vector"

using namespace std;

class Base64 {
public:
	static string encode(const string& data);

	static string encode(const vector<unsigned char>& data);

	static string decode(const string& encoded);

	static bool isValid(const string& str);

private:
	static const string BASE64_CHARS;
	static bool isBase64(unsigned char c);

};