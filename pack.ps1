[CmdletBinding()]
param([string]$DivinePath = 'divine.exe')

$ErrorActionPreference = 'Stop'
$divine = (Get-Command $DivinePath -ErrorAction Stop).Source
$sourceDir = Join-Path $PSScriptRoot 'script-extender/Data'
$outputDir = Join-Path $PSScriptRoot 'dist'
$package = Join-Path $outputDir 'TrueThirdPersonCamera.pak'
$verifyDir = Join-Path $PSScriptRoot ('build/package-check-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $divine --action create-package --source $sourceDir --destination $package --game bg3
if ($LASTEXITCODE -ne 0) { throw 'Creating the companion package failed.' }
& $divine --action extract-package --source $package --destination $verifyDir --game bg3
if ($LASTEXITCODE -ne 0) { throw 'Extracting the companion package for verification failed.' }

$sourceFiles = @(Get-ChildItem -LiteralPath $sourceDir -Recurse -File)
$packagedFiles = @(Get-ChildItem -LiteralPath $verifyDir -Recurse -File)
$expected = @($sourceFiles | ForEach-Object { [IO.Path]::GetRelativePath($sourceDir, $_.FullName) })
$actual = @($packagedFiles | ForEach-Object { [IO.Path]::GetRelativePath($verifyDir, $_.FullName) })
if (Compare-Object $expected $actual) { throw 'The package file list differs from the source.' }
foreach ($relative in $expected) {
    $before = (Get-FileHash -LiteralPath (Join-Path $sourceDir $relative) -Algorithm SHA256).Hash
    $after = (Get-FileHash -LiteralPath (Join-Path $verifyDir $relative) -Algorithm SHA256).Hash
    if ($before -ne $after) { throw "Packaged file differs from source: $relative" }
}
Write-Host "Verified $($expected.Count) files in $package"
