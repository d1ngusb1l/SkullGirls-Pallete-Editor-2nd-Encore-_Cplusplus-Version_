param()
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    if(@(& git status --porcelain).Count){throw 'Commit or set aside source changes before preparing release assets.'}
    $revision=(& git rev-parse HEAD).Trim()
    $versionHeader=Get-Content -LiteralPath 'ReleaseVersion.h' -Raw
    if($versionHeader -notmatch '#define SGPE_VERSION "(\d+\.\d+\.\d+)"'){throw 'ReleaseVersion.h must contain a numeric release version.'}
    $version=$Matches[1]
    & "$PSScriptRoot/build.ps1" -Version $version
    & "$PSScriptRoot/build-dll.ps1" -Version $version -RequireOverlayShader
    if(@(& git status --porcelain).Count -or (& git rev-parse HEAD).Trim() -ne $revision){throw 'Source changed during the build.'}

    $output=Join-Path $repoRoot "dist/v$version"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $exeName='SkullGirls.Pallete.Editor.2nd.Encore.exe'
    $dllPath=Join-Path $repoRoot 'build/dll/d3d9.dll'
    $exePath=Join-Path $repoRoot 'build/app/PaletteEditor-V1.exe'
    foreach($binary in @($dllPath,$exePath)) {
        $bytes=[IO.File]::ReadAllBytes($binary)
        $pe=[BitConverter]::ToInt32($bytes,0x3c)
        if([BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x14c){throw "Expected a Win32 PE binary: $binary"}
        $info=[Diagnostics.FileVersionInfo]::GetVersionInfo($binary)
        if($info.ProductVersion -ne $version){throw "Binary product version mismatch: $binary"}
    }
    if([Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($dllPath)).Contains('dll-smoke.request')){throw 'DLL still contains the opt-in validation probe.'}
    $networkVerification=& "$PSScriptRoot/verify-dll-network-disabled.ps1" -DllPath $dllPath

    Copy-Item -LiteralPath $dllPath -Destination "$output/d3d9.dll"
    Copy-Item -LiteralPath $exePath -Destination (Join-Path $output $exeName)
    Copy-Item -LiteralPath 'PalleteEditor/data01/3.7.14.json' -Destination "$output/groups.json"
    Copy-Item -LiteralPath 'PalleteEditor/data01/3.7.14.json' -Destination "$output/3.7.14.json"
    Copy-Item -LiteralPath 'PalleteEditor/data01/overlay-textures.json' -Destination "$output/overlay-textures.json"
    Copy-Item -LiteralPath 'release-docs/RELEASE-NOTES.md' -Destination "$output/RELEASE-NOTES.md"

    $assets=@('d3d9.dll',$exeName,'groups.json','3.7.14.json','overlay-textures.json','RELEASE-NOTES.md')
    $packages=@()
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    foreach($variant in @('DLL','EXE')) {
        $stage=Join-Path $repoRoot "build/release-$version-$variant"
        New-Item -ItemType Directory -Force -Path $stage | Out-Null
        $binaryName=if($variant -eq 'DLL'){'d3d9.dll'}else{$exeName}
        foreach($file in @($binaryName,'groups.json','overlay-textures.json','RELEASE-NOTES.md')) {
            Copy-Item -LiteralPath (Join-Path $output $file) -Destination $stage
        }
        Copy-Item -LiteralPath "release-docs/INSTALL-$variant.md" -Destination "$stage/README.md"
        Copy-Item -LiteralPath 'docs/PAL-FORMAT.md' -Destination "$stage/PAL-FORMAT.md"
        Copy-Item -LiteralPath 'docs/UPSTREAM-README.md' -Destination "$stage/UPSTREAM-README.md"
        $manifest=[ordered]@{version=$version;variant=$variant;architecture='x86';configuration='Release';sourceCommit=$revision;workingTreeDirty=$false;gameBuild=26588330;binary=$binaryName;binarySha256=(Get-FileHash (Join-Path $stage $binaryName)).Hash;onlinePaletteSharingEnabled=$false;validationProbesEnabled=($variant -eq 'EXE');runtimeSignoffComplete=$false}
        $members=@($binaryName,'groups.json','overlay-textures.json','RELEASE-NOTES.md','README.md','PAL-FORMAT.md','UPSTREAM-README.md','build-manifest.json')
        $zipName="PaletteEditor-$variant-$version-win32.zip"
        $paths=@($members|ForEach-Object {Join-Path $stage $_})
        if($variant -eq 'DLL') {
            $shaderFolder=Join-Path $stage 'overlay-shader'
            New-Item -ItemType Directory -Force -Path $shaderFolder|Out-Null
            Copy-Item -LiteralPath 'build/dll/overlay-shader/prototype.bin' -Destination $shaderFolder
            $manifest.overlayShaderSha256=(Get-FileHash (Join-Path $shaderFolder 'prototype.bin')).Hash
            $members+='overlay-shader/prototype.bin'
            $paths+=$shaderFolder
        }
        $manifest | ConvertTo-Json | Set-Content -LiteralPath "$stage/build-manifest.json" -Encoding utf8
        Compress-Archive -LiteralPath $paths -DestinationPath (Join-Path $output $zipName) -Force
        $zip=[IO.Compression.ZipFile]::OpenRead((Join-Path $output $zipName))
        try {
            $actual=@($zip.Entries|ForEach-Object FullName)
            if(@(Compare-Object ($members|Sort-Object) ($actual|Sort-Object)).Count){throw "Unexpected package members in $zipName"}
            $packagedBinary=@($zip.Entries|Where-Object FullName -eq $binaryName)[0]
            $stream=$packagedBinary.Open()
            try {
                $sha=[Security.Cryptography.SHA256]::Create()
                try {$hash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}finally{$sha.Dispose()}
            }finally{$stream.Dispose()}
            if($hash -ne $manifest.binarySha256){throw "Packaged binary hash mismatch: $zipName"}
        }finally{$zip.Dispose()}
        $assets+=$zipName
        $packages+=$manifest
    }

    foreach($format in @('zip','tar.gz')) {
        $name="PaletteEditor-$version-source.$format"
        & git archive "--format=$format" "--prefix=PaletteEditor-$version/" "--output=$output/$name" $revision
        if($LASTEXITCODE -ne 0){throw "Source archive failed: $format"}
        $assets+=$name
    }
    $sourceZip=[IO.Compression.ZipFile]::OpenRead("$output/PaletteEditor-$version-source.zip")
    try {
        foreach($entry in $sourceZip.Entries) {
            if($entry.FullName -match '/(build|dist|\.git)/|/(PaletteEditorConfig|PaletteEditorDLLConfig)\.json$|/persistence-test-files/'){throw "Private/generated file in source archive: $($entry.FullName)"}
        }
        foreach($required in @('ReleaseVersion.h','scripts/prepare-release.ps1','release-docs/RELEASE-NOTES.md','PalleteEditor/ImGui Standalone.vcxproj','DllEditor/PalleteEditor/PaletteEditorDLL.vcxproj')) {
            if(!$sourceZip.GetEntry("PaletteEditor-$version/$required")){throw "Source archive missing $required"}
        }
    }finally{$sourceZip.Dispose()}

    $release=[ordered]@{version=$version;suggestedTag="v$version";sourceCommit=$revision;workingTreeDirty=$false;architecture='x86';configuration='Release';gameBuild=26588330;offlineChecksPassed=28;networkVerification=$networkVerification;runtimeSignoffComplete=$false;publicationStatus='Prepared locally; not tagged or published';packages=$packages;assets=$assets}
    $release|ConvertTo-Json -Depth 8|Set-Content -LiteralPath "$output/release-manifest.json" -Encoding utf8
    $assets+='release-manifest.json'
    $checksums=@($assets|Sort-Object|ForEach-Object {"$((Get-FileHash -LiteralPath (Join-Path $output $_) -Algorithm SHA256).Hash.ToLowerInvariant())  $_"})
    [IO.File]::WriteAllText("$output/SHA256SUMS.txt",($checksums -join "`n")+"`n")
    $expected=@($assets+'SHA256SUMS.txt'|Sort-Object)
    $actual=@(Get-ChildItem -LiteralPath $output -File|ForEach-Object Name|Sort-Object)
    if(@(Compare-Object $expected $actual).Count){throw 'Unexpected files in the release output folder; inspect them before distributing it.'}
    Write-Output "Prepared $output from $revision"
    Write-Output 'No tag, GitHub release, or upload was created. Final gameplay/UI signoff remains pending.'
}finally{Pop-Location}
