$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
Push-Location $repoRoot
try {
  $manifest = Get-Content -Raw tools\readme_media_manifest.json | ConvertFrom-Json
  $missing = New-Object System.Collections.Generic.List[string]
  foreach ($project in $manifest.projects) {
    $image = Join-Path $manifest.mediaDir $project.image
    if (-not (Test-Path -LiteralPath $image)) { $missing.Add($image) }
    elseif ((Get-Item -LiteralPath $image).Length -le 0) { $missing.Add("$image is empty") }
  }
  $gifProjects = @($manifest.projects | Where-Object { $null -ne $_.PSObject.Properties['gif'] -and -not [string]::IsNullOrWhiteSpace([string]$_.gif) })
  foreach ($project in $gifProjects) {
    $gifPath = Join-Path $manifest.mediaDir $project.gif
    if (-not (Test-Path -LiteralPath $gifPath)) { $missing.Add($gifPath) }
    elseif ((Get-Item -LiteralPath $gifPath).Length -le 0) { $missing.Add("$gifPath is empty") }
  }
  if (-not (Test-Path -LiteralPath README_old.md)) { $missing.Add("README_old.md") }
  $readme = Get-Content -Raw README.md
  if ($readme -match 'github\.com/user-attachments') { $missing.Add("README.md still references github.com/user-attachments") }
  foreach ($project in $manifest.projects) {
    if ($readme -notmatch [regex]::Escape("docs/media/readme/$($project.image)")) {
      $missing.Add("README.md missing docs/media/readme/$($project.image)")
    }
  }
  foreach ($project in $gifProjects) {
    if ($readme -notmatch [regex]::Escape("docs/media/readme/$($project.gif)")) {
      $missing.Add("README.md missing docs/media/readme/$($project.gif)")
    }
  }
  if ($missing.Count -gt 0) {
    $missing | ForEach-Object { Write-Error -Message $_ -ErrorAction Continue }
    exit 1
  }
  "README media verification passed"
}
finally {
  Pop-Location
}
