#pragma once

#include <string>
#include <unordered_map>

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
    // key -> {中文, 英文}
    std::unordered_map<std::string, std::pair<const char *, const char *>> m_data;
};

#define TR(key) LangSys::I().get(key)