#include "GeneralUtils.h"

// C++
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <codecvt>

template <typename T>
inline size_t MinSize(size_t size, const std::basic_string<T>& string) {
    if (size == size_t(-1) || size > string.size()) {
        return string.size();
    } else {
        return size;
    }
}

inline bool IsLeadSurrogate(char16_t c) {
    return (0xD800 <= c) && (c <= 0xDBFF);
}

inline bool IsTrailSurrogate(char16_t c) {
    return (0xDC00 <= c) && (c <= 0xDFFF);
}

inline void PushUTF8CodePoint(std::string& ret, char32_t cp) {
    if (cp <= 0x007F) {
        ret.push_back(cp);
    } else if (cp <= 0x07FF) {
        ret.push_back(0xC0 | (cp >> 6));
        ret.push_back(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        ret.push_back(0xE0 | (cp >> 12));
        ret.push_back(0x80 | ((cp >> 6) & 0x3F));
        ret.push_back(0x80 | (cp & 0x3F));
    } else if (cp <= 0x10FFFF) {
        ret.push_back(0xF0 | (cp >> 18));
        ret.push_back(0x80 | ((cp >> 12) & 0x3F));
        ret.push_back(0x80 | ((cp >> 6) & 0x3F));
        ret.push_back(0x80 | (cp & 0x3F));
    } else {
        assert(false);
    }
}

// https://stackoverflow.com/questions/7153935/how-to-convert-utf-8-stdstring-to-utf-16-stdwstring/7154226
std::u16string GeneralUtils::UTF8ToUTF16(const std::string& utf8)
{
    std::vector<unsigned long> unicode;
    size_t i = 0;
    while (i < utf8.size())
    {
        unsigned long uni;
        size_t todo;
        bool error = false;
        unsigned char ch = utf8[i++];
        if (ch <= 0x7F)
        {
            uni = ch;
            todo = 0;
        }
        else if (ch <= 0xBF)
        {
            // probably fucked, continue regardless
            //throw std::logic_error("not a UTF-8 string");
            todo = 0;
            uni = 0xffef;
        }
        else if (ch <= 0xDF)
        {
            uni = ch&0x1F;
            todo = 1;
        }
        else if (ch <= 0xEF)
        {
            uni = ch&0x0F;
            todo = 2;
        }
        else if (ch <= 0xF7)
        {
            uni = ch&0x07;
            todo = 3;
        }
        else
        {
            todo = 0;
            uni = 0xffef;
            // probably fucked, continue regardless
        }
        for (size_t j = 0; j < todo; ++j)
        {
            if (i == utf8.size())
            {
                uni = 0xfffd;
                break;
            }
            unsigned char ch = utf8[i++];
            if (ch < 0x80 || ch > 0xBF) {
                uni = 0xfffd;
                break;
            }
            uni <<= 6;
            uni += ch & 0x3F;
        }

        if (uni > 0x10FFFF)
            uni = 0xfffd;
        unicode.push_back(uni);
    }

    std::u16string utf16;
    for (size_t i = 0; i < unicode.size(); ++i)
    {
        unsigned long uni = unicode[i];
        if (uni <= 0xFFFF)
        {
            utf16 += (uint16_t)uni;
        }
        else
        {
            // Cry
            utf16 += (uint16_t)0xfffd;
            /*uni -= 0x10000;
            utf16 += (uint16_t)((uni >> 10) + 0xD800);
            utf16 += (uint16_t)((uni & 0x3FF) + 0xDC00);*/
        }
    }
    return utf16;
}

//! Converts an std::string (ASCII) to UCS-2 / UTF-16
std::u16string GeneralUtils::ASCIIToUTF16(const std::string& string, size_t size) {
    size_t newSize = MinSize(size, string);
    std::u16string ret;
    ret.reserve(newSize);

    for (size_t i = 0; i < newSize; i++) {
        char c = string[i];
        assert(c > 0 && c <= 127);
        ret.push_back(static_cast<char16_t>(c));
    }

    return ret;
}

//! Converts a (potentially-ill-formed) UTF-16 string to UTF-8
//! See: <http://simonsapin.github.io/wtf-8/#decoding-ill-formed-utf-16>
std::string GeneralUtils::UTF16ToWTF8(const std::u16string& string, size_t size) {
    size_t newSize = MinSize(size, string);
    std::string ret;
    ret.reserve(newSize);

    for (size_t i = 0; i < newSize; i++) {
        char16_t u = string[i];
        if (IsLeadSurrogate(u) && (i + 1) < newSize) {
            char16_t next = string[i + 1];
            if (IsTrailSurrogate(next)) {
                i += 1;
                char32_t cp = 0x10000
                    + ((static_cast<char32_t>(u) - 0xD800) << 10)
                    + (static_cast<char32_t>(next) - 0xDC00);
                PushUTF8CodePoint(ret, cp);
            } else {
                PushUTF8CodePoint(ret, u);
            }
        } else {
            PushUTF8CodePoint(ret, u);
        }
    }

    return ret;
}

bool GeneralUtils::CaseInsensitiveStringCompare(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end (), b.begin(), b.end(),[](char a, char b) { return tolower(a) == tolower(b); });
}

// MARK: Bits

//! Sets a specific bit in a signed 64-bit integer
int64_t GeneralUtils::SetBit(int64_t value, uint32_t index) {
    return value |= 1ULL << index;
}

//! Clears a specific bit in a signed 64-bit integer
int64_t GeneralUtils::ClearBit(int64_t value, uint32_t index) {
    return value &= ~(1ULL << index);
}

//! Checks a specific bit in a signed 64-bit integer
bool GeneralUtils::CheckBit(int64_t value, uint32_t index) {
    return value & (1ULL << index);
}

bool GeneralUtils::ReplaceInString(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

std::vector<std::wstring> GeneralUtils::SplitString(std::wstring& str, wchar_t delimiter)
{
    std::vector<std::wstring> vector = std::vector<std::wstring>();
    std::wstring current;

    for (const auto& c : str) {
        if (c == delimiter) {
            vector.push_back(current);
            current = L"";
        } else {
            current += c;
        }
    }

    vector.push_back(current);
    return vector;
}

std::vector<std::u16string> GeneralUtils::SplitString(std::u16string& str, char16_t delimiter)
{
    std::vector<std::u16string> vector = std::vector<std::u16string>();
    std::u16string current;

    for (const auto& c : str) {
        if (c == delimiter) {
            vector.push_back(current);
            current = u"";
        } else {
            current += c;
        }
    }

    vector.push_back(current);
    return vector;
}

std::vector<std::string> GeneralUtils::SplitString(const std::string& str, char delimiter)
{
	std::vector<std::string> vector = std::vector<std::string>();
	std::string current = "";

	for (size_t i = 0; i < str.length(); i++)
	{
		char c = str[i];

		if (c == delimiter)
		{
			vector.push_back(current);
			current = "";
		}
		else
		{
			current += c;
		}
	}

	vector.push_back(current);

	return vector;
}

std::u16string GeneralUtils::ReadWString(RakNet::BitStream *inStream) {
    uint32_t length;
    inStream->Read<uint32_t>(length);

    std::u16string string;
    for (auto i = 0; i < length; i++) {
        uint16_t c;
        inStream->Read(c);
        string.push_back(c);
    }

    return string;
}
