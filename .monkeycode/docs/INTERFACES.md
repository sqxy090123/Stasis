# Stasis 接口文档

Stasis 是一个 GUI 应用程序，主要通过**图形界面**与用户交互，无 HTTP/gRPC/API 接口。本文档记录其**用户可见交互界面**、**内部模块间接口**、**配置文件格式**及**命令行参数**。

## 1. 用户界面交互

### 1.1 主窗口

| 区域 | 组件 | 交互 | 说明 |
|------|------|------|------|
| 标题栏 | 应用图标 + "Stasis" 文字 | 左键拖拽移动窗口 | 无原生标题栏，自绘实现 |
| 标题栏 | 最小化按钮 (—) | 点击 → 隐藏窗口到托盘，进程降为 `BELOW_NORMAL_PRIORITY_CLASS` | 不退出程序 |
| 标题栏 | 关闭按钮 (✕) | 点击 → 同最小化 + 托盘气泡提示 "Stasis 已驻留后台" | 同上 |
| 状态栏 | CPU 渐变进度条 | 实时显示总 CPU 占用% | 蓝→青渐变 |
| 状态栏 | 内存渐变进度条 | 实时显示总内存占用% | 绿→青绿渐变 |
| 状态栏 | "已冻结: N" | 实时显示冻结栈深度 | 白色文字 |
| 控制区 | "自动" 开关 | 点击切换 `autoMode`，状态持久化到 INI | 开启时监控线程自动冻结/解冻 |
| 控制区 | "CPU" 滑块 | 拖动设置 `cpuThreshold` (50-100, 步进 5%) | 实时保存 |
| 控制区 | "内存" 滑块 | 拖动设置 `memThreshold` (50-100, 步进 5%) | 实时保存 |
| 控制区 | "解冻" 滑块 | 拖动设置 `thawThreshold` (30-80, 步进 5%) | 实时保存 |
| 控制区 | "白名单" 按钮 | 点击 → MessageBox "白名单功能待实现" | 预留功能 |
| 控制区 | "托盘" 按钮 | 点击 → 隐藏窗口到托盘 | 同标题栏最小化 |
| 控制区 | "选项" 按钮 | 点击 → MessageBox "选项功能待实现" | 预留功能 |
| 底部 | "提交反馈" 链接 | 点击 → 打开 GitHub Issues 页面 | `ShellExecuteW` |
| 列表区 | ListView (报表模式) | 显示进程：状态图标/进程名/PID/CPU%/内存(KB) | 1s 定时刷新，冻结进程显示 ❄️ |

### 1.2 系统托盘

| 交互 | 行为 |
|------|------|
| 托盘图标左键双击 | 显示主窗口 (`SW_RESTORE`)，恢复进程优先级 `NORMAL_PRIORITY_CLASS` |
| 托盘图标右键 | 弹出菜单：<br>1. **显示主界面** - 同双击<br>2. **暂停自动模式** - 切换 `pauseAuto`，菜单项打勾<br>3. **全部解冻并退出** - `DestroyWindow` 触发清理流程 |
| 窗口关闭 (WM_CLOSE) | 隐藏窗口 (`SW_HIDE`)，设置 `trayVisible=TRUE`，降低优先级，托盘气泡提示 |

### 1.3 启动参数

| 参数 | 说明 |
|------|------|
| `--debug` | 启用调试日志 (`g_DebugMode=TRUE`)，`DebugLog()` 写入详细信息 |

### 1.4 单实例保护

- 互斥体名称：`Stasis_SingleInstance_Mutex`
- 若已有实例运行：`FindWindowW(L"StasisMainWindowClass", L"Stasis")` 找到窗口 → `SetForegroundWindow` + `ShowWindow(SW_RESTORE)` → 退出新实例

## 2. 内部模块接口

### 2.1 `Stasis.h` 导出函数 (模块间调用)

#### 进程管理 (`process_manager.c`)

```c
// 初始化：加载 ntdll.dll 获取挂起/恢复函数指针
// 返回：TRUE 成功，FALSE 失败 (程序无法运行)
BOOL InitProcessAPI(void);

// 开启 SeDebugPrivilege，允许操作系统进程
// 返回：TRUE 成功
BOOL EnableDebugPrivilege(void);

// 挂起进程
// 参数：pid - 目标进程 ID
// 返回：TRUE 成功
BOOL SuspendProcessByPid(DWORD pid);

// 恢复进程
// 参数：pid - 目标进程 ID
// 返回：TRUE 成功
BOOL ResumeProcessByPid(DWORD pid);

// 枚举所有进程 PID
// 参数：pids - 输出数组，maxCount - 最大容量，outCount - 实际数量
// 返回：TRUE 成功
BOOL EnumProcessesEx(DWORD* pids, int maxCount, int* outCount);

// 获取进程内存 (KB)
// 参数：pid - 进程 ID，memKB - 输出内存值
// 返回：TRUE 成功
BOOL GetProcessMemoryKB(DWORD pid, SIZE_T* memKB);

// 检查系统关键进程 (System、Idle、csrss、winlogon、services、lsass、svchost、explorer、dwm、taskmgr)
// 参数：name - 进程文件名 (不含路径)
// 返回：TRUE 为关键进程
BOOL IsCriticalProcess(const WCHAR* name);

// 检查用户白名单 (线程安全，内部加锁)
// 参数：name - 进程文件名
// 返回：TRUE 在白名单中
BOOL IsProcessInUserWhitelist(const WCHAR* name);

// 检查进程是否拥有前台窗口
// 参数：pid - 进程 ID
// 返回：TRUE 为前台进程
BOOL IsProcessForeground(DWORD pid);

// 自我保护：防止冻结自身
// 参数：pid - 进程 ID
// 返回：TRUE 为当前进程
BOOL IsCurrentProcess(DWORD pid);
```

#### 监控引擎 (`monitor_engine.c`)

```c
// 获取系统总 CPU 使用率 (%)
// 返回：0.0-100.0
double GetTotalCpuUsage(void);

// 获取系统总内存使用率 (%)
// 返回：0.0-100.0
double GetTotalMemUsage(void);

// 核心冻结逻辑：按阈值冻结高 CPU 进程
void FreezeHighCpuProcesses(void);

// 解冻逻辑：资源低于解冻阈值时 LIFO 恢复
void ThawProcessesIfNeeded(void);

// 强制解冻所有 (退出/看门狗触发)
void ForceThawAll(void);

// 监控线程入口 (CreateThread 启动)
// 参数：param - 未使用
// 返回：线程退出码
DWORD WINAPI MonitorThread(LPVOID param);
```

#### UI 窗口 (`app_window.c`)

```c
// 创建主窗口
// 参数：hInstance - 模块句柄
// 返回：窗口句柄，NULL 失败
HWND CreateMainWindow(HINSTANCE hInstance);

// 主窗口过程 (注册给 WNDCLASSEXW)
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 从全局状态刷新 UI (InvalidateRect)
void UpdateUIFromState(void);

// 重新填充 ListView 进程列表
void RefreshListView(void);
```

#### 自绘控件 (`ui_controls.c`)

```c
// 绘制圆角按钮 (WM_DRAWITEM 调用)
// 参数：hdc - 绘图表面，rc - 按钮矩形，text - 按钮文字，hover - 悬停态，pressed - 按下态
void DrawRoundedButton(HDC hdc, RECT rc, const WCHAR* text, BOOL hover, BOOL pressed);

// 绘制渐变进度条
// 参数：hdc - 绘图表面，rc - 矩形，value - 0-100，color1/color2 - 渐变端点色
void DrawGradientProgressBar(HDC hdc, RECT rc, double value, COLORREF color1, COLORREF color2);

// 绘制开关 (Toggle Switch)
// 参数：hdc - 绘图表面，rc - 矩形，state - TRUE=开
void DrawToggleSwitch(HDC hdc, RECT rc, BOOL state);

// 绘制滑块
// 参数：hdc - 绘图表面，rc - 矩形，min/max - 范围，value - 当前值
void DrawSlider(HDC hdc, RECT rc, int min, int max, int value);

// 初始化自定义控件 (预留)
void InitCustomControls(void);

// 滑块子类化过程 (处理鼠标拖动更新阈值)
// 参数：标准子类化回调参数
LRESULT CALLBACK SliderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
```

#### 托盘图标 (`tray_icon.c`)

```c
// 创建托盘图标
// 参数：hwnd - 接收 WM_TRAYICON 的窗口
void CreateTrayIcon(HWND hwnd);

// 更新托盘图标提示 (当前仅 NIM_MODIFY)
// 参数：frozenCount - 冻结进程数
void UpdateTrayIcon(int frozenCount);

// 移除托盘图标
void RemoveTrayIcon(void);

// 显示托盘右键菜单
// 参数：hwnd - 所有者窗口
void ShowTrayMenu(HWND hwnd);
```

#### 配置存储 (`settings_store.c`)

```c
// 从 Stasis.ini 加载所有设置
void LoadSettings(void);

// 保存当前设置到 Stasis.ini
void SaveSettings(void);

// 设置/取消开机自启
// 参数：enable - TRUE 添加，FALSE 删除
// 返回：TRUE 成功
BOOL SetAutoStart(BOOL enable);

// 检查是否已设置开机自启
// 返回：TRUE 已设置
BOOL IsAutoStartEnabled(void);

// 将设置应用到 UI (当前仅 InvalidateRect)
void ApplySettingsToUI(void);
```

#### 日志系统 (`log.c`)

```c
// 初始化日志 (打开 Stasis.log，写入启动标记)
void InitLog(void);

// 关闭日志 (写入结束标记，关闭文件，删除临界区)
void CloseLog(void);

// 记录事件日志 (线程安全，带毫秒时间戳)
// 参数：format - 格式化字符串，... - 可变参数
void LogEvent(const WCHAR* format, ...);

// 记录调试日志 (仅 g_DebugMode=TRUE 时写入)
void DebugLog(const WCHAR* format, ...);
```

### 2.2 自定义窗口消息

| 消息 | 值 | 发送方 | 接收方 | 说明 |
|------|-----|--------|--------|------|
| `WM_TRAYICON` | `WM_APP + 1` | Shell_NotifyIcon | `MainWndProc` | 托盘鼠标事件 (`lParam=WM_RBUTTONUP/WM_LBUTTONDBLCLK`) |
| `WM_WATCHDOG_TIMER` | `WM_APP + 2` | `SetTimer(hwnd, WM_WATCHDOG_TIMER, 1000, NULL)` | `MainWndProc` | UI 线程心跳，`InterlockedIncrement(&g_State.watchdogCounter)` |
| `WM_WATCHDOG_ALERT` | `WM_APP + 3` | 监控线程 (`PostMessage`) | `MainWndProc` | 看门狗触发强制解冻，弹出警告对话框 |
| `WM_USER + 1` | `WM_USER + 1` | 监控线程 (`PostMessage`) | `MainWndProc` | 冻结栈变化通知 UI 刷新 (当前未显式处理，靠定时器刷新) |

## 3. 配置文件格式

### 3.1 `Stasis.ini` (UTF-16 LE, 写入时自动创建)

位置：可执行文件同目录

```ini
[Settings]
CpuThreshold=85          ; CPU 冻结阈值 (50-100)
MemThreshold=90          ; 内存冻结阈值 (50-100)
ThawThreshold=60         ; 解冻阈值 (30-80)
AutoMode=1               ; 自动模式: 1=开启, 0=关闭
Whitelist=notepad.exe;chrome.exe  ; 用户白名单，分号分隔
```

**读取逻辑** (`settings_store.c:LoadSettings`)：
- `GetPrivateProfileIntW` 读取整数，缺省值见 `Stasis.h` `DEFAULT_*`
- `GetPrivateProfileStringW` 读取 `Whitelist`，`wcstok_s` 按 `;` 分割，去除前导空格
- 动态分配 `WCHAR** userWhitelist`，受 `CRITICAL_SECTION` 保护

**写入逻辑** (`settings_store.c:SaveSettings`)：
- `WritePrivateProfileStringW` 逐项写入
- 白名单用 `wcscat_s` 拼接 `;` 分隔字符串

### 3.2 注册表 (开机自启)

路径：`HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`
值名称：`Stasis`
值类型：`REG_SZ`
值数据：可执行文件完整路径 (如 `C:\Path\Stasis.exe`)

API：`RegSetValueExW` / `RegDeleteValueW` / `RegQueryValueExW`

## 4. 资源文件接口

### 4.1 `resource.rc` 定义的资源

| 资源类型 | ID/名称 | 说明 |
|----------|---------|------|
| `ICON` | `APP_ICON` | `stasis.ico`，`LoadIconW(hInstance, L"APP_ICON")` 加载 |
| `RT_MANIFEST` | `1` (ID) | `manifest.xml`，请求管理员权限 + Per-Monitor V2 DPI 感知 |
| `VS_VERSION_INFO` | - | 版本信息块，`FILEVERSION/PRODUCTVERSION` 由 `build.bat` 传入宏定义 |

### 4.2 版本信息字段

| 字段 | 来源宏 | 示例 |
|------|--------|------|
| `CompanyName` | 硬编码 | `Stasis Project` |
| `FileDescription` | 硬编码 | `Stasis Process Manager` |
| `FileVersion` | `VERSION_MAJOR.MINOR.BUILD.REVISION` | `1.0.0.123` |
| `InternalName` | 硬编码 | `Stasis` |
| `LegalCopyright` | 硬编码 | `Copyright (C) 2025` |
| `OriginalFilename` | 硬编码 | `Stasis.exe` |
| `ProductName` | 硬编码 | `Stasis Process Manager` |
| `ProductVersion` | 同上 | 同上 |

## 5. 构建接口

### 5.1 `build.bat` 参数

```cmd
build.bat [版本号]
```

| 参数格式 | 示例 | 解析结果 |
|----------|------|----------|
| `major.minor.build.revision` | `build.bat 1.0.0.5` | 四段版本号 |
| `dev-commit` | `build.bat dev-a1b2c3d` | 开发版，内部版本号置 0 |
| 无参数 | `build.bat` | `1.0.0.%BUILD_VERSION%` (CI 传入 `github.run_number`) |

环境变量：
- `BUILD_VERSION` - CI 注入的运行编号 (默认 0)
- `CI` / `GITHUB_ACTIONS` - 检测 CI 环境，跳过 `pause`

### 5.2 编译器标志 (`build.bat`)

```bat
CL_FLAGS=/MT /O2 /GL /GS- /W4 /utf-8 /D UNICODE /D _UNICODE /D _CRT_SECURE_NO_WARNINGS
LINK_FLAGS=/link user32.lib kernel32.lib gdi32.lib shell32.lib advapi32.lib psapi.lib comctl32.lib msimg32.lib /SUBSYSTEM:WINDOWS /OUT:Stasis.exe
```

| 标志 | 含义 |
|------|------|
| `/MT` | 静态链接 CRT (无需 VC++ 运行时) |
| `/O2` | 最大速度优化 |
| `/GL` | 全程序优化 (LTCG) |
| `/GS-` | 禁用缓冲区安全检查 (性能) |
| `/W4` | 警告等级 4 |
| `/utf-8` | 源文件编码 UTF-8 |

### 5.3 GitHub Actions 工作流 (`.github/workflows/build.yml`)

触发：`push` to `main` / `pull_request` / `workflow_dispatch` / `release` (published)

关键步骤：
1. `vswhere` 定位最新 MSVC
2. `vcvarsall.bat x64` 设置环境
3. `build.bat ${{ steps.version.outputs.VERSION }}` 编译
4. `cl /analyze /W4 /c *.c` 静态分析 (非阻断)
5. 上传 `Stasis.exe` 为 Artifact (保留 30 天)
6. Tag 触发：`softprops/action-gh-release@v2` 创建 Release，附件 `Stasis.exe`
7. Main 分支 push：生成预发布 Artifact (保留 7 天)

## 6. 运行时文件

| 文件 | 位置 | 说明 |
|------|------|------|
| `Stasis.ini` | 可执行文件目录 | 用户配置，首次运行自动创建默认值 |
| `Stasis.log` | 可执行文件目录 | 运行日志，>50KB 自动轮转保留 500 行 |
| `stasis.ico` | 资源内嵌 | 应用图标，托盘/窗口/任务栏显示 |

## 7. 权限需求

| 权限 | 用途 | 获取方式 |
|------|------|----------|
| `SeDebugPrivilege` | `OpenProcess(PROCESS_SUSPEND_RESUME)` 挂起/恢复任意进程 | `EnableDebugPrivilege()` 启动时调用 |
| 管理员权限 | 操作系统进程、写入 HKCU\Run (需管理员才能挂起系统进程) | `manifest.xml: requireAdministrator` |
| `PROCESS_SUSPEND_RESUME` | `NtSuspendProcess/NtResumeProcess` | 打开进程时请求 |

## 8. 错误码与约定

- 所有 `BOOL` 返回函数：`TRUE` 成功，`FALSE` 失败 (调用 `GetLastError()` 获取详情)
- `HANDLE` 返回：`NULL`/`INVALID_HANDLE_VALUE` 表示失败
- 关键初始化失败 (`InitProcessAPI`、`CreateMainWindow`) → `MessageBox` 报错并 `return 1` 退出
- 日志写入失败：静默忽略 (不影响主功能)