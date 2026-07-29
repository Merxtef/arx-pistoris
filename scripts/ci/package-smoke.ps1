# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

param(
  [Parameter(Mandatory = $true)]
  [string]$Prefix
)

$ErrorActionPreference = 'Stop'

$Exe = Join-Path $Prefix 'bin/arx-pistor.exe'
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
  throw "missing executable: $Exe"
}

& $Exe --version *> $null
if ($LASTEXITCODE -ne 0) { throw "--version failed with exit code $LASTEXITCODE" }

& $Exe --help *> $null
if ($LASTEXITCODE -ne 0) { throw "--help failed with exit code $LASTEXITCODE" }

$PreviousErrorActionPreference = $ErrorActionPreference
try {
  $ErrorActionPreference = 'SilentlyContinue'
  & $Exe *> $null
  $ExitCode = $LASTEXITCODE
} finally {
  $ErrorActionPreference = $PreviousErrorActionPreference
}
if ($ExitCode -ne 1) { throw "no-argument invocation returned $ExitCode, expected 1" }

$DocDir = Join-Path $Prefix 'share/doc/arx-pistoris'
foreach ($Name in @(
  'LICENSE',
  'ADDITIONAL_TERMS',
  'AUTHORS',
  'ARX_LIBERTATIS_AUTHORS',
  'README.md',
  'cgltf-LICENSE',
  'stb-LICENSE',
  'blast-LICENSE',
  'pklib-LICENSE',
  'nlohmann-json-LICENSE',
  'mapbox-earcut-LICENSE'
)) {
  $Path = Join-Path $DocDir $Name
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    throw "missing installed document: $Path"
  }
}

[scriptblock]::Create((Get-Content -LiteralPath scripts/install.ps1 -Raw)) | Out-Null
