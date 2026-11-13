$proj = Join-Path $PSScriptRoot "imagedetect.vcxproj"
$xml = Get-Content $proj -Raw -Encoding UTF8

# Always refresh PostBuildEvent command (trailing \ in OutDir breaks cmd quotes)
$postBuildInner = @'
    <PostBuildEvent>
      <Command>call "$(ProjectDir)..\setup_runtime_copy.bat" "$(ProjectDir)$(OutDir)."</Command>
      <Message>Stage Snap7/Onna/MVS/Python/OpenCV runtime DLLs</Message>
    </PostBuildEvent>
'@

if ($xml -match 'setup_runtime_copy\.bat') {
    $xml2 = [regex]::Replace($xml, '(?s)\s*<PostBuildEvent>.*?</PostBuildEvent>', "")
    $xml = $xml2
}

$xml2 = [regex]::Replace($xml, '(?m)^  </ItemDefinitionGroup>', ($postBuildInner + "`r`n  </ItemDefinitionGroup>"))
if ($xml2 -eq $xml) {
    Write-Error "Failed to inject PostBuildEvent"
    exit 1
}
Set-Content -Path $proj -Value $xml2 -Encoding UTF8
Write-Host "PostBuildEvent injected (OutDir. quote-safe)."
