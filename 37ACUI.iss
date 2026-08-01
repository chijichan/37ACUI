; ============================================================
;  37ACUI.iss — Inno Setup 安装脚本
;  用法: iscc 37ACUI.iss
;        （可用 /DBuildConfig=Debug 切换打包 Debug 构建）
; ============================================================

; ---- 预处理器常量（可通过 iscc /Dxxx=yyy 覆盖）----
#ifndef AppVersion
  #define AppVersion "0.0.1"
#endif
#ifndef AppId
  #define AppId "{{6E5F7A93-2B4C-4D8E-9F1A-3C7D5E9B2A41}"
#endif
#ifndef BuildDir
  #define BuildDir "build"
#endif
#ifndef BuildConfig
  #define BuildConfig "Release"
#endif

[Setup]
AppId={#AppId}
AppName=37ACUI
AppVersion={#AppVersion}
AppVerName=37ACUI {#AppVersion}
AppPublisher=37ACUI Project
AppPublisherURL=https://github.com/chijichan/37AC
AppSupportURL=https://github.com/chijichan/37AC
AppComments=37AC 控制器 — 服务端管理与 CLI 操作图形界面

; 程序把 acui.ini / acui_settings.ini 写在 exe 所在目录，
; 若装到 Program Files 普通用户将无法写入配置，
; 因此使用用户级目录安装，无需管理员权限，配置始终可写。
DefaultDirName={localappdata}\Programs\37ACUI
DefaultGroupName=37ACUI
DisableProgramGroupPage=yes
PrivilegesRequired=lowest

; 仅支持 64 位系统（vcpkg x64-windows 构建）
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

MinVersion=6.1sp1
WizardStyle=modern dynamic
UninstallDisplayIcon={app}\ACUI.exe
UninstallDisplayName=37ACUI

SetupIconFile=resources\37AC.ico
LicenseFile=LICENSE
VersionInfoVersion={#AppVersion}
VersionInfoCompany=37ACUI Project
VersionInfoDescription=37ACUI 安装程序
VersionInfoProductName=37ACUI
VersionInfoProductVersion={#AppVersion}

Compression=lzma2/ultra
SolidCompression=yes
OutputDir=installer
OutputBaseFilename=37ACUI-Setup-{#AppVersion}
; 安装结束后不自动重启
RestartIfNeededByRun=no

[Languages]
; 若编译器缺少 ChineseSimplified.isl（官方安装包不含简体中文），
; 请从 https://jrsoftware.org/files/istrans/ 下载放到 Inno Setup 的 Languages 目录，
; 或删除下面这一行改用英文安装界面。
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; ---- 主程序与运行时依赖 ----
Source: "{#BuildDir}\{#BuildConfig}\ACUI.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\{#BuildConfig}\glfw3.dll"; DestDir: "{app}"; Flags: ignoreversion
; ---- 默认 ImGui 窗口布局（用户设置 acui_settings.ini 由程序运行时生成，不打包）----
Source: "acui.ini"; DestDir: "{app}"; Flags: ignoreversion
; ---- 文档 ----
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Icons]
Name: "{group}\37ACUI"; Filename: "{app}\ACUI.exe"
Name: "{autodesktop}\37ACUI"; Filename: "{app}\ACUI.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ACUI.exe"; Description: "{cm:LaunchProgram,37ACUI}"; Flags: nowait postinstall skipifsilent

; ---- 卸载时清理程序生成的用户数据 ----
[UninstallDelete]
Type: files; Name: "{app}\acui.ini"
Type: files; Name: "{app}\acui_settings.ini"
Type: filesandordirs; Name: "{app}\project"
Type: filesandordirs; Name: "{app}\.venv"

[Code]
// 检测 ACUI.exe 是否正在运行（程序无互斥体，按进程名判断）
function IsAppRunning: Boolean;
var
  FSWbemLocator: Variant;
  FWMIService: Variant;
  FWbemObjectSet: Variant;
begin
  Result := False;
  try
    FSWbemLocator := CreateOleObject('WbemScripting.SWbemLocator');
    FWMIService := FSWbemLocator.ConnectServer('', 'root\CIMV2');
    FWbemObjectSet := FWMIService.ExecQuery(
      'SELECT Name FROM Win32_Process WHERE Name = ''ACUI.exe''');
    Result := not VarIsNull(FWbemObjectSet) and (FWbemObjectSet.Count > 0);
  except
  end;
end;

procedure EnsureAppNotRunning;
begin
  if IsAppRunning then
  begin
    MsgBox('检测到 37ACUI (ACUI.exe) 正在运行。' #13#13
           '请先退出程序后再继续，否则文件可能被占用导致操作失败。',
           mbError, MB_OK);
    Abort;
  end;
end;

function InitializeSetup: Boolean;
begin
  Result := True;
  EnsureAppNotRunning;
end;

function InitializeUninstall: Boolean;
begin
  Result := True;
  EnsureAppNotRunning;
end;
