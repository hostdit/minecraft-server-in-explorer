Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1
$data = Join-Path $env:LOCALAPPDATA "McExplorer"
Remove-Item (Join-Path $data "world.bin*") -ErrorAction SilentlyContinue
if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) { Start-Process explorer.exe }
"World reset. The next server start gets a fresh flat world."
