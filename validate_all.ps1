param(
    [string]$Vmrun = "C:\Program Files (x86)\VMware\VMware Workstation\vmrun.exe",
    [string]$Vmx = "D:\document\RL\学生资料\01.开发环境\Ubuntu2204Bit\BitDev.vmx",
    [string]$GuestUser = "bit",
    [string]$GuestPass = "123456",
    [switch]$SkipPackage,
    [switch]$NoStart,
    [switch]$OpenConsole
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path

$ClientDir = Join-Path $Root "ChargingUserClient"
$ServerDir = Join-Path $Root "EVChargingServer"
$ClientZip = Join-Path $Root "ChargingUserClient_用户端.zip"
$ServerZip = Join-Path $Root "EVChargingServer.zip"
$GuestScript = Join-Path $Root "guest_validate_charging.sh"
$SmokePro = Join-Path $Root "socket_smoke.pro"
$SmokeCpp = Join-Path $Root "socket_smoke.cpp"

$ResultFiles = @(
    @{ Guest = "/home/bit/charging_validation/summary.txt"; Local = "validate_summary.txt" },
    @{ Guest = "/home/bit/charging_validation/server_build.log"; Local = "validate_server_build.log" },
    @{ Guest = "/home/bit/charging_validation/client_build.log"; Local = "validate_client_build.log" },
    @{ Guest = "/home/bit/charging_validation/server_runtime.log"; Local = "validate_server_runtime.log" },
    @{ Guest = "/home/bit/charging_validation/process_status.txt"; Local = "validate_process_status.txt" },
    @{ Guest = "/home/bit/charging_validation/smoke_build.log"; Local = "validate_smoke_build.log" },
    @{ Guest = "/home/bit/charging_validation/smoke_result.txt"; Local = "validate_smoke_result.txt" }
)

function Write-Step($Message) {
    Write-Host "[STEP] $Message" -ForegroundColor Cyan
}

function Write-Ok($Message) {
    Write-Host "[ OK ] $Message" -ForegroundColor Green
}

function Write-WarnLine($Message) {
    Write-Host "[WARN] $Message" -ForegroundColor Yellow
}

function Invoke-Vmrun {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [switch]$AllowFail
    )

    & $Vmrun @Arguments
    $code = $LASTEXITCODE
    if ($code -ne 0 -and -not $AllowFail) {
        throw "vmrun failed with exit code ${code}: $($Arguments -join ' ')"
    }
    return $code
}

function Copy-ToGuest {
    param(
        [Parameter(Mandatory = $true)][string]$HostPath,
        [Parameter(Mandatory = $true)][string]$GuestPath
    )
    $copyArgs = @("-T", "ws", "-gu", $GuestUser, "-gp", $GuestPass,
        "CopyFileFromHostToGuest", $Vmx, $HostPath, $GuestPath)
    Invoke-Vmrun -Arguments $copyArgs | Out-Null
}

function Copy-FromGuest {
    param(
        [Parameter(Mandatory = $true)][string]$GuestPath,
        [Parameter(Mandatory = $true)][string]$HostPath
    )
    $copyArgs = @("-T", "ws", "-gu", $GuestUser, "-gp", $GuestPass,
        "CopyFileFromGuestToHost", $Vmx, $GuestPath, $HostPath)
    Invoke-Vmrun -Arguments $copyArgs -AllowFail | Out-Null
}

function Test-RequiredFile($Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "missing required file: $Path"
    }
}

Test-RequiredFile $Vmrun
Test-RequiredFile $Vmx
Test-RequiredFile $GuestScript
Test-RequiredFile $SmokePro
Test-RequiredFile $SmokeCpp

if (-not $SkipPackage) {
    Write-Step "刷新用户端和服务端源码压缩包"
    Test-RequiredFile $ClientDir
    Test-RequiredFile $ServerDir
    Compress-Archive -Path (Join-Path $ClientDir "*") -DestinationPath $ClientZip -Force
    Compress-Archive -Path (Join-Path $ServerDir "*") -DestinationPath $ServerZip -Force
    Write-Ok "压缩包已更新"
}

Test-RequiredFile $ClientZip
Test-RequiredFile $ServerZip

if (-not $NoStart) {
    Write-Step "检查并启动 Ubuntu 虚拟机"
    $running = & $Vmrun -T ws list
    if ($running -notcontains $Vmx) {
        $startArgs = @("-T", "ws", "start", $Vmx, "nogui")
        Invoke-Vmrun -Arguments $startArgs | Out-Null
        Start-Sleep -Seconds 12
    }

    $ready = $false
    for ($i = 0; $i -lt 18; $i++) {
        $probeArgs = @("-T", "ws", "-gu", $GuestUser, "-gp", $GuestPass,
            "RunProgramInGuest", $Vmx, "/bin/true")
        Invoke-Vmrun -Arguments $probeArgs -AllowFail | Out-Null
        if ($LASTEXITCODE -eq 0) {
            $ready = $true
            break
        }
        Start-Sleep -Seconds 5
    }
    if (-not $ready) {
        throw "Ubuntu guest is not ready for vmrun guest operations."
    }
    Write-Ok "虚拟机已就绪"
}

if ($OpenConsole) {
    Write-Step "打开 VMware 控制台窗口"
    $VmwareExe = Join-Path (Split-Path -Parent $Vmrun) "vmware.exe"
    if (Test-Path -LiteralPath $VmwareExe) {
        Start-Process -FilePath $VmwareExe -ArgumentList $Vmx
    } else {
        Write-WarnLine "未找到 vmware.exe，仅继续后台验证。"
    }
}

Write-Step "同步验证所需文件到 Ubuntu"
Copy-ToGuest $ClientZip "/home/bit/ChargingUserClient_UserClient_latest.zip"
Copy-ToGuest $ServerZip "/home/bit/EVChargingServer_latest.zip"
Copy-ToGuest $GuestScript "/home/bit/guest_validate_charging.sh"
Copy-ToGuest $SmokePro "/home/bit/socket_smoke.pro"
Copy-ToGuest $SmokeCpp "/home/bit/socket_smoke.cpp"
Write-Ok "文件同步完成"

Write-Step "在 Ubuntu 中编译工程并执行 socket 验证"
$guestRunArgs = @("-T", "ws", "-gu", $GuestUser, "-gp", $GuestPass,
    "RunProgramInGuest", $Vmx, "/bin/bash", "/home/bit/guest_validate_charging.sh")
$runCode = Invoke-Vmrun -Arguments $guestRunArgs -AllowFail

Write-Step "复制验证日志回当前文件夹"
foreach ($item in $ResultFiles) {
    $localPath = Join-Path $Root $item.Local
    Copy-FromGuest $item.Guest $localPath
}

$Summary = Join-Path $Root "validate_summary.txt"
if (Test-Path -LiteralPath $Summary) {
    Write-Host ""
    Get-Content -LiteralPath $Summary -Encoding UTF8
    Write-Host ""
}

if ($runCode -ne 0) {
    Write-WarnLine "验证失败，详见 validate_*.log 和 validate_summary.txt"
    exit $runCode
}

Write-Ok "验证完成：服务端 8888 监听、用户端协议登录/查询均通过。"


