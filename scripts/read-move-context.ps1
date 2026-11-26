# Read-only move-history evidence. No resource reads or palette/gameplay writes.
param([ValidateRange(0,60)][int]$Seconds=0,[string]$Character='Filia',[string]$OutputPath='build/move-context.json')
$ErrorActionPreference='Stop'
$counts=@{Filia=529;Cerebella=564;Peacock=548;Parasoul=568;MsFortune=545;Painwheel=578;Valentine=589;Double=565;Squigly=627;BigBand=578;Eliza=645;Fukua=538;Beowulf=688;RoboFortune=545;Annie=582;Umbrella=612;BlackDahlia=604;Marie=555}
if(!$counts.ContainsKey($Character)){throw 'Unsupported character name.'}
if(!('MoveContextReader' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class MoveContextReader {
 [DllImport("kernel32.dll",SetLastError=true)] public static extern IntPtr OpenProcess(uint access,bool inherit,int id);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] data,UIntPtr size,out UIntPtr read);
 [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
}
'@
}
$games=@(Get-Process SkullGirls -ErrorAction SilentlyContinue | Where-Object {$_.Path -like '*skullloadah\.work\lobby-spectator\runtime-game\SkullGirls.exe'})
if($games.Count -ne 1){throw 'Expected exactly one isolated runner.'}
$game=$games[0];$moduleBase=$game.MainModule.BaseAddress.ToInt64()
$processHandle=[MoveContextReader]::OpenProcess(0x410,$false,$game.Id)
if($processHandle -eq [IntPtr]::Zero){throw 'Could not open runner read-only.'}
function Read-Bytes([long]$Address,[int]$Count) {
 if($Address -le 0 -or $Count -lt 1 -or $Count -gt 4096){throw 'Read exceeds diagnostic bounds.'}
 $bytes=[byte[]]::new($Count);$read=[UIntPtr]::Zero
 if(![MoveContextReader]::ReadProcessMemory($processHandle,[IntPtr]$Address,$bytes,[UIntPtr]$Count,[ref]$read) -or $read.ToUInt64() -ne $Count){throw 'Read-only move probe failed.'}
 return ,$bytes
}
function U32([long]$Address){[BitConverter]::ToUInt32((Read-Bytes $Address 4),0)}
$observations=[Collections.Generic.List[object]]::new();$reason='Completed';$initialRoot=0
try {
 $pe=U32 ($moduleBase+0x3c)
 if($pe -gt 0x10000 -or (U32 ($moduleBase+$pe+8)) -ne 0x6af3723f -or (U32 ($moduleBase+$pe+24+56)) -ne 0x8bd000){throw 'Unsupported game build.'}
 $initialRoot=U32 ($moduleBase+0x85ea4c)
 if(!$initialRoot -or (U32 ($initialRoot+0x168)) -ne 4){throw 'An active match is required.'}
 $watch=[Diagnostics.Stopwatch]::StartNew();$previous=''
 do {
  if((U32 ($moduleBase+0x85ea4c)) -ne $initialRoot -or (U32 ($initialRoot+0x168)) -ne 4){$reason='Match changed';break}
  $fighters=@()
  for($slot=0;$slot -lt 6;++$slot) {
   $actor=U32 ($initialRoot+0x5e8+4*$slot);if(!$actor){continue}
   $name=[Text.Encoding]::ASCII.GetString((Read-Bytes ($actor+0x310) 32)).Split([char]0)[0]
   if($name -ne $Character){continue}
   $entities=@()
   for($entitySlot=0;$entitySlot -lt 6;++$entitySlot){$entity=U32 ($initialRoot+0x1d4+4*$entitySlot);if($entity -and (U32 ($entity+0x330)) -eq $actor){$entities+=$entity}}
   $entities=@($entities | Sort-Object -Unique)
   if($entities.Count -ne 1){$fighters+=@{slot=$slot;name=$name;status='Unavailable';reason='Unique actor-bound entity missing'};continue}
   $entity=$entities[0];$bytes=Read-Bytes ($entity+0x9c) 12
   $values=@();for($index=0;$index -lt 3;++$index){$value=[BitConverter]::ToSingle($bytes,4*$index);$values+=@{scriptId=('0x{0:x2}' -f (0x27+$index));value=$(if([float]::IsNaN($value) -or [float]::IsInfinity($value)){$null}else{$value});validIndex=(![float]::IsNaN($value) -and ![float]::IsInfinity($value) -and $value -ge 0 -and $value -lt $counts[$Character] -and [Math]::Floor($value) -eq $value)}}
   $flags=(Read-Bytes ($entity+0x367) 1)[0]
   if((U32 ($entity+0x330)) -ne $actor -or (U32 ($initialRoot+0x5e8+4*$slot)) -ne $actor){$reason='Actor ownership changed';break}
   $fighters+=@{slot=$slot;name=$name;actor=('{0:X8}' -f $actor);entity=('{0:X8}' -f $entity);status='ObservedCandidates';values=$values;transitionFlags=$flags}
  }
  if($reason -ne 'Completed'){break}
  if((U32 ($moduleBase+0x85ea4c)) -ne $initialRoot -or (U32 ($initialRoot+0x168)) -ne 4){$reason='Match changed';break}
  $key=ConvertTo-Json -InputObject $fighters -Depth 8 -Compress
  if($key -ne $previous){$record=@{utc=[DateTime]::UtcNow.ToString('o');elapsedMs=$watch.ElapsedMilliseconds;fighters=$fighters};$observations.Add($record);$record | ConvertTo-Json -Depth 9 -Compress | Write-Output;$previous=$key}
  if(!$Seconds){break};Start-Sleep -Milliseconds 10
 } while($watch.Elapsed.TotalSeconds -lt $Seconds)
} catch {
 $reason=$_.Exception.Message;throw
} finally {
 [MoveContextReader]::CloseHandle($processHandle) | Out-Null
 $result=@{gameBuild=26588330;pid=$game.Id;readOnly=$true;character=$Character;endReason=$reason;runtimeSupport='Not enabled; history lifetime remains unverified';observations=$observations.ToArray()}
 $result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $OutputPath -Encoding utf8
}
