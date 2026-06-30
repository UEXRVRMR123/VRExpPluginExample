@echo off
chcp 65001 >nul

:: ── 1. 自动查找当前目录下的 .uproject 文件 ──────────────────────────────────
set "UPROJECT="
for %%f in ("%~dp0*.uproject") do set "UPROJECT=%%f"

if not defined UPROJECT (
    echo [错误] 当前目录下未找到 .uproject 文件！
    pause & exit /b 1
)
echo 项目文件：%UPROJECT%

:: ── 2. 从 .uproject JSON 读取 EngineAssociation ──────────────────────────────
set "ENGINE_ID="
for /f "tokens=2 delims=:, " %%a in ('findstr /i "EngineAssociation" "%UPROJECT%"') do set "ENGINE_ID=%%~a"

if not defined ENGINE_ID (
    echo [错误] 无法从 .uproject 读取 EngineAssociation！
    pause & exit /b 1
)
echo 引擎标识：%ENGINE_ID%

:: ── 3. 查注册表 —— 先查自定义引擎（RegisterEngine.bat 写入的路径） ───────────
set "ENGINE_PATH="
for /f "tokens=2,*" %%a in (
    'reg query "HKEY_CURRENT_USER\Software\Epic Games\Unreal Engine\Builds" /v "%ENGINE_ID%" 2^>nul ^| findstr REG_SZ'
) do set "ENGINE_PATH=%%b"

:: 若未找到，再查 Launcher 安装的官方版本
if not defined ENGINE_PATH (
    for /f "tokens=2,*" %%a in (
        'reg query "HKEY_LOCAL_MACHINE\SOFTWARE\EpicGames\Unreal Engine\%ENGINE_ID%" /v "InstalledDirectory" 2^>nul ^| findstr REG_SZ'
    ) do set "ENGINE_PATH=%%b"
)

if not defined ENGINE_PATH (
    echo [错误] 注册表中未找到引擎 [%ENGINE_ID%] 的安装路径！
    echo        请先运行引擎目录下的 RegisterEngine.bat
    pause & exit /b 1
)

:: 将路径中的正斜杠统一为反斜杠
set "ENGINE_PATH=%ENGINE_PATH:/=\%"
echo 引擎路径：%ENGINE_PATH%
echo.

:: ── 4. 调用 UnrealBuildTool 生成 VS Code 工作区 ──────────────────────────────
echo 正在生成 VS Code 工作区...
"%ENGINE_PATH%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles -vscode -project="%UPROJECT%" -game

echo.
if %ERRORLEVEL% == 0 (
    echo [成功] 请用 VS Code 打开 .code-workspace 文件
) else (
    echo [失败] 请检查日志：
    echo        C:\Users\%USERNAME%\AppData\Local\UnrealBuildTool\Log_GPF.txt
)
echo.
pause
