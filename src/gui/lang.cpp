#include "lang.h"

LangSys &LangSys::I()
{
    static LangSys s;
    return s;
}

void LangSys::setLang(Lang lang) { m_lang = lang; }

LangSys::LangSys()
{
    // key -> {中文, English}
    // Menu
    m_data["menu.file"] = {"文件", "File"};
    m_data["menu.exit"] = {"退出", "Exit"};
    m_data["menu.tools"] = {"工具", "Tools"};
    m_data["menu.demo"] = {"ImGui Demo", "ImGui Demo"};
    m_data["menu.refresh"] = {"刷新", "Refresh"};
    m_data["menu.lang"] = {"语言", "Language"};
    m_data["menu.running"] = {"运行中...", "* running..."};
    m_data["menu.stop"] = {"停止", "stop"};
    m_data["menu.idle"] = {"空闲", "idle"};

    // Tabs
    m_data["tab.git"] = {"Git", "Git"};
    m_data["tab.env"] = {"环境", "Environment"};
    m_data["tab.server"] = {"服务端", "Server"};
    m_data["tab.cli"] = {"CLI", "CLI"};

    // Console
    m_data["console.title"] = {"控制台", "Console"};
    m_data["console.clear"] = {"清空", "clear"};
    m_data["console.hint"] = {"| 拖选复制 | 上下键历史", "| select+copy | up/down history"};

    // Nav
    m_data["nav.launch"] = {"启动", "Launch"};
    m_data["nav.tools"] = {"工具", "Tools"};
    m_data["nav.settings"] = {"设置", "Settings"};
    m_data["nav.exit"] = {"退出", "Exit"};
    m_data["nav.status"] = {"状态: ", "Status: "};
    m_data["nav.running"] = {"运行中", "RUNNING"};
    m_data["nav.idle"] = {"空闲", "idle"};
    m_data["nav.stop"] = {"停止", "STOP"};

    // Settings
    m_data["settings.language"] = {"语言", "Language"};
    m_data["settings.english"] = {"英语", "English"};
    m_data["settings.chinese"] = {"中文", "Chinese"};
    m_data["settings.program_dir"] = {"程序地址:", "Program dir:"};

    // Status
    m_data["status.python_ok"] = {"[OK] Python", "[OK] Python"};
    m_data["status.python_no"] = {"[NO] Python", "[NO] Python"};
    m_data["status.project"] = {"项目:", "Project:"};

    // Git
    m_data["git.title"] = {"从 GitHub 下载 37AC", "Download 37AC from GitHub"};
    m_data["git.url"] = {"Git URL:", "Git URL:"};
    m_data["git.dir"] = {"项目目录:", "Target dir:"};
    m_data["git.clone"] = {"克隆 37AC", "Clone 37AC"};
    m_data["git.check"] = {"检查目录", "Check dir"};
    m_data["git.cloning"] = {"正在克隆...", "cloning..."};
    m_data["git.browse"] = {"浏览...", "Browse..."};
    m_data["git.fill"] = {"请填写 URL 和目录", "fill in URL and dir"};
    m_data["git.exists"] = {"目录已存在", "dir exists"};
    m_data["git.none"] = {"目录不存在", "dir not found"};

    // Env
    m_data["env.python"] = {"Python", "Python"};
    m_data["env.ready"] = {"[OK] Python ready", "[OK] Python ready"};
    m_data["env.recheck"] = {"重新检测", "recheck"};
    m_data["env.version"] = {"版本:", "version:"};
    m_data["env.nofound"] = {"[NO] Python 未找到", "[NO] Python not found"};
    m_data["env.detect"] = {"检测", "detect"};
    m_data["env.paths"] = {"路径", "Paths"};
    m_data["env.root"] = {"根目录:", "root:"};
    m_data["env.cli"] = {"CLI:", "CLI:"};
    m_data["env.srv"] = {"服务端:", "server:"};
    m_data["env.actions"] = {"快捷操作", "Quick actions"};
    m_data["env.install_cli"] = {"安装 CLI 依赖", "Install CLI deps"};
    m_data["env.install_srv"] = {"安装服务端依赖", "Install server deps"};
    m_data["env.verify"] = {"验证环境", "Verify env"};
    m_data["env.venv"] = {"创建虚拟环境", "Create venv"};
    m_data["env.venv_ok"] = {"[venv] 创建成功", "[venv] created"};
    m_data["env.venv_fail"] = {"[venv] 创建失败", "[venv] failed"};

    // Server
    m_data["srv.title"] = {"服务端状态", "Server status"};
    m_data["srv.on"] = {"[ON] 运行中", "[ON] server running"};
    m_data["srv.off"] = {"[OFF] 未运行", "[OFF] not running"};
    m_data["srv.unknown"] = {"[?] 未检测", "[?] not checked"};
    m_data["srv.start"] = {"启动服务端", "Start server"};
    m_data["srv.check"] = {"检查状态", "Check status"};
    m_data["srv.online"] = {"[server] 在线", "[server] online"};
    m_data["srv.offline"] = {"[server] 离线", "[server] offline"};
    m_data["srv.links"] = {"快捷链接", "Links"};
    m_data["srv.web"] = {"打开 Web 前端", "Web frontend"};
    m_data["srv.api"] = {"API 测试", "API test"};

    // CLI
    m_data["cli.train"] = {"模型训练", "Training"};
    m_data["cli.btn_train"] = {"开始训练", "Train"};
    m_data["cli.btn_resume"] = {"继续训练", "Resume"};
    m_data["cli.btn_predict"] = {"预测角色", "Predict"};
    m_data["cli.node"] = {"节点与工具", "Node & Tools"};
    m_data["cli.btn_node"] = {"启动节点", "Node"};
    m_data["cli.btn_verify"] = {"验证图像", "Verify"};
    m_data["cli.btn_menu"] = {"交互菜单", "Menu"};

    // Log tags
    m_data["log.start"] = {"[start] ", "[start] "};
    m_data["log.done"] = {"[done] ", "[done] "};
    m_data["log.error"] = {"[error] ", "[error] "};
    m_data["log.warn"] = {"[warn] ", "[warn] "};
    m_data["log.git"] = {"[Git] ", "[Git] "};

    // General
    m_data["app.title"] = {"37ACUI", "37ACUI"};
    m_data["app.started"] = {"[OK] 37ACUI started", "[OK] 37ACUI started"};
    m_data["app.path"] = {"project path", "project path"};
    m_data["app.python"] = {"Python", "Python"};
    m_data["app.notfound"] = {"not found", "not found"};
}

const char *LangSys::get(const std::string &key) const
{
    auto it = m_data.find(key);
    if (it == m_data.end())
        return key.c_str();
    return (m_lang == Lang::Chinese) ? it->second.first : it->second.second;
}