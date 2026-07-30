#include "lang.h"

LangSys &LangSys::I()
{
    static LangSys s;
    return s;
}

void LangSys::setLang(Lang lang) { m_lang = lang; }
LangSys::LangSys() {}

const char *LangSys::get(const std::string &key) const
{
    bool cn = (m_lang == Lang::Chinese);

    // Menu
    if (key == "menu.file")
        return cn ? "\xE6\x96\x87\xE4\xBB\xB6" : "File";
    if (key == "menu.exit")
        return cn ? "\xE9\x80\x80\xE5\x87\xBA" : "Exit";
    if (key == "menu.tools")
        return cn ? "\xE5\xB7\xA5\xE5\x85\xB7" : "Tools";
    if (key == "menu.demo")
        return cn ? "ImGui Demo" : "ImGui Demo";
    if (key == "menu.refresh")
        return cn ? "\xE5\x88\xB7\xE6\x96\xB0" : "Refresh";
    if (key == "menu.lang")
        return cn ? "\xE8\xAF\xAD\xE8\xA8\x80" : "Language";
    if (key == "menu.running")
        return cn ? "\xE8\xBF\x90\xE8\xA1\x8C\xE4\xB8\xAD..." : "* running...";
    if (key == "menu.stop")
        return cn ? "\xE5\x81\x9C\xE6\xAD\xA2" : "stop";
    if (key == "menu.idle")
        return cn ? "\xE7\xA9\xBA\xE9\x97\xB2" : "idle";

    // Tabs
    if (key == "tab.git")
        return cn ? "Git" : "Git";
    if (key == "tab.env")
        return cn ? "\xE7\x8E\xAF\xE5\xA2\x83" : "Environment";
    if (key == "tab.server")
        return cn ? "\xE6\x9C\x8D\xE5\x8A\xA1\xE7\xAB\xAF" : "Server";
    if (key == "tab.cli")
        return cn ? "CLI" : "CLI";

    // Console
    if (key == "console.title")
        return cn ? "\xE6\x8E\xA7\xE5\x88\xB6\xE5\x8F\xB0" : "Console";
    if (key == "console.clear")
        return cn ? "\xE6\xB8\x85\xE7\xA9\xBA" : "clear";
    if (key == "console.hint")
        return cn ? "| \xE6\x8B\x96\xE9\x80\x89\xE5\xA4\x8D\xE5\x88\xB6 | \xE4\xB8\x8A\xE4\xB8\x8B\xE9\x94\xAE\xE5\x8E\x86\xE5\x8F\xB2" : "| select+copy | up/down history";

    // Status
    if (key == "status.python_ok")
        return cn ? "[OK] Python" : "[OK] Python";
    if (key == "status.python_no")
        return cn ? "[NO] Python" : "[NO] Python";
    if (key == "status.project")
        return cn ? "\xE9\xA1\xB9\xE7\x9B\xAE:" : "Project:";

    // Git
    if (key == "git.title")
        return cn ? "\xE4\xBB\x8E GitHub \xE4\xB8\x8B\xE8\xBD\xBD 37AC" : "Download 37AC from GitHub";
    if (key == "git.url")
        return cn ? "Git URL:" : "Git URL:";
    if (key == "git.dir")
        return cn ? "\xE7\x9B\xAE\xE6\xA0\x87\xE7\x9B\xAE\xE5\xBD\x95:" : "Target dir:";
    if (key == "git.clone")
        return cn ? "\xE5\x85\x8B\xE9\x9A\x86 37AC" : "Clone 37AC";
    if (key == "git.check")
        return cn ? "\xE6\xA3\x80\xE6\x9F\xA5\xE7\x9B\xAE\xE5\xBD\x95" : "Check dir";
    if (key == "git.cloning")
        return cn ? "\xE6\xAD\xA3\xE5\x9C\xA8\xE5\x85\x8B\xE9\x9A\x86..." : "cloning...";
    if (key == "git.browse")
        return cn ? "\xE6\xB5\x8F\xE8\xA7\x88..." : "Browse...";
    if (key == "git.fill")
        return cn ? "\xE8\xAF\xB7\xE5\xA1\xAB\xE5\x86\x99 URL \xE5\x92\x8C\xE7\x9B\xAE\xE5\xBD\x95" : "fill in URL and dir";
    if (key == "git.exists")
        return cn ? "\xE7\x9B\xAE\xE5\xBD\x95\xE5\xB7\xB2\xE5\xAD\x98\xE5\x9C\xA8" : "dir exists";
    if (key == "git.none")
        return cn ? "\xE7\x9B\xAE\xE5\xBD\x95\xE4\xB8\x8D\xE5\xAD\x98\xE5\x9C\xA8" : "dir not found";

    // Env
    if (key == "env.python")
        return cn ? "Python" : "Python";
    if (key == "env.ready")
        return cn ? "[OK] Python ready" : "[OK] Python ready";
    if (key == "env.recheck")
        return cn ? "\xE9\x87\x8D\xE6\x96\xB0\xE6\xA3\x80\xE6\xB5\x8B" : "recheck";
    if (key == "env.version")
        return cn ? "\xE7\x89\x88\xE6\x9C\xAC:" : "version:";
    if (key == "env.nofound")
        return cn ? "[NO] Python \xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0" : "[NO] Python not found";
    if (key == "env.detect")
        return cn ? "\xE6\xA3\x80\xE6\xB5\x8B" : "detect";
    if (key == "env.paths")
        return cn ? "\xE8\xB7\xAF\xE5\xBE\x84" : "Paths";
    if (key == "env.root")
        return cn ? "\xE6\xA0\xB9\xE7\x9B\xAE\xE5\xBD\x95:" : "root:";
    if (key == "env.cli")
        return cn ? "CLI:" : "CLI:";
    if (key == "env.srv")
        return cn ? "\xE6\x9C\x8D\xE5\x8A\xA1\xE7\xAB\xAF:" : "server:";
    if (key == "env.actions")
        return cn ? "\xE5\xBF\xAB\xE6\x8D\xB7\xE6\x93\x8D\xE4\xBD\x9C" : "Quick actions";
    if (key == "env.install_cli")
        return cn ? "\xE5\xAE\x89\xE8\xA3\x85 CLI \xE4\xBE\x9D\xE8\xB5\x96" : "Install CLI deps";
    if (key == "env.install_srv")
        return cn ? "\xE5\xAE\x89\xE8\xA3\x85\xE6\x9C\x8D\xE5\x8A\xA1\xE7\xAB\xAF\xE4\xBE\x9D\xE8\xB5\x96" : "Install server deps";
    if (key == "env.verify")
        return cn ? "\xE9\xAA\x8C\xE8\xAF\x81\xE7\x8E\xAF\xE5\xA2\x83" : "Verify env";
    if (key == "env.venv")
        return cn ? "\xE5\x88\x9B\xE5\xBB\xBA\xE8\x99\x9A\xE6\x8B\x9F\xE7\x8E\xAF\xE5\xA2\x83" : "Create venv";
    if (key == "env.venv_ok")
        return cn ? "[venv] \xE5\x88\x9B\xE5\xBB\xBA\xE6\x88\x90\xE5\x8A\x9F" : "[venv] created";
    if (key == "env.venv_fail")
        return cn ? "[venv] \xE5\x88\x9B\xE5\xBB\xBA\xE5\xA4\xB1\xE8\xB4\xA5" : "[venv] failed";

    // Server
    if (key == "srv.title")
        return cn ? "\xE6\x9C\x8D\xE5\x8A\xA1\xE7\xAB\xAF\xE7\x8A\xB6\xE6\x80\x81" : "Server status";
    if (key == "srv.on")
        return cn ? "[ON] \xE8\xBF\x90\xE8\xA1\x8C\xE4\xB8\xAD" : "[ON] server running";
    if (key == "srv.off")
        return cn ? "[OFF] \xE6\x9C\xAA\xE8\xBF\x90\xE8\xA1\x8C" : "[OFF] not running";
    if (key == "srv.unknown")
        return cn ? "[?] \xE6\x9C\xAA\xE6\xA3\x80\xE6\xB5\x8B" : "[?] not checked";
    if (key == "srv.start")
        return cn ? "\xE5\x90\xAF\xE5\x8A\xA8\xE6\x9C\x8D\xE5\x8A\xA1\xE7\xAB\xAF" : "Start server";
    if (key == "srv.check")
        return cn ? "\xE6\xA3\x80\xE6\x9F\xA5\xE7\x8A\xB6\xE6\x80\x81" : "Check status";
    if (key == "srv.online")
        return cn ? "[server] \xE5\x9C\xA8\xE7\xBA\xBF" : "[server] online";
    if (key == "srv.offline")
        return cn ? "[server] \xE7\xA6\xBB\xE7\xBA\xBF" : "[server] offline";
    if (key == "srv.links")
        return cn ? "\xE5\xBF\xAB\xE6\x8D\xB7\xE9\x93\xBE\xE6\x8E\xA5" : "Links";
    if (key == "srv.web")
        return cn ? "\xE6\x89\x93\xE5\xBC\x80 Web \xE5\x89\x8D\xE7\xAB\xAF" : "Web frontend";
    if (key == "srv.api")
        return cn ? "API \xE6\xB5\x8B\xE8\xAF\x95" : "API test";

    // CLI
    if (key == "cli.train")
        return cn ? "\xE6\xA8\xA1\xE5\x9E\x8B\xE8\xAE\xAD\xE7\xBB\x83" : "Training";
    if (key == "cli.btn_train")
        return cn ? "\xE5\xBC\x80\xE5\xA7\x8B\xE8\xAE\xAD\xE7\xBB\x83" : "Train";
    if (key == "cli.btn_resume")
        return cn ? "\xE7\xBB\xA7\xE7\xBB\xAD\xE8\xAE\xAD\xE7\xBB\x83" : "Resume";
    if (key == "cli.btn_predict")
        return cn ? "\xE9\xA2\x84\xE6\xB5\x8B\xE8\xA7\x92\xE8\x89\xB2" : "Predict";
    if (key == "cli.node")
        return cn ? "\xE8\x8A\x82\xE7\x82\xB9\xE4\xB8\x8E\xE5\xB7\xA5\xE5\x85\xB7" : "Node & Tools";
    if (key == "cli.btn_node")
        return cn ? "\xE5\x90\xAF\xE5\x8A\xA8\xE8\x8A\x82\xE7\x82\xB9" : "Node";
    if (key == "cli.btn_verify")
        return cn ? "\xE9\xAA\x8C\xE8\xAF\x81\xE5\x9B\xBE\xE5\x83\x8F" : "Verify";
    if (key == "cli.btn_menu")
        return cn ? "\xE4\xBA\xA4\xE4\xBA\x92\xE8\x8F\x9C\xE5\x8D\x95" : "Menu";

    // Log tags
    if (key == "log.start")
        return cn ? "[start] " : "[start] ";
    if (key == "log.done")
        return cn ? "[done] " : "[done] ";
    if (key == "log.error")
        return cn ? "[error] " : "[error] ";
    if (key == "log.warn")
        return cn ? "[warn] " : "[warn] ";
    if (key == "log.git")
        return cn ? "[Git] " : "[Git] ";

    // General
    if (key == "app.title")
        return cn ? "37ACUI" : "37ACUI";
    if (key == "app.started")
        return cn ? "[OK] 37ACUI started" : "[OK] 37ACUI started";
    if (key == "app.path")
        return cn ? "project path" : "project path";
    if (key == "app.python")
        return cn ? "Python" : "Python";
    if (key == "app.notfound")
        return cn ? "not found" : "not found";

    return key.c_str();
}