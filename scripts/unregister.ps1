$ErrorActionPreference = "Stop"
$clsid = "{A8DBB7AA-9EA6-47E3-ADE7-8615EDA229C7}"
Remove-Item "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\MyComputer\NameSpace\$clsid" -Recurse -ErrorAction SilentlyContinue
Remove-Item "HKCU:\Software\Classes\CLSID\$clsid" -Recurse -ErrorAction SilentlyContinue
"Unregistered. Restart Explorer to unload the DLL."
