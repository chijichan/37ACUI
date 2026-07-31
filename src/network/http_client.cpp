#include "http_client.h"
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <thread>

#pragma comment(lib, "winhttp.lib")

HttpClient::HttpClient()
    : m_baseUrl("http://127.0.0.1:13138"), m_userAgent("ACUI/1.0")
{
}

HttpClient::~HttpClient() = default;

void HttpClient::setBaseUrl(const std::string &url)
{
    m_baseUrl = url;
}

void HttpClient::setToken(const std::string &token)
{
    m_token = token;
}

// 解析 URL 为主机、端口、路径
static bool parseUrl(const std::string &url, std::string &host, int &port,
                     std::string &path, bool &isHttps)
{
    isHttps = false;
    std::string tmp = url;

    // 去掉协议前缀
    if (tmp.find("https://") == 0)
    {
        isHttps = true;
        tmp = tmp.substr(8);
    }
    else if (tmp.find("http://") == 0)
    {
        tmp = tmp.substr(7);
    }

    // 分离主机和路径
    auto slashPos = tmp.find('/');
    if (slashPos == std::string::npos)
    {
        host = tmp;
        path = "/";
    }
    else
    {
        host = tmp.substr(0, slashPos);
        path = tmp.substr(slashPos);
    }

    // 提取端口
    auto colonPos = host.find(':');
    if (colonPos != std::string::npos)
    {
        port = std::stoi(host.substr(colonPos + 1));
        host = host.substr(0, colonPos);
    }
    else
    {
        port = isHttps ? 443 : 80;
    }
    return true;
}

HttpResult HttpClient::request(const std::string &method, const std::string &path,
                               const std::string &body)
{
    HttpResult result;

    std::string host;
    int port = 80;
    std::string basePath;
    bool isHttps = false;

    if (!parseUrl(m_baseUrl, host, port, basePath, isHttps))
    {
        result.error = "Invalid base URL";
        return result;
    }

    std::string fullPath = basePath;
    if (fullPath.back() == '/')
        fullPath.pop_back();
    fullPath += path;

    HINTERNET hSession = WinHttpOpen(
        L"ACUI/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        nullptr, nullptr, 0);
    if (!hSession)
    {
        result.error = "WinHttpOpen failed";
        return result;
    }

    std::wstring whost(host.begin(), host.end());
    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect)
    {
        result.error = "WinHttpConnect failed";
        WinHttpCloseHandle(hSession);
        return result;
    }

    std::wstring wpath(fullPath.begin(), fullPath.end());
    std::wstring wmethod(method.begin(), method.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(), wpath.c_str(),
                                            nullptr, nullptr, nullptr, flags);
    if (!hRequest)
    {
        result.error = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // 设置超时
    WinHttpSetTimeouts(hRequest, 5000, 5000, 10000, 10000);

    // 设置 Headers
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!m_token.empty())
    {
        std::wstring wtoken(m_token.begin(), m_token.end());
        headers += L"Authorization: Bearer " + wtoken + L"\r\n";
    }
    headers += L"Accept: application/json\r\n";

    // 发送请求
    LPCWSTR pwHeaders = headers.c_str();
    LPVOID lpBody = body.empty() ? nullptr : (void *)body.c_str();
    DWORD bodyLen = body.empty() ? 0 : (DWORD)body.size();

    if (!WinHttpSendRequest(hRequest, pwHeaders, (DWORD)headers.size(),
                            lpBody, bodyLen, bodyLen, 0))
    {
        result.error = "WinHttpSendRequest failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr))
    {
        result.error = "WinHttpReceiveResponse failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // 获取状态码
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        nullptr, &statusCode, &statusSize, nullptr);
    result.status_code = (long)statusCode;

    // 读取响应体
    DWORD bytesRead = 0;
    std::string responseBody;
    char buffer[4096];
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead))
    {
        if (bytesRead == 0)
            break;
        responseBody.append(buffer, bytesRead);
    }
    result.body = responseBody;

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

HttpResult HttpClient::get(const std::string &path)
{
    return request("GET", path);
}

HttpResult HttpClient::post(const std::string &path, const std::string &jsonBody)
{
    return request("POST", path, jsonBody);
}

HttpResult HttpClient::put(const std::string &path, const std::string &jsonBody)
{
    return request("PUT", path, jsonBody);
}

HttpResult HttpClient::del(const std::string &path)
{
    return request("DELETE", path);
}

void HttpClient::getAsync(const std::string &path,
                          std::function<void(const HttpResult &)> callback)
{
    std::thread([this, path, callback]()
                {
        auto result = get(path);
        callback(result); })
        .detach();
}

void HttpClient::postAsync(const std::string &path, const std::string &jsonBody,
                           std::function<void(const HttpResult &)> callback)
{
    std::thread([this, path, jsonBody, callback]()
                {
        auto result = post(path, jsonBody);
        callback(result); })
        .detach();
}

bool HttpClient::ping()
{
    auto result = get("/dashboard/overview");
    return result.ok();
}