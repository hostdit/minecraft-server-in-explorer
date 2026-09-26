$ErrorActionPreference = "Stop"
$arch = if ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture -eq "Arm64") { "arm64" } else { "x64" }
$built = Join-Path $PSScriptRoot "..\build\$arch\shell\Release\McExplorer.dll"
if (-not (Test-Path $built)) { throw "No build at $built. Run cmake --build --preset windows-$arch first." }
$bin = Join-Path $env:LOCALAPPDATA "McExplorer\bin"
New-Item -ItemType Directory -Force $bin | Out-Null
$target = Join-Path $bin ("McExplorer-{0:yyyyMMdd-HHmmss}.dll" -f (Get-Date))
Copy-Item $built $target
& "$PSScriptRoot\register.ps1" $target
& "$PSScriptRoot\restart-explorer.ps1"
Get-ChildItem $bin -Filter "McExplorer*.dll" | Where-Object FullName -ne $target | ForEach-Object { try { Remove-Item $_.FullName -ErrorAction Stop } catch {} }
"Deployed $arch build. Open This PC > Minecraft Server to start the server."
