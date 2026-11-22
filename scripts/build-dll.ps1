param([switch]$Validation,[switch]$Package,[switch]$RequireOverlayShader,[string]$Version='1.0.1')
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
    & "$PSScriptRoot/verify-dll-network-disabled.ps1" -DllPath "$output/d3d9.dll"
    Copy-Item -LiteralPath 'PalleteEditor/data01/3.7.14.json' -Destination "$output/groups.json"
    Copy-Item -LiteralPath 'PalleteEditor/data01/overlay-textures.json' -Destination $output
    if($Package -or $RequireOverlayShader){
        $shader=Join-Path $repoRoot 'build/overlay-shader/prototype.bin'
        $shaderHash='94B647CE2A72EE3A42A440DE46337CD47E66F1CA481C1F7E7E37E532D582A72F'
        if(!(Test-Path -LiteralPath $shader) -or (Get-FileHash -LiteralPath $shader).Hash -ne $shaderHash) {
            throw 'Build the tested overlay shader with scripts/build-overlay-preview.ps1 before packaging.'
        }
        $shaderDirectory=Join-Path $output 'overlay-shader'
        New-Item -ItemType Directory -Force -Path $shaderDirectory|Out-Null
        Copy-Item -LiteralPath $shader -Destination $shaderDirectory
    }
    if($Package){
        if($Version -notmatch '^\d+\.\d+\.\d+(-[A-Za-z0-9.-]+)?$'){throw 'Invalid package version.'}
        if($Validation){throw 'Validation builds must not be packaged.'}
        New-Item -ItemType Directory -Force -Path 'dist'|Out-Null
        Copy-Item -LiteralPath 'DllEditor/README.md' -Destination "$output/README.md"
        Copy-Item -LiteralPath 'docs/PAL-FORMAT.md' -Destination $output
        $manifest=[ordered]@{version=$Version;sourceCommit=(& git rev-parse HEAD).Trim();workingTreeDirty=[bool](& git status --porcelain);gameBuild=26588330;dllSha256=(Get-FileHash "$output/d3d9.dll").Hash;overlayShaderSha256=$shaderHash}
        $manifest|ConvertTo-Json|Set-Content "$output/build-manifest.json"
        $files=@('d3d9.dll','groups.json','overlay-textures.json','README.md','PAL-FORMAT.md','build-manifest.json','overlay-shader')|ForEach-Object {Join-Path $output $_}
        Compress-Archive -LiteralPath $files -DestinationPath "dist/PaletteEditor-DLL-$Version.zip" -Force
    }
}finally{Pop-Location}
