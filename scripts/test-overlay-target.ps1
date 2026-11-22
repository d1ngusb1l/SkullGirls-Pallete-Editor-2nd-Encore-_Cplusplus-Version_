param(
    [Parameter(Mandatory)][string]$RunnerDirectory,
    [Parameter(Mandatory)][string]$PaletteFile,
    [ValidateRange(0,5)][int]$Slot=0,
    [ValidateSet('Fire','Water','Space')][string]$MarkerTexture,
    [switch]$Preview
)
$ErrorActionPreference='Stop'
$runner=(Resolve-Path -LiteralPath $RunnerDirectory).Path
$game=@(Get-Process SkullGirls -ErrorAction SilentlyContinue|Where-Object {$_.Path -eq (Join-Path $runner 'SkullGirls.exe')})
if($game.Count -ne 1){throw 'Expected the isolated runner to be open in Training.'}
$palettePath=(Resolve-Path -LiteralPath $PaletteFile).Path
$source=[IO.File]::ReadAllBytes($palettePath)
if($source.Length -lt 34 -or $source.Length -gt 4MB){throw 'Invalid palette file size.'}
$count=[BitConverter]::ToUInt32($source,16)
if($count -lt 1 -or $count -gt 4096){throw 'Invalid palette color count.'}
$prefixLength=22+4*($count-1)+12
if($source.Length -lt $prefixLength+12 -or [Text.Encoding]::ASCII.GetString($source,$prefixLength,7) -ne 'SGPEFX1'){
    throw 'Expected an extended .pal file.'
}
$document=[Text.Encoding]::UTF8.GetString($source,$prefixLength+12,$source.Length-$prefixLength-12)|ConvertFrom-Json
if($document.version -ne 2 -or !$document.effects.overlay -or !$document.effects.secondOverlay){throw 'Expected two saved overlays.'}
function Request($Name,$Data){
    $path=Join-Path $runner $Name
    if(Test-Path -LiteralPath $path){throw "Runner already has a pending request: $Name"}
    $Data|ConvertTo-Json -Depth 20 -Compress|Set-Content -LiteralPath "$path.tmp"
    Move-Item -LiteralPath "$path.tmp" -Destination $path
}
function Stamp($Path){
    if(Test-Path -LiteralPath $Path){return (Get-Item -LiteralPath $Path).LastWriteTimeUtc}
    return [datetime]::MinValue
}
function Wait-NewFile($Path,$Previous){
    for($attempt=0;$attempt -lt 50;$attempt++){
        if($game[0].HasExited){throw 'The runner exited before writing the result.'}
        if((Stamp $Path) -gt $Previous){return Get-Content -LiteralPath $Path -Raw|ConvertFrom-Json}
        Start-Sleep -Milliseconds 100
    }
    throw "Runner did not update $Path."
}
function Capture(){
    $path=Join-Path $runner 'renderer-trace/capture.json'
    $old=Stamp $path
    Set-Content -LiteralPath (Join-Path $runner 'renderer-trace.request') -Value 'run' -NoNewline
    Wait-NewFile $path $old
}
function Bindings($Capture){
    $bindings=@{}
    foreach($draw in $Capture.draws){
        if($draw.shader -ne '4f2d54bdda78e7fd'){continue}
        $feature=@($draw.textures|Where-Object sampler -eq 2)[0]
        if(!$feature){continue}
        $overlay=@($draw.textures|Where-Object sampler -eq 3)[0]
        if(!$bindings.ContainsKey($feature.object)){$bindings[$feature.object]=[Collections.Generic.HashSet[string]]::new()}
        [void]$bindings[$feature.object].Add([string]$overlay.object)
    }
    return $bindings
}
$markers=if($MarkerTexture){@($MarkerTexture)}else{@("Fire","Space","Water")|Where-Object {$_ -ne $document.effects.overlay.texture}}
$target=$null
foreach($markerTexture in $markers){
    $probe=$null
    $baseline=Bindings (Capture)

    $document.effects.overlay.texture=$markerTexture
    $payload=[Text.Encoding]::UTF8.GetBytes(($document|ConvertTo-Json -Depth 20 -Compress))
    $markerPath=Join-Path $runner ('overlay-target-'+[guid]::NewGuid().ToString('N')+'.pal')
    $header=[byte[]]$source[0..($prefixLength+7)]
    [IO.File]::WriteAllBytes($markerPath,[byte[]]($header+[BitConverter]::GetBytes([uint32]$payload.Length)+$payload))
    $result=Join-Path $runner 'dll-overlay-probe-result.json'
    try{
        $old=Stamp $result
        Request 'dll-overlay-probe.request.json' @{slot=$Slot;seconds=14;paletteFile=$markerPath}
        $probe=Wait-NewFile $result $old
        if($probe.state -ne 'Active'){throw "Marker probe failed: $($probe.error)"}
        $active=Bindings (Capture)
        $candidates=@($baseline.Keys|Where-Object {$active.ContainsKey($_) -and $baseline[$_].Count -eq 1 -and $active[$_].Count -eq 1 -and !$active[$_].SetEquals($baseline[$_]) -and !$active[$_].Contains("")})
        if($candidates.Count -gt 1){throw "Multiple draw resources changed overlay binding."}
        if(!$candidates.Count){continue}
        $target=[pscustomobject]@{feature=$candidates[0];slot=$Slot;palette=$probe.palette;markerTexture=$markerTexture}
    }finally{
        try {
            if($probe.state -eq 'Active'){
                for($i=0;$i -lt 80;$i++){
                    $current=Get-Content -LiteralPath $result -Raw|ConvertFrom-Json
                    if($current.restored){break}
                    Start-Sleep -Milliseconds 200
                }
                if(!$current.restored){throw 'The native probe did not report restoration.'}
            }
        }finally{Remove-Item -LiteralPath $markerPath}
    }
    break
}
if(!$target){throw 'No marker produced a unique overlay binding change.'}
$target
if($Preview){
    $old=Stamp $result
    Request 'dll-overlay-probe.request.json' @{slot=$Slot;seconds=40;paletteFile=$palettePath}
    $probe=Wait-NewFile $result $old
    if($probe.state -ne 'Active' -or $probe.palette -ne $target.palette){
        throw 'Target palette changed or the saved overlay probe failed.'
    }
    Request 'overlay-draw.request.json' @{action='capture-first';feature=$target.feature}
    $requestPath=Join-Path $runner 'overlay-draw.request.json'
    for($i=0;$i -lt 40 -and (Test-Path -LiteralPath $requestPath);$i++){Start-Sleep -Milliseconds 100}
    Start-Sleep -Milliseconds 500
    $drawResult=Join-Path $runner 'overlay-draw-result.json'
    $old=Stamp $drawResult
    Request 'overlay-draw.request.json' @{action='preview';paletteFile=$palettePath}
    for($i=0;$i -lt 100 -and (Stamp $drawResult) -le $old;$i++){
        if($game[0].HasExited){throw 'The runner exited during the draw preview.'}
        Start-Sleep -Milliseconds 200
    }
    $draw=Wait-NewFile $drawResult $old
    if(!$draw.finished -or $draw.drawsReplaced -le 0){throw "Draw substitution failed: $($draw.error)"}
    $draw
    for($i=0;$i -lt 130;$i++){
        $current=Get-Content -LiteralPath $result -Raw|ConvertFrom-Json
        if($current.restored){break}
        Start-Sleep -Milliseconds 200
    }
    if(!$current.restored){throw 'Saved overlay probe restoration was not verified.'}
}
