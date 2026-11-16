# Builds AxelRuiz-Sombras in %TEMP%, zips it to C:\Users\cereBRO\Modding and deletes the build tree.
# Usage (PowerShell): .\build.ps1 -Src <folder with this project> [-CLib <existing CommonLibF4VR checkout at 9057052>] [-CleanSrc]
param([Parameter(Mandatory)][string]$Src, [string]$Out = "C:\Users\cereBRO\Modding", [string]$CLib = "", [switch]$CleanSrc)
$ErrorActionPreference = "Stop"
$Version = "0.1.1"
$W = Join-Path $env:TEMP "sombras"
if (Test-Path $W) { Remove-Item $W -Recurse -Force }
Copy-Item $Src $W -Recurse
if ($CleanSrc) { Remove-Item $Src -Recurse -Force }
if (-not $env:VCPKG_ROOT) { $env:VCPKG_ROOT = "E:\General\Lifestyle\Modding\Fallout 4\VR Version\Herramientas\vcpkg" }
if (-not $CLib -and (Test-Path "$W\lib\CommonLibF4VR\CommonLibF4\include\REL\Relocation.h")) {
  # submodule checked out with the source
} elseif ($CLib) {
  New-Item -ItemType Directory "$W\lib" -Force | Out-Null
  robocopy $CLib "$W\lib\CommonLibF4VR" /E /XD .git /NFL /NDL /NJH /NJS | Out-Null
} else {
  git clone -q https://github.com/ArthurHub/CommonLibF4VR.git "$W\lib\CommonLibF4VR"
  git -C "$W\lib\CommonLibF4VR" checkout -q 9057052
}
cmake -S $W -B "$W\build" -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE) { throw "configure failed" }
cmake --build "$W\build" --config Release -- /m /v:m
if ($LASTEXITCODE) { throw "build failed" }
$dll = "$W\build\Release\AxelRuizSombras.dll"
$zip = Join-Path $Out "AxelRuiz-Sombras-$Version.zip"
Get-ChildItem $Out -Filter "AxelRuiz-Sombras-*.zip" | Remove-Item  # keep only the latest zip
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$z = [System.IO.Compression.ZipFile]::Open($zip, "Create")
$files = @{
  "F4SE/Plugins/AxelRuizSombras.dll" = $dll
  "F4SE/Plugins/AxelRuizSombras.pdb" = "$W\build\Release\AxelRuizSombras.pdb"
  "F4SE/Plugins/AxelRuizSombras.ini" = "$W\data\F4SE\Plugins\AxelRuizSombras.ini"
  "F4SE/Plugins/AxelRuizSombras_LICENSE.txt" = "$W\LICENSE"
  "F4SE/Plugins/AxelRuizSombras_README.txt" = "$W\README.md"
}
# GPL: the README links the public source; no nested archives (Nexus quarantines them).
foreach ($k in $files.Keys) { [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($z, $files[$k], $k) }
$z.Dispose()
Get-Item $dll, $zip | Format-Table Name, Length
Get-FileHash $zip -Algorithm SHA256 | Format-List
Set-Location $env:TEMP
Remove-Item $W -Recurse -Force
