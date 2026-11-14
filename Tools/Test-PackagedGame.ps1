[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,
    [switch]$Render
)

$ErrorActionPreference = 'Stop'
$package = (Resolve-Path -LiteralPath $PackageRoot).Path
$game = Join-Path $package 'MegaJamProject\Binaries\Win64\MegaJamProject.exe'
if (-not (Test-Path -LiteralPath $game)) {
    throw "Expected the packaged Windows directory; executable missing: $game"
}

$projectRoot = Split-Path $PSScriptRoot -Parent
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
$runDir = Join-Path $projectRoot "Saved\PackagedSmoke\$runId"
New-Item -ItemType Directory -Path $runDir -Force | Out-Null
$logPath = Join-Path $runDir 'Game.log'
$cachePath = Join-Path $runDir 'Cache'

# Keep diagnostics out of the player's script cache. Benchmark mode gives the
# startup/teardown regression a fixed frame budget; this is not a gameplay test.
$gameArguments = @(
    '-Unattended', '-NoSound', '-benchmark', '-seconds=3', '-fps=30',
    "-ABSLOG=$logPath", "-as-cache-root=$cachePath"
)
if ($Render) {
    $gameArguments += @('-RenderOffscreen', '-ResX=1280', '-ResY=720')
} else {
    $gameArguments += '-NullRHI'
}

# Start-Process joins ArgumentList before passing it to Windows; quote paths
# explicitly so a checkout or package path containing spaces still works.
$quotedArguments = $gameArguments | ForEach-Object { '"' + $_ + '"' }
$process = Start-Process -FilePath $game -ArgumentList $quotedArguments `
    -WorkingDirectory (Split-Path $game -Parent) -WindowStyle Hidden -PassThru
try {
    if (-not $process.WaitForExit(60000)) {
        # Only stop the diagnostic child created by this invocation.
        $process.Kill()
        throw "Packaged smoke timed out after 60 seconds. Log: $logPath"
    }
    if ($process.ExitCode -ne 0) {
        throw "Packaged game returned exit code $($process.ExitCode). Log: $logPath"
    }
} finally {
    $process.Dispose()
}

$logText = Get-Content -LiteralPath $logPath -Raw
foreach ($required in @('published=1', "Game class is 'ABackstageGameMode'", 'Object subsystem successfully closed.')) {
    if (-not $logText.Contains($required)) {
        throw "Required startup/shutdown marker missing: $required. Log: $logPath"
    }
}
if ($logText -match 'Angelscript: Error:|Fatal error:|Assertion failed:|Ensure condition failed:') {
    throw "Packaged game logged an error or assertion. Log: $logPath"
}

[pscustomobject]@{
    Package = $package
    Mode = $(if ($Render) { 'Rendered' } else { 'NullRHI' })
    ExitCode = 0
    Result = 'PASS: AngelScript initialized, Backstage GameMode loaded, shutdown completed'
    Log = $logPath
}
