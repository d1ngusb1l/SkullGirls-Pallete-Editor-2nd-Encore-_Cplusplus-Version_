param(
    [Parameter(Mandatory)][string]$Source,
    [Parameter(Mandatory)][string]$Fxc
)
$ErrorActionPreference='Stop'
if((Get-FileHash -LiteralPath $Source).Hash -ne '33C6DC40F76EF1DA903EBEFF3D920848D19522C87535CD02E3C68C7D7DEC931B'){
    throw 'Expected the tested Windows TiledSprite pixel shader source.'
}
$s=Get-Content -LiteralPath $Source -Raw
$decl='uniform sampler2D gChowderTexture;'
$s=$s.Replace($decl,($decl+"`n"+'sampler2D gSecondOverlay : register(s4);'+"`n"+'sampler2D gOverlayBandMap : register(s5);'))
$s=$s.Replace('float4 ChowderEffectSample(half3 worldPos)','half2 OverlayUV(half3 worldPos)')
$a=$s.IndexOf('#if defined(ChowderTechDebug)')
$b=$s.IndexOf('return chowderColor;',$a)
$s=$s.Substring(0,$a)+'return chowderUV;'+$s.Substring($b+'return chowderColor;'.Length)
$s=$s.Replace('half4 chowderColor, out half4 color','half2 chowderUV, out half4 color')
$s=$s.Replace('float4 chowderColor = ChowderEffectSample(chowderPosAnchor);','half2 chowderColor = OverlayUV(chowderPosAnchor);')
$s=$s.Replace('half lightingIn, out half lightingOut)','half lightingIn, float overlayTint, out half lightingOut)')
$s=$s.Replace('float indexV = gChowderPaletteIndex *','float indexV = overlayTint *').Replace('gChowderPaletteIndex > 0','overlayTint > 0')
$s=$s.Replace('color.rgb = ChowderEffectMix(color.rgb, chowderColor, shaderFeaturePixels[1], lighting, lighting);',@"
float2 overlaySettings = tex2D(gOverlayBandMap, float2(0.5, indexV)).rg;
    half4 chowderColor = lerp(tex2D(gChowderTexture, chowderUV), tex2D(gSecondOverlay, chowderUV), overlaySettings.r);
    color.rgb = ChowderEffectMix(color.rgb, chowderColor, shaderFeaturePixels[1], lighting, overlaySettings.g, lighting);
"@)
$registers=@{gChowderAnchorMode=28;gChowderBlendMode=30;gChowderAnchorPosition=31;gLineColor=32;gHueShiftCos=33;gHueShiftSin=34}
foreach($name in $registers.Keys){
    $s=$s -replace "(?m)(^uniform float[3]? $name)(?=[; =])",('$1 : register(c'+$registers[$name]+')')
}
$out=Join-Path (Split-Path -Parent $PSScriptRoot) 'build/overlay-shader'
New-Item -ItemType Directory -Force $out|Out-Null
Set-Content -LiteralPath "$out/prototype.hlsl" -Value $s
& $Fxc /nologo /T ps_3_0 /O3 /E PaletteGradient /Fo "$out/prototype.bin" /Fc "$out/prototype.asm" "$out/prototype.hlsl"
if($LASTEXITCODE){throw 'Overlay shader compilation failed.'}
