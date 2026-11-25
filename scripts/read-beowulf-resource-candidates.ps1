# Read-only diagnostic. Script slot 0x64 is a Hype candidate, not a supported trigger.
param([ValidateRange(0,60)][int]$Seconds=0,[string]$OutputPath='build/beowulf-resource-candidates.json')
$ErrorActionPreference='Stop'
if(!('BeowulfResourceReader' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class BeowulfResourceReader {
 [DllImport("kernel32.dll",SetLastError=true)] public static extern IntPtr OpenProcess(uint access,bool inherit,int id);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] data,UIntPtr size,out UIntPtr read);
 [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
}
'@
}
$games=@(Get-Process SkullGirls -ErrorAction SilentlyContinue | Where-Object {$_.Path -like '*skullloadah\.work\lobby-spectator\runtime-game\SkullGirls.exe'})
if($games.Count -ne 1){throw 'Expected exactly one isolated runner.'}
$game=$games[0];$moduleBase=$game.MainModule.BaseAddress.ToInt64()
$processHandle=[BeowulfResourceReader]::OpenProcess(0x410,$false,$game.Id)
if($processHandle -eq [IntPtr]::Zero){throw 'Could not open runner read-only.'}
function Read-Bytes([long]$Address,[int]$Count) {
    if($Address -le 0 -or $Count -lt 1 -or $Count -gt 4096){throw 'Read exceeds diagnostic bounds.'}
    $bytes=[byte[]]::new($Count);$read=[UIntPtr]::Zero
    if(![BeowulfResourceReader]::ReadProcessMemory($processHandle,[IntPtr]$Address,$bytes,[UIntPtr]$Count,[ref]$read) -or $read.ToUInt64() -ne $Count){throw 'Read-only probe failed.'}
    return ,$bytes
}
function U32([long]$Address){[BitConverter]::ToUInt32((Read-Bytes $Address 4),0)}
function F32([long]$Address){[BitConverter]::ToSingle((Read-Bytes $Address 4),0)}
$observations=[Collections.Generic.List[object]]::new();$reason='Completed';$initialRoot=0
try {
    $pe=U32 ($moduleBase+0x3c)
    if((U32 ($moduleBase+$pe+8)) -ne 0x6af3723f -or (U32 ($moduleBase+$pe+24+56)) -ne 0x8bd000){throw 'Unsupported game build; no samples taken.'}
    $initialRoot=U32 ($moduleBase+0x85ea4c)
    if(!$initialRoot -or (U32 ($initialRoot+0x168)) -ne 4){throw 'An active match is required.'}
    $watch=[Diagnostics.Stopwatch]::StartNew();$previous=''
    do {
        if((U32 ($moduleBase+0x85ea4c)) -ne $initialRoot -or (U32 ($initialRoot+0x168)) -ne 4){$reason='Match changed; stopped before using stale pointers';break}
        $fighters=@()
        for($slot=0;$slot -lt 6;++$slot) {
            $actor=U32 ($initialRoot+0x5e8+4*$slot);if(!$actor){continue}
            $name=[Text.Encoding]::ASCII.GetString((Read-Bytes ($actor+0x310) 32)).Split([char]0)[0]
            if($name -ne 'Beowulf'){continue}
            $entities=@()
            for($entitySlot=0;$entitySlot -lt 6;++$entitySlot) {
                $entity=U32 ($initialRoot+0x1d4+4*$entitySlot)
                if($entity -and (U32 ($entity+0x330)) -eq $actor){$entities+=$entity}
            }
            $entities=@($entities | Select-Object -Unique)
            $row=[ordered]@{slot=$slot;actor=('{0:X8}' -f $actor);name=$name;paletteIndex=[int](Read-Bytes ($actor+0x330) 1)[0];status='Unavailable';reason='Expected one actor-bound entity'}
            if($entities.Count -eq 1) {
                $entity=[long]$entities[0]
                # Native 0048bf30 reads ordinary kind-1 variables as float[scriptId].
                # 0x64 => entity+0x190. Ownership is checked separately at entity+0x330.
                $value=F32 ($entity+4*0x64)
                if((U32 ($initialRoot+0x5e8+4*$slot)) -ne $actor -or (U32 ($entity+0x330)) -ne $actor){throw 'Actor ownership changed during sample.'}
                $row.entity=('{0:X8}' -f $entity);$row.scriptId='0x64';$row.storage='float32 at entity+0x190'
                if(![float]::IsNaN($value) -and ![float]::IsInfinity($value) -and $value -ge 0 -and $value -le 3) {
                    $row.status='ObservedCandidate';$row.reason='Hype semantics remain under investigation';$row.value=$value;$row.animation=F32 ($entity+4*0x27)
                } else {$row.reason='Candidate outside observed range; unavailable'}
            }
            $fighters+=$row
        }
        if((U32 ($moduleBase+0x85ea4c)) -ne $initialRoot -or (U32 ($initialRoot+0x168)) -ne 4){$reason='Match changed during sample; discarded sample';break}
        $signature=ConvertTo-Json -InputObject $fighters -Depth 5 -Compress
        if($signature -ne $previous) {
            $sample=[ordered]@{utc=[DateTime]::UtcNow.ToString('o');elapsedMs=$watch.ElapsedMilliseconds;fighters=$fighters}
            $observations.Add($sample);$sample | ConvertTo-Json -Depth 6 -Compress | Write-Output;$previous=$signature
        }
        if($Seconds -gt 0){Start-Sleep -Milliseconds 50}
    }while($watch.Elapsed.TotalSeconds -lt $Seconds)
    $report=[ordered]@{gameBuild=26588330;pid=$game.Id;root=('{0:X8}' -f $initialRoot);readOnly=$true;runtimeTriggerSupport='Unsupported';endReason=$reason;observations=$observations}
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputPath -Encoding utf8
} finally {[void][BeowulfResourceReader]::CloseHandle($processHandle)}
