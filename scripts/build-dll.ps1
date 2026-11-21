param([switch]$Validation,[switch]$Package)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$msbuild=@(& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe')[0]
if(!$msbuild){throw 'Install Visual Studio 2022 with Desktop development with C++.'}
$output=Join-Path $repoRoot 'build/dll/'
Push-Location $repoRoot
try {
    & $msbuild 'DllEditor/PalleteEditor/PaletteEditorDLL.vcxproj' /p:Configuration=Release /p:Platform=Win32 "/p:OutDir=$output" "/p:IntDir=$repoRoot/build/dll-obj/" "/p:PaletteValidation=$Validation" /m /v:minimal
    if($LASTEXITCODE -ne 0){throw 'DLL build failed.'}
    Copy-Item -LiteralPath 'PalleteEditor/groups.json' -Destination $output -ErrorAction SilentlyContinue
    if(!(Test-Path "$output/groups.json")){Copy-Item -LiteralPath 'PalleteEditor/data01/3.7.14.json' -Destination "$output/groups.json"}
    Copy-Item -LiteralPath 'PalleteEditor/data01/overlay-textures.json' -Destination $output
    if($Package){
        if($Validation){throw 'Validation builds must not be packaged.'}
        New-Item -ItemType Directory -Force -Path 'dist'|Out-Null
        Copy-Item -LiteralPath 'DllEditor/README.md' -Destination "$output/README.md"
        Copy-Item -LiteralPath 'docs/PAL-FORMAT.md' -Destination $output
        $manifest=[ordered]@{sourceCommit=(& git rev-parse HEAD).Trim();workingTreeDirty=[bool](& git status --porcelain);gameBuild=26588330;dllSha256=(Get-FileHash "$output/d3d9.dll").Hash}
        $manifest|ConvertTo-Json|Set-Content "$output/build-manifest.json"
        $files=@('d3d9.dll','groups.json','overlay-textures.json','README.md','PAL-FORMAT.md','build-manifest.json')|ForEach-Object {Join-Path $output $_}
        Compress-Archive -LiteralPath $files -DestinationPath 'dist/PaletteEditor-DLL-1.0.0-rc.1.zip' -Force
    }
}finally{Pop-Location}
