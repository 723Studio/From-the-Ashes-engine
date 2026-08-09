param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDir,

    [Parameter(Mandatory = $true)]
    [string]$OutputFile
)

$ErrorActionPreference = 'Stop'

$buildDate = $env:FTA_BUILD_DATE
if ([string]::IsNullOrWhiteSpace($buildDate)) {
    $buildDate = [DateTime]::UtcNow.ToString('yyyyMMdd')
}
elseif ($buildDate -notmatch '^\d{8}$') {
    throw "FTA_BUILD_DATE must use UTC YYYYMMDD format, got: $buildDate"
}

$commit = 'unknown'
$dirtySuffix = ''
$git = Get-Command git.exe -ErrorAction SilentlyContinue
if ($null -ne $git) {
    $commitOutput = & $git.Source -C $SourceDir rev-parse --short=8 HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and "$commitOutput" -match '^[0-9a-fA-F]+$') {
        $commit = "$commitOutput".ToLowerInvariant()
    }

    $statusOutput = & $git.Source -C $SourceDir status --porcelain --untracked-files=no 2>$null
    if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace("$statusOutput")) {
        $dirtySuffix = '-dirty'
    }
}

$buildId = "$buildDate-$commit$dirtySuffix"
$content = "#pragma once`r`n`r`n#define FTA_BUILD_ID `"$buildId`"`r`n#define FTA_BUILD_DATE `"$buildDate`"`r`n"
$resolvedOutput = [IO.Path]::GetFullPath($OutputFile)
$outputDirectory = [IO.Path]::GetDirectoryName($resolvedOutput)
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null

$existing = if ([IO.File]::Exists($resolvedOutput)) { [IO.File]::ReadAllText($resolvedOutput) } else { '' }
if ($existing -ne $content) {
    [IO.File]::WriteAllText($resolvedOutput, $content, [Text.UTF8Encoding]::new($false))
}

Write-Host "FTA build ID: $buildId"
