#pragma once

#include <string>
#include <map>

enum class Lang
{
    Chinese = 0,
    English = 1
};

class LangSys
{
public:
    static LangSys &I();
    void setLang(Lang lang);
    Lang lang() const { return m_lang; }
    const char *get(const std::string &key) const;

private:
    LangSys();
    Lang m_lang = Lang::English;
    std::map<std::string, std::string[2]> m_data;
};

#define TR(key) LangSys::I().get(key)