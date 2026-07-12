# Stasis 开发者指南

## 项目目的

**Stasis** 是一个 Windows 原生进程管理工具，核心使命是在系统资源（CPU/内存）紧张时，**自动冻结高占用后台进程**，释放资源给前台应用，避免系统卡顿。它在更大系统中担任**“资源守护者”**角色：用户无感知地后台运行，关键时刻介入，事后自动恢复。

**核心职责**：
- 实时采样系统 CPU/内存负载（500ms 周期）
- 基于滞回阈值决策冻结/解冻
- 保护系统关键进程、用户白名单进程、前台进程不被误伤
- 提供现代化 GUI 供用户调整阈值、查看状态、管理白名单
- 托盘驻留，关闭即最小化，开机自启可配置

**相关系统**：
- Windows OS (ntdll.dll, kernel32.dll, psapi.dll, user32.dll, shell32.dll, comctl32.dll, msimg32.dll)
- GitHub Actions CI/CD (构建、静态分析、Release 发布)

---

## 环境搭建

### 前置条件

| 工具 | 版本要求 | 用途 |
|------|----------|------|
| Visual Studio 2022 (Community+) | 17.0+ | MSVC 编译器 (`cl.exe`)、链接器 (`link.exe`)、`vswhere.exe` |
| Windows 10 SDK | 10.0.19041.0+ | 头文件、库文件、`rc.exe` 资源编译器 |
| Git | 任意 | 版本控制 |
| (可选) Windows Terminal | - | 更好的命令行体验 |

### 安装与构建

```bash
# 1. 克隆仓库
git clone https://github.com/sqxy090123/Stasis.git
cd Stasis

# 2. 打开 "Developer Command Prompt for VS 2022" (或在 VS 中打开)
#    确保 cl.exe、link.exe、rc.exe 在 PATH 中

# 3. 编译 (默认版本 1.0.0.<BUILD_VERSION>)
build.bat

# 4. 或指定版本号
build.bat 1.2.3.456
# 或开发版
build.bat dev-abc1234

# 5. 运行 (需管理员权限)
Stasis.exe
# 调试模式
Stasis.exe --debug
```

### 环境变量

| 变量 | 必需 | 说明 | 示例 |
|------|------|------|------|
| `BUILD_VERSION` | 否 | CI 注入的构建号，无参数时作为 revision | `123` (GitHub Actions `github.run_number`) |
| `CI` / `GITHUB_ACTIONS` | 否 | 检测到时 `build.bat` 跳过 `pause` | `true` |

> ⚠️ **绝不提交密钥**。本项目无需 API Key。如需扩展云同步配置，请使用 `.env` 文件或 Windows 凭据管理器，并在 `.gitignore` 排除。

---

## 开发工作流

### 代码质量工具

| 工具 | 命令 | 目的 |
|------|------|------|
| MSVC 静态分析 | `cl /analyze /W4 /c *.c` | 潜在缺陷检测 (CI 执行，非阻断) |
| 编译警告 | `/W4 /WX-` | 等级 4 警告，不视为错误 |
| 运行时检查 | `/RTC1` (Debug) | 栈帧损坏、未初始化变量 (仅调试构建) |

> 本项目无单元测试框架。核心逻辑在 `monitor_engine.c`，建议手动测试：高负载制造（`stress -c 4` / 重型编译）、白名单验证、前台唤醒、看门狗触发。

### 提交前检查

本地执行：
1. `build.bat` 成功产出 `Stasis.exe`
2. 以管理员运行，验证：
   - 窗口正常显示、拖拽、最小化到托盘、双击托盘恢复
   - 滑块拖动阈值实时保存、开关切换持久化
   - `Stasis.ini` 正确更新
   - 高负载下自动冻结/解冻 (可用 `Task Manager` 观察进程状态)
   - 关闭窗口→托盘驻留→右键“全部解冻并退出”正常退出
3. `git diff` 确认无调试代码、无硬编码路径

### 分支策略

| 分支 | 用途 | 保护 |
|------|------|------|
| `main` | 生产就绪代码 | 必须 PR + CI 通过 |
| `feature/*` | 新功能 | 从 `main` 切出，完成后 PR 合并 |
| `fix/*` | Bug 修复 | 同上 |
| `release/vX.Y.Z` | 发布准备 | 仅版本号、更新日志修改 |

### Pull Request 流程

1. 从 `main` 创建 `feature/xxx` 或 `fix/xxx`
2. 编写代码，**每个逻辑变更单独提交**，message 规范见下
3. 推送分支，创建 PR，**填写描述**：
   - **What**: 解决什么问题/添加什么功能
   - **Why**: 为什么这样做 (替代方案对比)
   - **How**: 核心实现要点
   - **Test**: 如何验证 (步骤 + 预期结果)
4. CI 自动运行：编译 + 静态分析 + Artifact 上传
5. Code Review：至少 1 人 Approve
6. **Squash 合并** 到 `main` (保持历史整洁)
7. 如需发布：`git tag vX.Y.Z && git push origin vX.Y.Z` → 自动创建 Release

### 提交信息规范

```
<type>(<scope>): <subject>

<body>

<footer>
```

| type | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `refactor` | 重构 (无功能变更) |
| `perf` | 性能优化 |
| `docs` | 文档更新 |
| `chore` | 构建/工具/依赖更新 |
| `ci` | CI 配置变更 |

**示例**：
```
feat(monitor): add per-round freeze limit (max 5) to prevent cascade

Previously FreezeHighCpuProcesses could freeze unlimited processes
in one round when CPU spiked, causing system instability. Now capped
at 5 per 500ms cycle with 100ms interval between each suspend.

Test: stress -c 8 for 30s, verify frozen count <= 5 per round via log.
```

---

## 常见任务

### 添加新配置项 (阈值/开关等)

**涉及文件**：
1. `Stasis.h` - 新增 `#define DEFAULT_XXX` + `AppState` 字段
2. `settings_store.c` - `LoadSettings`/`SaveSettings` 读写 INI
3. `app_window.c` - `WM_CREATE` 创建控件、`WM_COMMAND`/`WM_DRAWITEM` 处理、`ApplySettingsToUI` 同步
4. `monitor_engine.c` - 使用新阈值决策
5. `build.bat` - 无需改动

**步骤**：
1. 在 `AppState` 增加 `int newThreshold;`
2. `LoadSettings`: `GetPrivateProfileIntW(L"Settings", L"NewThreshold", DEFAULT_NEW_THRESHOLD, CONFIG_FILE)`
3. `SaveSettings`: `WritePrivateProfileStringW` 写入
4. `WM_CREATE` 创建滑块/开关，`SetWindowSubclass` 或 `WM_COMMAND` 处理
4. `WM_DRAWITEM` 调用 `DrawSlider`/`DrawToggleSwitch`
5. `monitor_engine.c` 读取 `g_State.newThreshold` 参与判断
6. 运行验证：修改滑块 → `Stasis.ini` 更新 → 重启程序值保持

### 添加新自绘控件

**涉及文件**：`ui_controls.c`、`app_window.c`、`Stasis.h`

**步骤**：
1. `ui_controls.c` 实现 `DrawXxx(HDC, RECT, ...)` 绘制函数
2. `Stasis.h` 声明该函数
3. `app_window.c` `WM_CREATE` 创建 `BS_OWNERDRAW` 按钮，`SetWindowSubclass` 处理鼠标交互
4. `WM_DRAWITEM` 中 `GetDlgCtrlID` 分发调用 `DrawXxx`
5. 交互消息 (`WM_LBUTTONDOWN`/`WM_MOUSEMOVE`/`WM_LBUTTONUP`) 在子类化过程更新状态并 `InvalidateRect`

### 添加新进程过滤规则 (如：排除特定签名进程)

**涉及文件**：`process_manager.c`、`Stasis.h`

**步骤**：
1. `process_manager.c` 新增 `BOOL IsProcessAllowed(DWORD pid)` 或扩展 `IsCriticalProcess`
2. 使用 `WinVerifyTrust` / `GetFileVersionInfo` 验证签名/发布者
3. `FreezeHighCpuProcesses` 中调用新检查，通过则跳过冻结
4. 可选：在 `settings_store.c` 增加配置项持久化规则

### 修复 Bug

**流程**：
1. 编写复现步骤 (Issue 或 PR 描述)
2. 定位根因：结合 `Stasis.log` (含时间戳)、`DebugLog`、Visual Studio 调试器
3. 最小改动修复 (单一职责)
4. 验证：原复现步骤不再触发，回归测试核心流程 (冻结/解冻/托盘/配置持久化)
5. 提交：`fix(<module>): <简短描述>`

**示例提交**：
```
fix(process_manager): prevent self-freeze deadlock in FreezeHighCpuProcesses

IsCurrentProcess check was missing in freeze loop, causing Stasis to
suspend its own PID when total CPU spiked. Added early-continue guard.

Test: run Stasis, induce high CPU (stress), verify Stasis.exe never appears
in frozen stack (LogEvent shows skipped self-pid).
```

### 添加数据库 Migration (不适用)

本项目使用 INI 文件 + 注册表，无数据库。

---

## 编码规范

### 文件组织

- 每个 `.c` 对应一个功能模块，`Stasis.h` 统一导出声明
- 文件名 PascalCase：`process_manager.c`、`ui_controls.c`
- 内部函数 `static`，仅头文件声明对外 API

### 命名约定

| 类型 | 约定 | 示例 |
|------|------|------|
| 文件 | snake_case | `settings_store.c` |
| 函数 | PascalCase | `SuspendProcessByPid` |
| 全局变量 | `g_` 前缀 + PascalCase | `g_State`, `g_DebugMode` |
| 结构体 | PascalCase + `typedef` | `AppState`, `ProcCpu` |
| 结构体成员 | camelCase | `autoMode`, `frozenCount` |
| 宏/常量 | SCREAMING_SNAKE | `DEFAULT_CPU_THRESHOLD`, `WM_WATCHDOG_TIMER` |
| 枚举值 | PascalCase | `IDC_TOGGLE_AUTO` |
| 临时变量 | camelCase | `pid`, `cpuUsage` |

### 错误处理

```c
// 推荐：具体错误类型 + 上下文
HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
if (!hProcess) {
    LogEvent(L"OpenProcess failed for PID %lu, err=%lu", pid, GetLastError());
    return FALSE;
}

// 避免：通用错误
if (!hProcess) return FALSE;
```

### 日志规范

```c
// 关键操作：LogEvent (始终写入)
LogEvent(L"Frozen PID %lu (name=%s)", pid, name);

// 调试细节：DebugLog (仅 --debug 模式)
DebugLog(L"MonitorThread: totalCpu=%.1f, threshold=%d", totalCpu, g_State.cpuThreshold);
```

- 所有日志带 `[YYYY-MM-DD HH:MM:SS.mmm]` 时间戳 (见 `log.c`)
- 线程安全：`EnterCriticalSection(&g_LogCs)` / `LeaveCriticalSection(&g_LogCs)`
- 文件 >50KB 自动保留最后 500 行 (`LOG_MAX_LINES`)

### 测试约定

- 无自动化测试框架，依赖**手动验证清单**
- 核心变更必须覆盖：
  - [ ] 编译通过 (`build.bat`)
  - [ ] 管理员运行无报错
  - [ ] 窗口交互正常 (拖拽、最小化、托盘、滑块、开关)
  - [ ] 配置持久化 (修改→重启→值保持)
  - [ ] 高负载冻结/解冻 (可用 `stress` 或大型编译触发)
  - [ ] 看门狗触发 (人为卡死 UI 线程 3s+)
  - [ ] 前台唤醒 (冻结后点击该窗口)
  - [ ] 退出清理 (ForceThawAll、关闭日志、删除临界区、释放白名单内存)

---

## 安全起步点 (低风险修改区域)

| 区域 | 文件 | 风险 | 适合任务 |
|------|------|------|----------|
| UI 文案/颜色/布局 | `app_window.c` (DrawTitleBar, WM_PAINT) | 极低 | 文案调整、配色微调、控件位置 |
| 默认阈值 | `Stasis.h` (DEFAULT_*) | 低 | 调整出厂默认值 |
| 日志格式/轮转参数 | `log.c` | 低 | 时间戳格式、最大行数、文件大小阈值 |
| 托盘菜单项 | `tray_icon.c` (ShowTrayMenu) | 低 | 增加/调整菜单项 |
| 资源图标/清单 | `resource.rc`, `manifest.xml`, `stasis.ico` | 低 | 替换图标、调整 DPI/管理员设置 |

---

## 高风险区域 (需谨慎/资深审核)

| 区域 | 文件 | 风险点 |
|------|------|--------|
| 进程挂起/恢复 | `process_manager.c` (NtSuspend/ResumeProcess) | 误挂系统进程→蓝屏/死机；权限提升失败→静默失败 |
| 监控调度核心 | `monitor_engine.c` (FreezeHighCpuProcesses/ThawProcessesIfNeeded) | 死锁、竞态、冻结栈溢出、阈值抖动 |
| 看门狗/强制解冻 | `monitor_engine.c` (ForceThawAll, Watchdog) | 强制解冻时机、UI 线程通信竞态 |
| 临界区同步 | 所有访问 `g_State` 的位置 | 死锁 (嵌套锁)、忘记 Leave、锁粒度过大 |
| DPI/字体重建 | `app_window.c` (UpdateDpiScale/RebuildFonts) | 资源泄漏 (DeleteObject 遗漏)、字体句柄失效 |
| 单实例互斥体 | `main.c` (CreateMutex/FindWindow) | 互斥体遗弃、窗口类名冲突 |

---

## 附录：快速命令参考

```bash
# 构建
build.bat                    # 默认版本
build.bat 1.0.1.42           # 指定版本
build.bat dev-abc123         # 开发版

# 运行 (需管理员)
Stasis.exe
Stasis.exe --debug           # 启用 DebugLog

# 查看日志
type Stasis.log
notepad Stasis.log

# 查看配置
type Stasis.ini

# 清理构建产物
del *.obj *.lib *.res *.exp

# Git 工作流
git checkout -b feat/xxx
git add -A
git commit -m "feat(module): description"
git push origin feat/xxx
# 创建 PR → Review → Squash merge

# 发布
git tag v1.2.0
git push origin v1.2.0
# → GitHub Actions 自动构建 Release
```

---

## 重要文件速查

| 文件 | 核心职责 | 关键函数/结构 |
|------|----------|---------------|
| `Stasis.h` | 全局类型/宏/声明 | `AppState`, `g_State`, 所有模块 API |
| `main.c` | 入口、单实例、消息泵 | `wWinMain`, `MonitorThread` 启动 |
| `app_window.c` | 主窗口、UI、消息分发 | `MainWndProc`, `RefreshListView`, `DrawTitleBar` |
| `process_manager.c` | 进程挂起/恢复/枚举/白名单 | `SuspendProcessByPid`, `IsCriticalProcess`, `IsCurrentProcess` |
| `monitor_engine.c` | 监控线程、冻结/解冻调度 | `MonitorThread`, `FreezeHighCpuProcesses`, `ForceThawAll` |
| `ui_controls.c` | 自绘控件实现 | `DrawRoundedButton`, `DrawGradientProgressBar`, `DrawToggleSwitch`, `DrawSlider`, `SliderSubclassProc` |
| `tray_icon.c` | 托盘图标/菜单 | `CreateTrayIcon`, `ShowTrayMenu` |
| `settings_store.c` | INI/注册表持久化 | `LoadSettings`, `SaveSettings`, `SetAutoStart` |
| `log.c` | 线程安全日志 | `LogEvent`, `DebugLog`, `InitLog`, `CloseLog` |
| `build.bat` | 编译脚本 | 版本号解析、RC 宏传递、cl/link 调用 |
| `resource.rc` | 资源定义 | 图标、清单、版本信息 |
| `manifest.xml` | 管理员+DPI 感知 | `requireAdministrator`, `PerMonitorV2` |
| `.github/workflows/build.yml` | CI/CD | vswhere、vcvarsall、build.bat、静态分析、Release |

---

## 贡献检查清单 (PR 提交前)

- [ ] `build.bat` 无警告通过 (或仅接受已知无害警告)
- [ ] 管理员运行 `Stasis.exe` 主流程无崩溃
- [ ] 新增配置项已在 `LoadSettings`/`SaveSettings` 处理
- [ ] 新增 UI 控件已 `WM_SETFONT`、`WM_DRAWITEM`、`WM_COMMAND` 完整处理
- [ ] 所有共享状态访问均在 `Enter/LeaveCriticalSection(&g_State.cs)` 保护下
- [ ] `LogEvent` 记录关键路径，`DebugLog` 记录调试细节
- [ ] 无硬编码路径、无 `printf`/`MessageBox` 残留调试代码
- [ ] 提交信息符合规范，PR 描述完整 (What/Why/How/Test)
- [ ] 更新相关文档 (`ARCHITECTURE.md`, `INTERFACES.md`, `DEVELOPER_GUIDE.md`)