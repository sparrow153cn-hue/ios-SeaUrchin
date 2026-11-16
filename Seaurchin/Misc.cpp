#include "Misc.h"
#include "ScriptScene.h"

using namespace std;

wstring ConvertUTF8ToUnicode(const string &utf8str)
{
    return sea::Utf8ToWide(utf8str);
}

string ConvertUnicodeToUTF8(const wstring &utf16str)
{
    return sea::WideToUtf8(utf16str);
}

void ScriptSceneWarnOutOf(const string &type, asIScriptContext *ctx)
{
    const char *secn;
    int col, row;
    row = ctx->GetLineNumber(0, &col, &secn);
    ctx->GetEngine()->WriteMessage(secn, row, col, asEMsgType::asMSGTYPE_WARNING, ("You can call Yield Function only from " + type + "!").c_str());
}

double ToDouble(const char *str)
{
    double result = 0.0, base = 1.0;
    int sign = 1;
    const char *it = str;
    unsigned char ch = '0';

    it = *it == '-' ? (sign = -sign, ++it) : it;
    while (((unsigned char)(ch = *(it++) - '0')) <= 9) result = result * 10 + ch;
    if (*(--it) == '.') while (((unsigned char)(ch = *(++it) - '0')) <= 9) result += (base *= 0.1) * ch;
    return sign * result;
}

int32_t ConvertInteger(const string &input)
{
    return atoi(input.c_str());
}

uint32_t ConvertHexatridecimal(const string &input)
{
    return stoul(input, 0, 36);
}

double ConvertFloat(const string &input)
{
    return ToDouble(input.c_str());
}

bool ConvertBoolean(const string &input)
{
    auto test = input;
    transform(test.cbegin(), test.cend(), test.begin(), tolower);
    return
        input == "1"
        || input == "true"
        || input == "y"
        || input == "yes"
        || input == "enable"
        || input == "enabled";
}

double lerp(double x, double a, double b)
{
    return (1.0 - x) * a + x * b;
}