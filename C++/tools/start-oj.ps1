[CmdletBinding()]
param(
    [string]$Distribution = 'Ubuntu-22.04',
    [switch]$NoBrowser,
    [switch]$Stop,
    [switch]$Check
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$taskUtf8 = New-Object System.Text.UTF8Encoding($false)
[Console]::OutputEncoding = $taskUtf8
$OutputEncoding = $taskUtf8
# This applies to this launcher process only; do not change machine settings.
$env:WSL_UTF8 = '1'

# Use argument arrays so Chinese paths, spaces and apostrophes stay literal.
function Invoke-WslProbe {
    param([string[]]$TaskArguments)
    $taskPreviousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $taskLines = @(& $script:TaskWsl @TaskArguments 2>&1)
        $taskCode = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $taskPreviousPreference }
    $taskText = @($taskLines | ForEach-Object { $_.ToString().Replace([string][char]0, '').Trim() } | Where-Object { $_ })
    return [pscustomobject]@{ ExitCode = $taskCode; Lines = $taskText }
}

function Get-LocalResponseBytes {
    param([Uri]$TaskUrl)
    $taskRequest = [Net.HttpWebRequest]::Create($TaskUrl)
    $taskRequest.Proxy = $null
    $taskRequest.AllowAutoRedirect = $false
    $taskRequest.Timeout = 2500
    $taskRequest.ReadWriteTimeout = 2500
    $taskResponse = $null
    $taskMemory = New-Object IO.MemoryStream
    try {
        $taskResponse = $taskRequest.GetResponse()
        if ([int]$taskResponse.StatusCode -ne 200) { throw '本地网页没有返回成功状态。' }
        $taskResponse.GetResponseStream().CopyTo($taskMemory)
        return ,$taskMemory.ToArray()
    }
    finally {
        if ($taskResponse) { $taskResponse.Dispose() }
        $taskMemory.Dispose()
    }
}

try {
    if ($Stop -and $Check) { throw '停止和检查参数不能同时使用。' }
    $taskProject = Split-Path -Parent $PSScriptRoot
    $script:TaskWsl = (Get-Command wsl.exe -ErrorAction Stop).Source
    Write-Host ''
    $taskTitle = 'CodeOJ 一键启动'
    if ($Stop) { $taskTitle = 'CodeOJ 停止服务' }
    elseif ($Check) { $taskTitle = 'CodeOJ 状态检查' }
    Write-Host $taskTitle -ForegroundColor Cyan
    Write-Host ('项目目录：' + $taskProject)

    $taskDistros = Invoke-WslProbe -TaskArguments @('--list', '--quiet')
    if ($taskDistros.ExitCode -ne 0) {
        throw ('无法读取 WSL 环境。请先完成 WSL 安装。' + [Environment]::NewLine + ($taskDistros.Lines -join [Environment]::NewLine))
    }
    if ($taskDistros.Lines -notcontains $Distribution) {
        throw ('没有找到 ' + $Distribution + '。首次使用请运行项目中的“1-双击安装WSL.bat”。已有的其他发行版不会被删除。')
    }

    $taskPathResult = Invoke-WslProbe -TaskArguments @('-d', $Distribution, '--exec', 'wslpath', '-a', '-u', $taskProject)
    $taskLinuxPaths = @($taskPathResult.Lines | Where-Object { $_.StartsWith('/') })
    if ($taskPathResult.ExitCode -ne 0 -or $taskLinuxPaths.Count -ne 1) {
        throw '无法将当前项目路径转换为 WSL 路径。请把项目保存在可由 WSL 访问的本地磁盘中。'
    }
    $taskLinuxProject = $taskLinuxPaths[0]

    if (-not $Stop -and -not $Check) {
        Write-Host '正在启动数据库，无需输入 Ubuntu 密码……'
        $taskDatabase = Invoke-WslProbe -TaskArguments @('-d', $Distribution, '-u', 'root', '--exec', '/usr/sbin/service', 'mysql', 'start')
        if ($taskDatabase.ExitCode -ne 0) {
            throw ('MySQL 启动失败。' + [Environment]::NewLine + ($taskDatabase.Lines -join [Environment]::NewLine))
        }
    }

    $taskEngineArguments = @('-d', $Distribution, '--exec', 'env', 'PYTHONUNBUFFERED=1', 'PYTHONIOENCODING=utf-8', 'python3', ($taskLinuxProject + '/tools/start_oj.py'), '--source', $taskLinuxProject)
    if ($Stop) { $taskEngineArguments += '--stop' }
    if ($Check) { $taskEngineArguments += '--check' }
    $taskReadyUrl = $null
    $taskPreviousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $script:TaskWsl @taskEngineArguments 2>&1 | ForEach-Object {
            $taskLine = $_.ToString().Replace([string][char]0, '').TrimEnd()
            if ($taskLine.StartsWith('OJ_READY=')) {
                $taskReadyUrl = $taskLine.Substring('OJ_READY='.Length)
            }
            elseif ($taskLine.StartsWith('wsl:') -and $taskLine.Contains('localhost')) {
                # WSL's unrelated NAT proxy notice should not obscure OJ status.
            }
            elseif ($taskLine) { Write-Host $taskLine }
        }
        $taskEngineExit = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $taskPreviousPreference }
    if ($taskEngineExit -ne 0) { throw ('操作未完成，退出码 ' + $taskEngineExit + '。请查看上方具体原因。') }
    if ($Stop) {
        Write-Host '本项目服务已停止，数据库保持运行。' -ForegroundColor Green
        exit 0
    }
    if (-not $taskReadyUrl) { throw '服务尚未通过检查，没有打开网页。请查看上方日志。' }
    [Uri]$taskUri = $null
    if (-not [Uri]::TryCreate($taskReadyUrl, [UriKind]::Absolute, [ref]$taskUri) -or $taskUri.Scheme -ne 'http' -or $taskUri.Host -ne 'localhost') {
        throw '启动结果包含无效的本地服务地址。'
    }
    $taskWindowsReady = $false
    $taskWindowsFailure = ''
    # Use .NET directly: batch-launched Windows PowerShell can inherit a
    # different PSModulePath from VS Code/PowerShell 7.
    $taskSourceHasher = [Security.Cryptography.SHA256]::Create()
    try {
        $taskSourceBytes = [IO.File]::ReadAllBytes((Join-Path $taskProject 'public\css\style.css'))
        $taskExpectedCss = [BitConverter]::ToString($taskSourceHasher.ComputeHash($taskSourceBytes)).Replace('-', '').ToLowerInvariant()
    }
    finally { $taskSourceHasher.Dispose() }
    for ($taskAttempt = 0; $taskAttempt -lt 8; $taskAttempt++) {
        try {
            $taskLoginBytes = Get-LocalResponseBytes -TaskUrl $taskUri
            if (-not ([Text.Encoding]::UTF8.GetString($taskLoginBytes)).Contains('CodeOJ')) { throw '本地端口返回了其他网页。' }
            $taskCssUri = New-Object Uri($taskUri, ('/css/style.css?v=' + $taskExpectedCss.Substring(0, 16)))
            $taskCssBytes = Get-LocalResponseBytes -TaskUrl $taskCssUri
            $taskHasher = [Security.Cryptography.SHA256]::Create()
            try { $taskActualCss = [BitConverter]::ToString($taskHasher.ComputeHash($taskCssBytes)).Replace('-', '').ToLowerInvariant() }
            finally { $taskHasher.Dispose() }
            if ($taskActualCss -ne $taskExpectedCss) { throw '本地网页仍返回其他版本的样式。' }
            $taskWindowsReady = $true
            break
        }
        catch { $taskWindowsFailure = $_.Exception.Message; Start-Sleep -Milliseconds 300 }
    }
    if (-not $taskWindowsReady) {
        throw ('WSL 服务已启动，但 Windows 本地地址或样式版本检查未通过，因此没有打开网页。原因：' + $taskWindowsFailure)
    }
    Write-Host ''
    Write-Host ('服务就绪：' + $taskReadyUrl) -ForegroundColor Green
    if (-not $NoBrowser -and -not $Check) {
        Start-Process -FilePath $taskReadyUrl
    }
    Write-Host '服务在后台运行，可以关闭此窗口。需要停止时双击“停止OJ.bat”。'
    exit 0
}
catch {
    Write-Host ''
    Write-Host ('[错误] ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
