param(
  [string]$Repo = $env:ARX_PISTORIS_REPO,
  [string]$Version = $env:ARX_PISTORIS_VERSION,
  [string]$InstallDir = $env:ARX_PISTORIS_INSTALL_DIR
)

$ErrorActionPreference = 'Stop'

if (-not $Repo) { $Repo = 'Merxtef/arx-pistoris' }
if (-not $Version) { $Version = 'latest' }
if (-not $InstallDir) { $InstallDir = Join-Path $env:LOCALAPPDATA 'arx-pistoris\bin' }

$machineArch = if ($env:PROCESSOR_ARCHITEW6432) { $env:PROCESSOR_ARCHITEW6432 } else { $env:PROCESSOR_ARCHITECTURE }
if ($machineArch -notin @('AMD64', 'x86_64')) {
  throw "unsupported architecture: $machineArch"
}

$asset = 'arx-pistoris-windows-x64.zip'
if ($Version -eq 'latest') {
  $apiUrl = "https://api.github.com/repos/$Repo/releases/latest"
} else {
  $apiUrl = "https://api.github.com/repos/$Repo/releases/tags/$Version"
}

$release = Invoke-RestMethod -Uri $apiUrl
$download = $release.assets | Where-Object { $_.name -eq $asset } | Select-Object -First 1
if (-not $download) {
  throw "release asset not found: $asset in $Repo ($Version)"
}

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("arx-pistoris-install-" + [System.Guid]::NewGuid())
New-Item -ItemType Directory -Path $tmp | Out-Null

try {
  $archive = Join-Path $tmp $asset
  Invoke-WebRequest -Uri $download.browser_download_url -OutFile $archive
  Expand-Archive -Path $archive -DestinationPath $tmp -Force

  $exe = Get-ChildItem -Path $tmp -Filter 'arx-pistor.exe' -Recurse -File | Select-Object -First 1
  if (-not $exe) {
    throw "arx-pistor.exe not found in $asset"
  }

  New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
  Copy-Item -Path $exe.FullName -Destination (Join-Path $InstallDir 'arx-pistor.exe') -Force

  $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
  $parts = @()
  if ($userPath) {
    $parts = $userPath -split ';' | Where-Object { $_ }
  }

  $alreadyOnPath = $false
  foreach ($part in $parts) {
    if ($part.TrimEnd('\') -ieq $InstallDir.TrimEnd('\')) {
      $alreadyOnPath = $true
      break
    }
  }

  if (-not $alreadyOnPath) {
    $newPath = if ($userPath) { "$userPath;$InstallDir" } else { $InstallDir }
    [Environment]::SetEnvironmentVariable('Path', $newPath, 'User')
    Write-Host "Added $InstallDir to the user PATH."
    Write-Host "Open a new terminal before running arx-pistor by name."
  }

  Write-Host "Installed: $(Join-Path $InstallDir 'arx-pistor.exe')"
} finally {
  Remove-Item -Path $tmp -Recurse -Force -ErrorAction SilentlyContinue
}
