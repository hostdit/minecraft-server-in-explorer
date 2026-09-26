param([Parameter(Mandatory)][string]$Dll)
$ErrorActionPreference = "Stop"
$path = (Resolve-Path $Dll).Path
$result = Start-Process regsvr32 -ArgumentList "/s `"$path`"" -Wait -PassThru
if ($result.ExitCode) { throw "regsvr32 failed with $($result.ExitCode)" }
"Registered $path"
