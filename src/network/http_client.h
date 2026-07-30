#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>

// 简单的 HTTP 客户端（基于 WinHTTP）
struct HttpResult
{
    long status_code = 0;
    std::string body;
    std::string error;
    bool ok() const { return status_code >= 200 && status_code < 300; }
};

class HttpClient
{
public:
    HttpClient();
    ~HttpClient();

    // 设置基础 URL
    void setBaseUrl(const std::string &url);
    const std::string &baseUrl() const { return m_baseUrl; }

    // 设置认证 Token
    void setToken(const std::string &token);
    const std::string &token() const { return m_token; }

    // 同步请求
    HttpResult get(const std::string &path);
    HttpResult post(const std::string &path, const std::string &jsonBody);
    HttpResult put(const std::string &path, const std::string &jsonBody);
    HttpResult del(const std::string &path);

    // 异步请求
    void getAsync(const std::string &path,
                  std::function<void(const HttpResult &)> callback);
    void postAsync(const std::string &path, const std::string &jsonBody,
                   std::function<void(const HttpResult &)> callback);

    // 检查服务端是否在线
    bool ping();

private:
    HttpResult request(const std::string &method, const std::string &path,
                       const std::string &body = "");

    std::string m_baseUrl;
    std::string m_token;
    std::string m_userAgent;
};