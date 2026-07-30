#pragma once

#include <string>
#include <atomic>
#include <thread>
#include "network/http_client.h"
#include "gui/console.h"
#include "gui/lang.h"

class App
{
public:
    App();
    ~App();

    bool init(const char *title, int width, int height);
    void run();

    HttpClient &http() { return m_http; }
    const std::string &projectRoot() const { return m_projectRoot; }
    const std::string &cliRoot() const { return m_cliRoot; }
    const std::string &serverRoot() const { return m_serverRoot; }

    void addLog(const std::string &msg, unsigned int = 0xFFFFFFFF);
    void addInfo(const std::string &msg) { addLog(msg); }
    void addSuccess(const std::string &msg) { addLog(msg); }
    void addError(const std::string &msg) { addLog(msg); }
    void addWarn(const std::string &msg) { addLog(msg); }

    bool runPython(const std::string &cmd, const std::string &cwd,
                   std::string &output, int &exitCode);
    bool checkPython();

    void startProcess(const std::string &name, const std::string &cmd,
                      const std::string &cwd);
    void stopProcess();
    bool isProcRunning() const { return m_procRunning; }

    void startGitClone(const std::string &repoUrl, const std::string &targetDir);

private:
    void render();
    void renderMenuBar();
    void renderTabBar();
    void renderStatusBar();
    void renderLogArea();
    void renderEnvPanel();
    void renderServerPanel();
    void renderCliPanel();
    void renderGitPanel();

    void procThreadFunc(const std::string &name, const std::string &cmd,
                        const std::string &cwd);

    void saveConfig();
    void loadConfig();

    HttpClient m_http;
    std::string m_projectRoot, m_cliRoot, m_serverRoot;
    int m_activeTab = 0;
    bool m_showDemo = false, m_pythonOk = false;
    std::string m_pythonVersion;
    std::atomic<bool> m_procRunning{false};
    std::thread m_procThread;
    ConsoleWidget m_console;
    void *m_procStdinWrite = nullptr;
    char m_gitUrl[256] = "https://github.com/chijichan/37AC.git";
    char m_gitDir[256] = "E:/apps/37ACUI";
    bool m_gitCloning = false;
    int m_winX = 100, m_winY = 50, m_winW = 1280, m_winH = 800;
    int m_serverStatus = -1;
};