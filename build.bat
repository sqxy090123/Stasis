@echo off

:: Stasis 编译脚本 - 使用 MSVC 工具链
:: 支持在 Visual Studio 开发者命令提示符 或 GitHub Actions 中运行

set CL_FLAGS=/MT /O2 /GL /GS- /W4 /utf-8 /D UNICODE /D _UNICODE /D _CRT_SECURE_NO_WARNINGS
set LINK_FLAGS=/link user32.lib kernel32.lib gdi32.lib shell32.lib advapi32.lib psapi.lib comctl32.lib msimg32.lib /SUBSYSTEM:WINDOWS /OUT:Stasis.exe

:: 1. 编译资源文件
rc resource.rc

:: 2. 编译所有 C 文件并链接
cl %CL_FLAGS% main.c app_window.c process_manager.c monitor_engine.c ui_controls.c tray_icon.c settings_store.c log.c resource.res %LINK_FLAGS%

if %ERRORLEVEL% EQU 0 (
    echo 编译成功: Stasis.exe
) else (
    echo 编译失败，请检查代码。
    exit /b 1
)

:: 仅在本地交互式运行时暂停，CI 环境跳过
if "%CI%"=="" if "%GITHUB_ACTIONS%"=="" pause
del *.lib *.obj 2>nul