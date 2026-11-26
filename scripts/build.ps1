param([switch]$Package,[string]$Version='1.0.1')
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if(!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022 with Desktop development with C++ and the Windows SDK.' }
$msbuild=@(& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe')[0]
if(!$msbuild) { throw 'MSBuild was not found.' }
$appOutput=Join-Path $repoRoot 'build/app/'
$testOutput=Join-Path $repoRoot 'build/tests/'
Push-Location $repoRoot
try {
    & $msbuild 'PalleteEditor/ImGui Standalone.vcxproj' /p:Configuration=Release /p:Platform=Win32 /p:TargetName=PaletteEditor-V1 "/p:OutDir=$appOutput" "/p:IntDir=$repoRoot/build/app-obj/" /m /v:minimal
    if($LASTEXITCODE -ne 0) { throw 'Palette editor build failed.' }
    & $msbuild 'tests/PaletteDocumentTests.vcxproj' /p:Configuration=Release /p:Platform=Win32 "/p:OutDir=$testOutput" "/p:IntDir=$repoRoot/build/test-obj/" /m /v:minimal
    if($LASTEXITCODE -ne 0) { throw 'Palette format test build failed.' }
    Push-Location $testOutput
    try {
        & (Join-Path $testOutput 'PaletteDocumentTests.exe')
        if($LASTEXITCODE -ne 0) { throw 'Palette format tests failed.' }
    } finally { Pop-Location }
    & $msbuild 'tests/NativeRuleRuntimeTests.vcxproj' /p:Configuration=Release /p:Platform=Win32 "/p:OutDir=$testOutput" "/p:IntDir=$repoRoot/build/native-rule-test-obj/" /m /v:minimal
    if($LASTEXITCODE -ne 0) { throw 'Native rule runtime test build failed.' }
    & (Join-Path $testOutput 'NativeRuleRuntimeTests.exe')
    if($LASTEXITCODE -ne 0) { throw 'Native rule runtime tests failed.' }
    & $msbuild 'tests/MoveRouteTrackerTests.vcxproj' /p:Configuration=Release /p:Platform=Win32 "/p:OutDir=$testOutput" "/p:IntDir=$repoRoot/build/move-route-test-obj/" /m /v:minimal
    if($LASTEXITCODE -ne 0) { throw 'Move route tracker test build failed.' }
    & (Join-Path $testOutput 'MoveRouteTrackerTests.exe')
    if($LASTEXITCODE -ne 0) { throw 'Move route tracker tests failed.' }
    if($Package) {
        if($Version -notmatch '^\d+\.\d+\.\d+(-[A-Za-z0-9.-]+)?$'){throw 'Invalid package version.'}
        $stage=Join-Path $repoRoot "dist/PaletteEditor-$version"
        New-Item -ItemType Directory -Force -Path $stage | Out-Null
        $files=@('PaletteEditor-V1.exe','groups.json','overlay-textures.json')
        foreach($file in $files) { Copy-Item -LiteralPath (Join-Path $appOutput $file) -Destination $stage }
        Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination $stage
        Copy-Item -LiteralPath (Join-Path $repoRoot 'docs/PAL-FORMAT.md') -Destination $stage
        $revision=(& git rev-parse HEAD).Trim()
        $manifest=[ordered]@{version=$version;sourceCommit=$revision;workingTreeDirty=[bool](& git status --porcelain);gameBuild=26588330;executableSha256=(Get-FileHash (Join-Path $stage 'PaletteEditor-V1.exe')).Hash}
        $manifest | ConvertTo-Json | Set-Content (Join-Path $stage 'build-manifest.json')
        $releaseFiles=@($files+@('README.md','PAL-FORMAT.md','build-manifest.json') | ForEach-Object { Join-Path $stage $_ })
        $zip=Join-Path $repoRoot "dist/PaletteEditor-$version.zip"
        Compress-Archive -LiteralPath $releaseFiles -DestinationPath $zip -Force
        Write-Output "Created $zip"
    }
} finally { Pop-Location }
