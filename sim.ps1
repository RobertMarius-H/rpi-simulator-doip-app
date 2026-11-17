# Wrapper Windows pentru ./sim.sh: rulează comanda în WSL din folderul acestui proiect.
# Exemple:  .\sim.ps1 up   |   .\sim.ps1 logs   |   .\sim.ps1 down
# Distribuția WSL se poate schimba cu -Distro (implicit: distribuția WSL implicită).
param(
    [Parameter(Position = 0)] [string] $Command = "help",
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)] [string[]] $Rest = @(),
    [string] $Distro = ""
)

$ErrorActionPreference = "Stop"
$distroArgs = @()
if ($Distro -ne "") { $distroArgs = @("-d", $Distro) }

if ($Command -eq "test") {
    # Testul rulează direct din Windows, exact ca aplicația RpiDiag.
    $target = if ($Rest.Count -ge 1) { $Rest[0] } else { "127.0.0.1" }
    $port = if ($Rest.Count -ge 2) { $Rest[1] } else { "13400" }
    python "$PSScriptRoot\tools\doip_smoke_test.py" $target $port
    exit $LASTEXITCODE
}

$windowsPath = $PSScriptRoot -replace '\\', '/'
$linuxPath = (& wsl.exe @distroArgs wslpath -a "$windowsPath").Trim()
$arguments = (@($Command) + $Rest) -join " "
& wsl.exe @distroArgs bash -lc "cd '$linuxPath' && bash ./sim.sh $arguments"
exit $LASTEXITCODE
