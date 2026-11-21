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
    auto add = [this](const char *k, const char *zh, const char *en)
    { m_data[k] = {zh, en}; };

    // ---------------------------------------------------------------- 应用
    add("app.title", "37ACUI", "37ACUI");
    add("app.subtitle", "37AC 控制器", "37AC Controller");
    add("app.version", "版本", "version");

    // ---------------------------------------------------------------- 导航
    add("nav.workspace", "工作台", "WORKSPACE");
    add("nav.system", "系统", "SYSTEM");
    add("nav.overview", "概览", "Overview");
    add("nav.model", "模型", "Models");
    add("nav.dataset", "数据集", "Dataset");
    add("nav.node", "节点", "Node");
    add("nav.server", "服务端", "Server");
    add("nav.env", "环境", "Environment");
    add("nav.settings", "设置", "Settings");
    add("nav.exit", "退出", "Exit");

    // ---------------------------------------------------------------- 通用
    add("common.refresh", "刷新", "Refresh");
    add("common.open", "打开", "Open");
    add("common.open_dir", "打开目录", "Open folder");
    add("common.browse", "浏览…", "Browse…");
    add("common.cancel", "取消", "Cancel");
    add("common.confirm", "确定", "Confirm");
    add("common.delete", "删除", "Delete");
    add("common.start", "启动", "Start");
    add("common.stop", "停止", "Stop");
    add("common.close", "关闭", "Close");
    add("common.recheck", "重新检测", "Recheck");
    add("common.none", "—", "—");
    add("common.yes", "是", "Yes");
    add("common.no", "否", "No");
    add("common.ready", "就绪", "Ready");
    add("common.missing", "缺失", "Missing");
    add("common.exists", "存在", "Exists");
    add("common.online", "在线", "Online");
    add("common.offline", "离线", "Offline");
    add("common.running", "运行中", "Running");
    add("common.idle", "空闲", "Idle");
    add("common.loading", "读取中…", "Loading…");
    add("common.not_installed", "尚未安装 37AC", "37AC is not installed");
    add("common.not_installed_desc", "在下方下载源码并完成初始化后，即可使用全部功能。",
        "Download the source and finish setup to unlock everything.");
    add("common.hint", "提示", "Hint");
    add("git.clone", "下载 37AC", "Download 37AC");
    add("git.exists", "目录已存在", "directory already exists");

    // ---------------------------------------------------------------- 托盘
    add("tray.tip", "37ACUI · 37AC 控制器", "37ACUI · 37AC Controller");
    add("tray.show", "显示主界面", "Show window");
    add("tray.quit", "退出", "Exit");
    add("tray.balloon", "已隐藏到系统托盘，双击图标可恢复界面。",
        "Hidden to the system tray. Double-click the icon to restore.");
    add("tray.log_enter", "已隐藏到系统托盘，界面资源已释放", "hidden to tray, UI resources released");
    add("tray.log_restore", "已从托盘恢复界面", "restored from tray");

    // ---------------------------------------------------------------- 概览
    add("overview.title", "概览", "Overview");
    add("overview.subtitle", "训练、数据集、节点与服务端的一站式控制台",
        "One place for training, datasets, nodes and the API server");
    add("overview.hero_badge", "本地 · 离线可用", "Local · offline first");
    add("overview.hero_title", "欢迎回到 37AC 控制器", "Welcome back to 37AC");
    add("overview.hero_desc", "管理模型训练、数据集处理、识别节点与 Flask 服务端；所有操作都会记录到下方控制台。",
        "Manage training, datasets, recognition nodes and the Flask server. Every action is streamed to the console below.");
    add("overview.quick", "快速开始", "Quick start");
    add("overview.quick_desc", "常用动作一键直达", "Shortcuts for the everyday actions");
    add("overview.runtime", "运行状态", "Runtime");
    add("overview.runtime_desc", "进程与端口实时状态", "Processes and ports at a glance");
    add("overview.recent", "最近日志", "Recent output");
    add("overview.recent_desc", "控制台最新 6 行输出", "Latest 6 lines from the console");
    add("overview.open_console", "打开控制台", "Open console");
    add("overview.collapse_console", "收起控制台", "Hide console");
    add("overview.stat_python", "Python", "Python");
    add("overview.stat_model", "模型版本", "Model version");
    add("overview.stat_classes", "类别数", "Classes");
    add("overview.stat_checkpoints", "检查点", "Checkpoints");
    add("overview.stat_none", "未就绪", "Not ready");
    add("overview.stat_project", "项目目录", "Project");
    add("overview.btn_init", "一键初始化", "Bootstrap");
    add("overview.btn_download", "下载源码", "Download source");
    add("overview.btn_web", "打开 Web", "Open web UI");
    add("overview.btn_train", "开始训练", "Train");
    add("overview.btn_dataset", "数据集", "Dataset");
    add("overview.btn_server", "服务端", "Server");
    add("overview.btn_node", "节点", "Node");
    add("overview.proc", "当前进程", "Current process");
    add("overview.proc_none", "无", "None");
    add("overview.ports", "端口", "Ports");
    add("overview.hint_init", "初始化会下载源码、创建虚拟环境、安装依赖并生成 .env",
        "Bootstrap downloads the source, creates a venv, installs deps and writes .env");

    // ---------------------------------------------------------------- 模型
    add("model.title", "模型", "Models");
    add("model.subtitle", "当前模型、训练、检查点与训练基模", "Current model, training, checkpoints and base weights");
    add("model.current", "当前模型", "Current model");
    add("model.current_desc", "读取 saves/models 下的权重与配置", "Reads weights and config from saves/models");
    add("model.version", "版本", "Version");
    add("model.trained_at", "训练时间", "Trained at");
    add("model.classes", "类别数", "Classes");
    add("model.size", "权重大小", "Size");
    add("model.base", "来源基模", "Base model");
    add("model.best_acc", "最佳验证准确率", "Best val accuracy");
    add("model.checkpoints", "检查点", "Checkpoints");
    add("model.dataset", "训练数据集", "Dataset");
    add("model.image_size", "输入尺寸", "Input size");
    add("model.sha", "权重 SHA-256", "Weight SHA-256");
    add("model.no_model", "暂无模型", "No model yet");
    add("model.no_model_desc", "完成一次训练后，这里会显示版本、准确率与检查点。",
        "After the first training run, version, accuracy and checkpoints show up here.");
    add("model.train", "训练", "Training");
    add("model.train_desc", "选择数据集后开始训练", "Pick a dataset and start training");
    add("model.dataset_choice", "训练数据集", "Dataset");
    add("model.dataset_raw", "原始数据集", "Raw dataset");
    add("model.dataset_cropped", "已裁剪数据集", "Cropped dataset");
    add("model.dataset_yolo", "训练前 YOLO 裁剪", "YOLO crop before training");
    add("model.btn_train", "从头训练", "Train from scratch");
    add("model.btn_resume", "继续训练", "Resume training");
    add("model.train_hint", "训练输出会实时进入控制台；可用控制台的「停止」中断。",
        "Training output streams to the console; use its Stop button to interrupt.");
    add("model.ckpt", "检查点", "Checkpoints");
    add("model.ckpt_desc", "回滚、删除或清理历史权重", "Roll back, delete or prune snapshots");
    add("model.ckpt_empty", "暂无检查点", "No checkpoints");
    add("model.ckpt_empty_desc", "首次进入模型管理或训练后会生成检查点。",
        "Checkpoints are created when model tools are opened or after training.");
    add("model.ckpt_restore", "回滚", "Restore");
    add("model.ckpt_delete", "删除", "Delete");
    add("model.ckpt_prune", "清理", "Prune");
    add("model.ckpt_accuracy", "准确率", "Accuracy");
    add("model.ckpt_kind", "类型", "Kind");
    add("model.ckpt_missing", "目录缺失", "Missing files");
    add("model.ckpt_restore_title", "回滚检查点", "Restore checkpoint");
    add("model.ckpt_restore_body", "将当前模型替换为所选检查点的权重（回滚前会自动备份当前状态）。",
        "Replaces the current weights with this snapshot (the current state is backed up first).");
    add("model.ckpt_delete_title", "删除检查点", "Delete checkpoint");
    add("model.ckpt_delete_body", "该检查点目录会被永久删除，操作不可撤销。",
        "The snapshot folder is removed permanently. This cannot be undone.");
    add("model.ckpt_prune_title", "清理检查点", "Prune checkpoints");
    add("model.ckpt_prune_body", "保留最近 10 个检查点（总量上限 2 GB），其余会被删除。",
        "Keeps the latest 10 snapshots (max 2 GB total) and removes the rest.");
    add("model.base_title", "训练基模", "Base weights");
    add("model.base_desc", "切换只写入 .env，对下一次训练生效", "Switching writes .env and applies to the next run");
    add("model.base_current", "当前基模", "Current base");
    add("model.base_builtin", "内置（零下载）", "Built-in (no download)");
    add("model.base_hf", "HF 基模", "From HF Hub");
    add("model.base_download", "从 HF 下载", "Download from HF");
    add("model.base_login", "HF 登录", "Sign in to HF");
    add("model.base_download_title", "下载训练基模", "Download base weights");
    add("model.base_download_label", "HF 仓库名（如 timm/efficientnet_b3.ra2_in1k）",
        "HF repo id (e.g. timm/efficientnet_b3.ra2_in1k)");
    add("model.hf_login_title", "登录 Hugging Face", "Sign in to Hugging Face");
    add("model.hf_login_label", "Access Token（仅本次运行内存保存）", "Access token (kept in memory only)");
    add("model.publish", "发布到 HF Hub", "Publish to HF Hub");
    add("model.publish_desc", "上传当前权重、classes.json 与 config.json（私有仓库）",
        "Upload current weights, classes.json and config.json to a private repo");
    add("model.publish_repo", "Repo ID", "Repo ID");
    add("model.publish_hint", "需要先在「训练基模」中完成 HF 登录，或粘贴 Token。",
        "Sign in to HF first, or paste a token below.");
    add("model.publish_token", "Token（可选）", "Token (optional)");
    add("model.btn_publish", "发布", "Publish");
    add("model.need_python", "需要可用的 Python 环境才能读取模型信息。", "A working Python is required to read model info.");

    // ---------------------------------------------------------------- 数据集
    add("dataset.title", "数据集", "Dataset");
    add("dataset.subtitle", "图像校验、YOLO 裁剪、压缩与格式统一", "Validate, crop, compress and normalize images");
    add("dataset.paths", "数据集目录", "Dataset folders");
    add("dataset.paths_desc", "路径来自 src/cli/.env 与 config.base", "Paths come from src/cli/.env and config.base");
    add("dataset.raw", "原始数据集", "Raw dataset");
    add("dataset.cropped", "已裁剪数据集", "Cropped dataset");
    add("dataset.ops", "数据处理", "Processing");
    add("dataset.ops_desc", "长任务会输出到控制台，可随时停止", "Long jobs stream to the console and can be stopped");
    add("dataset.verify", "验证图像", "Validate images");
    add("dataset.verify_desc", "检查 IP/角色 目录结构与图片有效性", "Check IP/role layout and image validity");
    add("dataset.crop", "YOLO 裁剪", "YOLO crop");
    add("dataset.crop_desc", "裁剪原始数据集并写入 saves/dataset（含质量门控）",
        "Crop the raw dataset into saves/dataset with quality gating");
    add("dataset.compress", "压缩数据集", "Compress dataset");
    add("dataset.compress_desc", "把已裁剪图片的最长边压缩到 DATASET_COMPRESS_SIZE",
        "Resize cropped images to DATASET_COMPRESS_SIZE on the long edge");
    add("dataset.jpeg", "转 JPEG", "Convert to JPEG");
    add("dataset.jpeg_desc", "统一为 JPEG，训练读图更快", "Normalize to JPEG for faster training IO");
    add("dataset.crop_title", "开始 YOLO 裁剪", "Start YOLO cropping");
    add("dataset.crop_body", "将重新扫描原始数据集并写入 saves/dataset，已有产物会按增量跳过。",
        "The raw dataset is scanned and written into saves/dataset; existing outputs are skipped.");
    add("dataset.compress_title", "压缩已裁剪数据集", "Compress cropped dataset");
    add("dataset.compress_body", "会就地压缩 saves/dataset 内的图片，可重复执行。",
        "Compresses images in saves/dataset in place; safe to run again.");
    add("dataset.jpeg_title", "转换为 JPEG", "Convert to JPEG");
    add("dataset.jpeg_body", "非 JPEG 图片会被转换并替换，建议在训练前执行一次。",
        "Non-JPEG images are converted in place. Recommended once before training.");
    add("dataset.pipeline", "处理流程", "Pipeline");
    add("dataset.pipeline_desc", "原始图片 → YOLO 裁剪（质量门控）→ 压缩/转 JPEG → 训练。训练可在「模型」页一键开始。",
        "Raw images → YOLO crop (with gating) → compress/JPEG → training. Start training from the Models page.");
    add("dataset.missing_raw", "原始数据集目录不存在，请先在 .env 配置 DATASET_DIR。",
        "Raw dataset folder not found. Set DATASET_DIR in .env first.");
    add("dataset.missing_cropped", "已裁剪数据集目录不存在，先执行 YOLO 裁剪。",
        "Cropped dataset folder not found. Run YOLO crop first.");

    // ---------------------------------------------------------------- 节点
    add("node.title", "节点", "Node");
    add("node.subtitle", "分布式识别节点与连通性检查", "Distributed recognition node and connectivity");
    add("node.service", "节点服务", "Node service");
    add("node.service_desc", "向服务端注册并接收识别任务", "Registers with the server and receives tasks");
    add("node.start", "启动节点", "Start node");
    add("node.stop", "停止节点", "Stop node");
    add("node.no_update", "本次不自动更新模型", "Skip automatic model update");
    add("node.config", "节点配置", "Node config");
    add("node.config_desc", "来自 src/cli/.env", "From src/cli/.env");
    add("node.id", "节点 ID", "Node ID");
    add("node.tcp", "TCP 监听", "TCP listen");
    add("node.server", "服务端地址", "Server URL");
    add("node.token", "节点 Token", "Node token");
    add("node.token_set", "已配置", "Configured");
    add("node.token_missing", "未配置", "Not configured");
    add("node.max_tasks", "最大并发任务", "Max concurrent tasks");
    add("node.heartbeat", "心跳间隔", "Heartbeat");
    add("node.connectivity", "连通性", "Connectivity");
    add("node.connectivity_desc", "本机端口探测（4 秒刷新）", "Local port probe (refreshes every 4s)");
    add("node.web_port", "Flask Web 端口", "Flask web port");
    add("node.tcp_port", "节点 TCP 端口", "Node TCP port");
    add("node.hint", "节点需要先在 Web 后台注册并取得 Token；离线时会按 RECONNECT_DELAY_SEC 自动重连。",
        "Register the node in the web dashboard to get a token. Offline nodes reconnect automatically.");
    add("node.not_configured_hint", "尚未配置 TOKEN，节点可能无法通过服务端鉴权。",
        "TOKEN is empty; the node may fail server authentication.");

    // ---------------------------------------------------------------- 服务端
    add("server.title", "服务端", "Server");
    add("server.subtitle", "Flask API、TCP 节点管理与 Web 前端入口", "Flask API, node TCP server and web entry points");
    add("server.control", "服务进程", "Service process");
    add("server.control_desc", "runserver.py：Flask + TCP 节点管理", "runserver.py: Flask + node TCP listener");
    add("server.start", "启动服务端", "Start server");
    add("server.stop", "停止服务端", "Stop server");
    add("server.ports", "端口", "Ports");
    add("server.web_port", "Flask Web", "Flask web");
    add("server.tcp_port", "节点 TCP", "Node TCP");
    add("server.links", "快捷入口", "Quick links");
    add("server.links_desc", "在浏览器中打开", "Open in the browser");
    add("server.link_site", "Web 站点", "Web site");
    add("server.link_upload", "上传识别页", "Upload & recognize");
    add("server.link_dashboard", "用户控制台", "Dashboard");
    add("server.link_api", "上传 API", "Upload API");
    add("server.hint", "PHP 前端需单独启动：在 src/www 运行 start-nginx.bat（默认 8000 端口）。",
        "The PHP frontend runs separately: start-nginx.bat in src/www (port 8000).");
    add("server.offline_hint", "服务端未运行，端口显示为离线。", "Server is not running; ports show offline.");
    add("server.db", "数据库", "Database");
    add("server.db_host", "DB 主机", "DB host");

    // ---------------------------------------------------------------- 环境
    add("env.title", "环境", "Environment");
    add("env.subtitle", "Python 环境、依赖与 .env 配置", "Python runtime, dependencies and .env files");
    add("env.python", "Python 状态", "Python");
    add("env.recheck_ok", "Python 环境正常", "Python environment OK");
    add("env.recheck_fail", "未检测到可用的 Python", "No working Python found");
    add("env.python_desc", "系统解释器与 .venv", "Interpreter and .venv");
    add("env.ready", "Python 就绪", "Python ready");
    add("env.nofound", "Python 未找到", "Python not found");
    add("env.version", "版本", "version");
    add("env.paths", "路径", "Paths");
    add("env.paths_desc", "程序推导的项目结构", "Project layout derived from the program folder");
    add("env.root", "根目录", "Root");
    add("env.cli", "CLI", "CLI");
    add("env.srv", "服务端", "Server");
    add("env.venv", "创建虚拟环境", "Create venv");
    add("env.venv_del", "删除并重建", "Delete & recreate");
    add("env.venv_ok", "[venv] 创建成功", "[venv] created");
    add("env.venv_fail", "[venv] 创建失败", "[venv] failed");
    add("env.venv_del_ok", "[venv] 旧环境已删除，正在重建…", "[venv] old env removed, recreating…");
    add("env.venv_exist", "[venv] .venv 已存在，重建前请先停止运行中的进程", "[venv] .venv exists; stop running processes first");
    add("env.venv_recreate_body", "将删除现有 .venv 并重新创建；完成后需要重新安装依赖。",
        "Deletes the existing .venv and recreates it; dependencies must be reinstalled afterwards.");
    add("env.venv_perm", "[venv] 权限被拒绝：.venv 可能被占用，请先停止进程再重试",
        "[venv] Permission denied: .venv may be in use; stop processes and retry");
    add("env.verify", "验证环境", "Verify env");
    add("env.install_cli", "安装 CLI 依赖", "Install CLI deps");
    add("env.install_srv", "安装服务端依赖", "Install server deps");
    add("env.init", "初始化项目", "Init project");
    add("env.init_start", "开始初始化项目…", "starting project init…");
    add("env.init_clone_done", "下载完成", "download done");
    add("env.init_clone_fail", "下载失败 exit=", "download failed exit=");
    add("env.init_missing_dirs", "注意: src/cli 或 src/server 不存在，依赖安装可能失败",
        "warning: src/cli or src/server missing, dependency install may fail");
    add("env.init_dirs_ok", "目录检查通过", "directory check passed");
    add("env.init_cli_fail", "CLI 依赖安装失败", "CLI deps install failed");
    add("env.init_cli_ok", "CLI 依赖安装完成", "CLI deps installed");
    add("env.init_srv_fail", "服务端依赖安装失败", "server deps install failed");
    add("env.init_srv_ok", "服务端依赖安装完成", "server deps installed");
    add("env.init_verify_warn", "verify_env.py 未通过或不存在，可手动点击「验证环境」检查",
        "verify_env.py failed or missing; click 'Verify env' to check");
    add("env.init_env", "生成环境变量文件 (.env)", "Create env files (.env)");
    add("env.init_done", "初始化完成！", "project initialized!");
    add("env.actions", "环境操作", "Environment actions");
    add("env.actions_desc", "创建依赖与校验", "Prepare dependencies and verify");
    add("env.components", "组件配置", "Component .env");
    add("env.components_desc", "CLI / 服务端 / Web 前端的 .env", ".env for CLI, server and web");
    add("env.env_exists", ".env 已存在", ".env exists");
    add("env.env_missing", ".env 未创建", ".env missing");
    add("env.gen", "从示例生成 .env", "Create .env from example");
    add("env.open_env", "打开 .env", "Open .env");
    add("env.open_example", "打开示例", "Open example");
    add("env.del_env", "删除 .env", "Delete .env");
    add("env.env_ok", "[env] 已从示例生成 .env", "[env] .env created from example");
    add("env.env_fail", "[env] 生成失败：缺少 .env.example", "[env] failed: missing .env.example");
    add("env.env_del", "[env] 已删除 .env", "[env] .env deleted");
    add("env.env_del_fail", "[env] 删除 .env 失败", "[env] delete .env failed");
    add("env.del_title", "删除 .env", "Delete .env");
    add("env.del_body", "删除后该组件将丢失本地配置，可从 .env.example 重新生成。",
        "The component loses its local config; it can be recreated from .env.example.");
    add("env.comp_cli", "CLI", "CLI");
    add("env.comp_srv", "服务端", "Server");
    add("env.comp_web", "Web 前端", "Web");
    add("env.config", "关键配置", "Key settings");
    add("env.config_desc", "读取自 src/cli/.env", "Read from src/cli/.env");
    add("env.llm", "LLM 识别", "LLM recognition");
    add("env.llm_on", "已启用", "Enabled");
    add("env.llm_off", "未启用", "Disabled");
    add("env.llm_db", "角色库对照", "Character DB matching");
    add("env.auto_update", "模型自动更新", "Auto model update");
    add("env.model_id", "模型 ID", "Model ID");
    add("env.dataset_dir", "数据集目录", "Dataset dir");
    add("env.gpu", "设备", "Device");
    add("env.pretrained_base", "训练基模", "Base model");

    // ---------------------------------------------------------------- 设置
    add("settings.title", "设置", "Settings");
    add("settings.subtitle", "语言、路径与控制台偏好", "Language, paths and console preferences");
    add("settings.language", "语言", "Language");
    add("settings.language_desc", "界面文案立即生效并写入配置", "Applies instantly and is saved");
    add("settings.chinese", "中文", "Chinese");
    add("settings.english", "English", "English");
    add("settings.paths", "路径", "Paths");
    add("settings.paths_desc", "修改后会重新推导 CLI / 服务端目录", "Re-derives CLI and server folders");
    add("settings.program_dir", "程序目录", "Program dir");
    add("settings.project_dir", "项目目录", "Project dir");
    add("settings.git_url", "源码下载地址", "Source archive URL");
    add("settings.git_url_desc", "GitHub 归档 zip，下载后自动解压到项目目录", "GitHub archive zip, unpacked into the project folder");
    add("settings.window", "窗口", "Window");
    add("settings.window_desc", "最小化与关闭行为", "Minimize and close behavior");
    add("settings.close_to_tray", "关闭时最小化到系统托盘", "Minimize to the tray when closing");
    add("settings.close_to_tray_hint", "点 × 时隐藏到托盘（托盘菜单「退出」仍会结束程序）；点 − 仍最小化到任务栏。",
        "× hides the window to the tray (Exit in the tray menu still quits); − minimizes to the taskbar.");
    add("settings.console", "控制台", "Console");
    add("settings.console_desc", "日志视图与自动滚动", "Log view and auto-scroll");
    add("settings.log_view", "日志视图", "Log view");
    add("settings.colored", "彩色", "Colored");
    add("settings.plain", "纯文本", "Plain text");
    add("settings.auto_scroll", "自动滚动到底部", "Auto-scroll to bottom");
    add("settings.log_height", "控制台高度", "Console height");
    add("settings.system", "系统信息", "System info");
    add("settings.system_desc", "CPU、内存与磁盘占用", "CPU, memory and disk");
    add("settings.cpu", "CPU 核心", "CPU cores");
    add("settings.mem", "内存", "Memory");
    add("settings.disk", "磁盘剩余", "Disk free");
    add("settings.about", "关于", "About");
    add("settings.about_desc", "37ACUI 是 37AC 的桌面控制器，遵循 GPL-3.0。",
        "37ACUI is the desktop controller for 37AC, licensed under GPL-3.0.");
    add("settings.open_config", "打开配置文件", "Open settings file");
    add("settings.design", "设计系统：樱花拿铁（与 37AC Web 端一致）", "Design system: Sakura Latte (shared with the 37AC web app)");

    // ---------------------------------------------------------------- 控制台
    add("console.title", "控制台", "Console");
    add("console.clear", "清空", "Clear");
    add("console.copy", "复制全部", "Copy all");
    add("console.colored", "彩色", "Colored");
    add("console.plain", "纯文本", "Plain");
    add("console.auto", "自动滚动", "Auto-scroll");
    add("console.send", "发送", "Send");
    add("console.hint", "输入命令后回车，例如 help、train、node", "Type a command and press Enter, e.g. help, train, node");
    add("console.input_hint", "train | predict | node | verify | crop | server | menu | help",
        "train | predict | node | verify | crop | server | menu | help");
    add("console.empty", "暂无输出", "No output yet");
    add("console.collapse", "收起", "Hide");
    add("console.expand", "展开", "Show");
    add("console.copied", "日志已复制到剪贴板", "Log copied to clipboard");
    add("console.stopped", "已停止当前进程", "Current process stopped");

    // ---------------------------------------------------------------- 状态栏
    add("status.python", "Python", "Python");
    add("status.venv", "venv", "venv");
    add("status.project", "项目", "Project");
    add("status.no_python", "Python 缺失", "Python missing");
    add("status.no_venv", "未创建 venv", "no venv");
    add("status.console", "控制台", "Console");
    add("status.proc", "进程", "Process");

    // ---------------------------------------------------------------- 提示 / Toast
    add("toast.started", "已启动：%s", "Started: %s");
    add("toast.node_started", "节点服务已启动", "Node service started");
    add("toast.server_started", "服务端已启动", "Server started");
    add("toast.stop_first", "已有进程在运行，请先停止", "Another process is running; stop it first");
    add("toast.need_python", "未检测到可用的 Python 环境", "No working Python environment detected");
    add("toast.need_project", "项目目录不存在，请先下载源码", "Project folder missing; download the source first");
    add("toast.need_dirs", "项目结构不完整，缺少 src/cli 或 src/server", "Incomplete project: src/cli or src/server missing");
    add("toast.bridge_ok", "%s 完成", "%s finished");
    add("toast.bridge_fail", "%s 失败：%s", "%s failed: %s");
    add("toast.env_saved", "已写入 .env：%s", "Saved to .env: %s");
    add("toast.reload_base", "新基模将在下一次训练生效", "The new base applies to the next training run");
    add("toast.need_token", "请先填写 Token", "Enter a token first");
    add("toast.need_repo", "请填写 Repo ID（user/repo）", "Enter a repo id (user/repo)");
    add("toast.llm_disabled", "LLM 识别未启用（.env LLM_RECOGNITION_ENABLED=False）",
        "LLM recognition is disabled (.env LLM_RECOGNITION_ENABLED=False)");
    add("toast.train_started", "训练已启动，输出见控制台", "Training started; see the console");
    add("toast.initializing", "正在初始化项目…", "Bootstrapping the project…");
    add("toast.downloading", "正在下载 37AC 源码…", "Downloading the 37AC source…");
    add("toast.confirm_delete", "已删除", "Deleted");
    add("toast.copy_fail", "剪贴板不可用", "Clipboard unavailable");
    add("toast.info_loading", "正在读取模型信息…", "Reading model info…");
    add("toast.image_needed", "请选择要识别的图片", "Pick an image to recognize");

    // ---------------------------------------------------------------- 日志标签
    add("log.init", "[init] ", "[init] ");
    add("log.pip", "[pip] ", "[pip] ");
    add("log.start", "[start] ", "[start] ");
    add("log.done", "[done] ", "[done] ");
    add("log.error", "[error] ", "[error] ");
    add("log.warn", "[warn] ", "[warn] ");

    // ---------------------------------------------------------------- 桥接动作名
    add("action.info", "读取模型信息", "Read model info");
    add("action.ckpt_restore", "回滚检查点", "Restore checkpoint");
    add("action.ckpt_delete", "删除检查点", "Delete checkpoint");
    add("action.ckpt_prune", "清理检查点", "Prune checkpoints");
    add("action.base_download", "下载基模", "Download base weights");
    add("action.hf_login", "HF 登录", "HF sign in");
    add("action.publish", "发布到 HF", "Publish to HF");
    add("action.verify", "验证图像", "Validate images");
    add("action.crop", "YOLO 裁剪", "YOLO crop");
    add("action.compress", "压缩数据集", "Compress dataset");
    add("action.jpeg", "转 JPEG", "Convert to JPEG");
}

const char *LangSys::get(const std::string &key) const
{
    auto it = m_data.find(key);
    // 注意：不能返回 key.c_str()——key 是临时 std::string 的引用，返回即悬垂
    if (it == m_data.end())
        return "";
    return (m_lang == Lang::Chinese) ? it->second.first : it->second.second;
}
