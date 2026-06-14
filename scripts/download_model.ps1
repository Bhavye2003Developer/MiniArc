$ModelRepo = "Qwen/Qwen2.5-0.5B-Instruct-GGUF"
$ModelFile = "qwen2.5-0.5b-instruct-q4_k_m.gguf"
$HfUrl     = "https://huggingface.co/$ModelRepo/resolve/main/$ModelFile"
$Dest      = "models\$ModelFile"

if (-not (Test-Path "models")) {
    New-Item -ItemType Directory -Path "models" | Out-Null
}

if (Test-Path $Dest) {
    Write-Host "Model already exists at $Dest"
    exit 0
}

Write-Host "Downloading $ModelFile (~300 MB) from HuggingFace..."
Write-Host "URL: $HfUrl"
Write-Host ""

$wc = New-Object System.Net.WebClient
$wc.DownloadProgressChanged += {
    param($sender, $e)
    Write-Progress -Activity "Downloading" -Status "$($e.ProgressPercentage)%" `
                   -PercentComplete $e.ProgressPercentage
}
$task = $wc.DownloadFileTaskAsync($HfUrl, [System.IO.Path]::GetFullPath($Dest))
$task.Wait()

Write-Host ""
Write-Host "Saved to $Dest"
Write-Host "Run:  .\build\Debug\miniARC.exe"
