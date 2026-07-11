@echo off

:: Stasis 编译脚本 - 使用 MSVC 工具链
:: 支持在 Visual Studio 开发者命令提示符 或 GitHub Actions 中运行
:: 用法: build.bat [版本号]  例如: build.bat 1.0.0.123

set CL_FLAGS=/MT /O2 /GL /GS- /W4 /utf-8 /D UNICODE /D _UNICODE /D _CRT_SECURE_NO_WARNINGS
set LINK_FLAGS=/link user32.lib kernel32.lib gdi32.lib shell32.lib advapi32.lib psapi.lib comctl32.lib msimg32.lib /SUBSYSTEM:WINDOWS /OUT:Stasis.exe

:: 解析版本号参数 (支持: major.minor.build.revision 或 dev-commit)
if "%1"=="" (
    set VERSION_MAJOR=1
    set VERSION_MINOR=0
    set VERSION_BUILD=0
    set VERSION_REVISION=%BUILD_VERSION%
    set VERSION_FULL=1.0.0.%BUILD_VERSION%
) else (
    set "VER=%~1"
    echo 检测到版本参数: %VER%
    :: 检查是否包含点号
    echo %VER% | findstr /R "\." >nul
    if %ERRORLEVEL% EQU 0 (
        :: 标准版本号格式: major.minor.build.revision
        for /f "tokens=1-4 delims=." %%a in ("%VER%") do (
            set VERSION_MAJOR=%%a
            set VERSION_MINOR=%%b
            set VERSION_BUILD=%%c
            set VERSION_REVISION=%%d
        )
    ) else (
        :: 开发版本格式: dev-commit
        set VERSION_MAJOR=0
        set VERSION_MINOR=0
        set VERSION_BUILD=0
        set VERSION_REVISION=0
    )
    set VERSION_FULL=%VER%
)

:: 设置资源编译器定义
set RC_FLAGS=/D VERSION_MAJOR=%VERSION_MAJOR% /D VERSION_MINOR=%VERSION_MINOR% /D VERSION_BUILD=%VERSION_BUILD% /D VERSION_REVISION=%VERSION_REVISION%

echo 编译版本: %VERSION_FULL% (内部: %VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_BUILD%.%VERSION_REVISION%)

:: 1. 编译资源文件
rc %RC_FLAGS% resource.rc
if %ERRORLEVEL% NEQ 0 (
    echo 资源编译失败
    exit /b 1
)

:: 2. 编译所有 C 文件并链接
cl %CL_FLAGS% main.c app_window.c process_manager.c monitor_engine.c ui_controls.c tray_icon.c settings_store.c log.c resource.res %LINK_FLAGS%

if %ERRORLEVEL% EQU 0 (
    echo 编译成功: Stasis.exe (版本 %VERSION_FULL%)
    exit /b 0
) else (
    echo 编译失败，请检查代码。
    exit /b 1
)

:: 仅在本地交互式运行时暂停，CI 环境跳过
if "%CI%"=="" if "%GITHUB_ACTIONS%"=="" pause
del *.lib *.obj 2>nul