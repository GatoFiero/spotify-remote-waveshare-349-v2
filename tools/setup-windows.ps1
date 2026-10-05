param(
    [string]$Port = '',
    [string]$Python = 'python',
    [ValidateSet('rev2','rev1')][string]$Environment = 'rev2',
    [switch]$SkipBootCheck
)
$ErrorActionPreference = 'Stop'
$env:PYTHONIOENCODING = 'utf-8'
$taskProject = Split-Path $PSScriptRoot -Parent
$taskPython = (Get-Command $Python -ErrorAction Stop).Source
Push-Location $taskProject
try {
    & $taskPython -c 'import platformio, serial, esptool'
    if ($LASTEXITCODE -ne 0) { throw 'Install the tools with: python -m pip install -r requirements.txt' }
    if (!(Test-Path -LiteralPath 'src/Secrets.h')) {
        $taskListener = Get-NetTCPConnection -LocalPort 8888 -State Listen -ErrorAction SilentlyContinue
        if (!$taskListener) {
            Start-Process -FilePath $taskPython -ArgumentList 'tools/local_setup.py' -WorkingDirectory $taskProject -WindowStyle Hidden
        }
        Start-Process 'http://127.0.0.1:8888/'
        Write-Host 'Complete Spotify and Wi-Fi setup in your browser. Keep the board plugged in.'
        while (!(Test-Path -LiteralPath 'src/Secrets.h')) { Start-Sleep -Seconds 2 }
    }
    & $taskPython -m platformio run -e $Environment
    if ($LASTEXITCODE -ne 0) { throw 'Build failed; firmware was not uploaded.' }
    $taskUploadArgs = @('-m','platformio','run','-e',$Environment,'-t','upload')
    if ($Port) { $taskUploadArgs += @('--upload-port',$Port) }
    & $taskPython @taskUploadArgs
    if ($LASTEXITCODE -ne 0) { throw 'Upload failed; check the data cable and selected serial port.' }
    if (!$SkipBootCheck -and $Port) {
        & $taskPython tools/check_boot.py --port $Port
        if ($LASTEXITCODE -ne 0) { throw 'Boot diagnostics failed.' }
    }
} finally { Pop-Location }
