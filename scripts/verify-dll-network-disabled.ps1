param([Parameter(Mandatory=$true)][string]$DllPath)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
[xml]$project=Get-Content -LiteralPath (Join-Path $repoRoot 'DllEditor/PalleteEditor/PaletteEditorDLL.vcxproj') -Raw
$ns=New-Object Xml.XmlNamespaceManager($project.NameTable)
$ns.AddNamespace('msb','http://schemas.microsoft.com/developer/msbuild/2003')
$compiled=@($project.SelectNodes('//msb:ItemGroup/msb:ClCompile[@Include]',$ns)|ForEach-Object {$_.Include})
foreach($file in @('NetworkingPalette.cpp','SteamLogger.cpp')) {
    if($compiled -contains $file){throw "Disabled network source was included in the DLL build: $file"}
}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
$toolset=Get-ChildItem -LiteralPath (Join-Path $vs 'VC/Tools/MSVC') -Directory|Sort-Object Name -Descending|Select-Object -First 1
$dumpbin=Join-Path $toolset.FullName 'bin/Hostx64/x86/dumpbin.exe'
$imports=(& $dumpbin /imports $DllPath | Out-String)
if($LASTEXITCODE -ne 0){throw 'Could not inspect DLL imports.'}
if($imports -match '(?i)steam_api(?:64)?\.dll|SteamAPI_'){throw 'DLL still imports the Steam API.'}
$bytes=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $DllPath))
$text=[Text.Encoding]::ASCII.GetString($bytes)
foreach($marker in @('PAL_HASH','PAL_REQ')) {
    if($text.Contains($marker)){throw "Legacy palette protocol remains in the DLL: $marker"}
}
[pscustomobject]@{paletteNetworkingCompiled=$false;steamLobbyToolsCompiled=$false;steamApiImportsPresent=$false;legacyPaletteProtocolMarkersPresent=$false}
