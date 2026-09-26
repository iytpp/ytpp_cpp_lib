param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# ============================================================
# 项目配置
# ============================================================

$ProjectRoot = $PSScriptRoot

$VcVarsAll = "D:\.Software\Visual Studio 2026\VC\Auxiliary\Build\vcvarsall.bat"

$BuildConfigs = @(
    @{
        Name   = "x64 Debug"
        Preset = "x64-debug-local"
        Arch   = "x64"
    },
    @{
        Name   = "x64 Release"
        Preset = "x64-release-local"
        Arch   = "x64"
    },
    @{
        Name   = "x86 Debug"
        Preset = "x86-debug-local"
        Arch   = "x86"
    },
    @{
        Name   = "x86 Release"
        Preset = "x86-release-local"
        Arch   = "x86"
    }
)


# ============================================================
# 工具函数
# ============================================================

function Write-Title {
    param(
        [string]$Text
    )

    Write-Host ""
    Write-Host "============================================================"
    Write-Host " $Text"
    Write-Host "============================================================"
}


function Stop-WithError {
    param(
        [string]$Message
    )

    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Red
    Write-Host " 构建失败" -ForegroundColor Red
    Write-Host "============================================================" -ForegroundColor Red
    Write-Host ""
    Write-Host $Message -ForegroundColor Red
    Write-Host ""

    Read-Host "按 Enter 键关闭窗口"
    exit 1
}


function Invoke-MsvcCommand {
    param(
        [string]$Arch,
        [string]$Command
    )

    # 在初始化 MSVC 环境之前统一控制台编码和工具链语言，确保 CMake 与 Ninja
    # 使用相同编码识别 /showIncludes 输出，避免打印海量头文件依赖信息。
    $fullCommand = "chcp 65001 >nul && set `"VSLANG=1033`" && call `"$VcVarsAll`" $Arch >nul && $Command"

    $process = Start-Process `
        -FilePath "cmd.exe" `
        -ArgumentList "/d", "/s", "/c", "`"$fullCommand`"" `
        -NoNewWindow `
        -PassThru

    # Start-Process -Wait 可能继续等待 MSVC/vcpkg 派生出的驻留服务进程。
    # WaitForExit() 只等待本次直接启动的 cmd.exe，命令结束后即可继续下一阶段。
    $process.WaitForExit()
    $exitCode = [int]$process.ExitCode
    $process.Dispose()
    return $exitCode
}


function Build-Configuration {
    param(
        [hashtable]$Config,
        [int]$Index,
        [int]$Total
    )

    $name   = $Config.Name
    $preset = $Config.Preset
    $arch   = $Config.Arch

    Write-Title "[$Index/$Total] $name"

    Write-Host "Preset : $preset"
    Write-Host "Arch   : $arch"
    Write-Host ""


    # ========================================================
    # Configure
    # ========================================================

    Write-Host "[$Index/$Total] 正在配置 $name ..." -ForegroundColor Cyan
    Write-Host ""

    # 始终重新探测编译器输出编码，防止旧缓存中的 /showIncludes 前缀与当前代码页不一致。
    # --fresh 只重建 CMake 配置缓存，不删除已经生成的对象文件和安装目录。
    $configureCommand = "cmake --preset `"$preset`" --fresh"

    $exitCode = Invoke-MsvcCommand `
        -Arch $arch `
        -Command $configureCommand

    if ($exitCode -ne 0) {
        Stop-WithError "$name 配置失败，退出代码：$exitCode"
    }

    Write-Host ""
    Write-Host "[$Index/$Total] $name 配置完成。" -ForegroundColor Green
    Write-Host ""


    # ========================================================
    # Build
    # ========================================================

    Write-Host "[$Index/$Total] 正在构建 $name ..." -ForegroundColor Cyan
    Write-Host ""

    $buildCommand = "cmake --build --preset `"$preset`""

    if ($Clean) {
        $buildCommand += " --clean-first"
    }

    $exitCode = Invoke-MsvcCommand `
        -Arch $arch `
        -Command $buildCommand

    if ($exitCode -ne 0) {
        Stop-WithError "$name 构建失败，退出代码：$exitCode"
    }

    Write-Host ""
    Write-Host "[$Index/$Total] $name 构建完成。" -ForegroundColor Green
    Write-Host ""


    # ========================================================
    # Install
    # ========================================================

    Write-Host "[$Index/$Total] 正在安装 $name ..." -ForegroundColor Cyan
    Write-Host ""

    $installCommand = "cmake --build --preset `"$preset`" --target install"

    $exitCode = Invoke-MsvcCommand `
        -Arch $arch `
        -Command $installCommand

    if ($exitCode -ne 0) {
        Stop-WithError "$name 安装失败，退出代码：$exitCode"
    }

    Write-Host ""
    Write-Host "[$Index/$Total] $name 安装完成。" -ForegroundColor Green
    Write-Host ""
}


# ============================================================
# 主程序
# ============================================================

Clear-Host

Write-Title "ytpp_cpp_lib - Build All"

Write-Host "项目目录：$ProjectRoot"
Write-Host "MSVC环境 ：$VcVarsAll"
Write-Host "配置数量：$($BuildConfigs.Count)"
Write-Host ""

if ($Clean) {
    Write-Host "Clean First：ON" -ForegroundColor Yellow
}
else {
    Write-Host "Clean First：OFF"
}

Write-Host "Install    ：ON" -ForegroundColor Yellow
Write-Host ""


# ============================================================
# 基础检查
# ============================================================

if (-not (Test-Path $ProjectRoot)) {
    Stop-WithError "项目目录不存在：$ProjectRoot"
}

if (-not (Test-Path $VcVarsAll)) {
    Stop-WithError "找不到 vcvarsall.bat：$VcVarsAll"
}

if (-not (Test-Path (Join-Path $ProjectRoot "CMakeLists.txt"))) {
    Stop-WithError "找不到 CMakeLists.txt：$ProjectRoot"
}

if (-not (Test-Path (Join-Path $ProjectRoot "CMakePresets.json"))) {
    Stop-WithError "找不到 CMakePresets.json：$ProjectRoot"
}

if (-not (Test-Path (Join-Path $ProjectRoot "CMakeUserPresets.json"))) {
    Stop-WithError "找不到 CMakeUserPresets.json：$ProjectRoot"
}


# ============================================================
# 切换到项目目录
# ============================================================

Set-Location $ProjectRoot


# ============================================================
# 检查 cmake
# ============================================================

try {
    $cmakeCommand = Get-Command cmake -ErrorAction Stop
}
catch {
    Stop-WithError "找不到 cmake.exe，请确认 CMake 已加入 PATH。"
}

Write-Host "CMake     ：$($cmakeCommand.Source)"
Write-Host ""


# ============================================================
# 开始构建
# ============================================================

$startTime = Get-Date
$total = $BuildConfigs.Count

for ($i = 0; $i -lt $total; $i++) {
    Build-Configuration `
        -Config $BuildConfigs[$i] `
        -Index ($i + 1) `
        -Total $total
}


# ============================================================
# 完成
# ============================================================

$endTime = Get-Date
$elapsed = $endTime - $startTime

Write-Title "全部构建并安装成功"

Write-Host "已成功完成以下配置：" -ForegroundColor Green
Write-Host ""

foreach ($config in $BuildConfigs) {
    Write-Host "  [OK] $($config.Name)" -ForegroundColor Green
}

Write-Host ""
Write-Host ("总耗时：{0:hh\:mm\:ss}" -f $elapsed)
Write-Host ""

if ($Clean) {
    Write-Host "执行模式：Configure + Clean Build + Install" -ForegroundColor Green
}
else {
    Write-Host "执行模式：Configure + Build + Install" -ForegroundColor Green
}

Write-Host ""
Write-Host "全部任务已成功完成。" -ForegroundColor Green
Write-Host ""

Read-Host "按 Enter 键关闭窗口"
