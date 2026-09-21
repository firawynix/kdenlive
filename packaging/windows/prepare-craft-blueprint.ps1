param(
    [Parameter(Mandatory = $true)]
    [string]$CraftRoot,

    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[0-9a-fA-F]{40}$")]
    [string]$Commit
)

$ErrorActionPreference = "Stop"

$blueprint = Get-ChildItem -LiteralPath $CraftRoot -Recurse -File -Filter "kdenlive.py" |
    Where-Object { $_.FullName -match "[\\/]kde[\\/]kdemultimedia[\\/]kdenlive[\\/]kdenlive\.py$" } |
    Select-Object -First 1

if (-not $blueprint) {
    throw "The KDE Craft Kdenlive blueprint was not found below $CraftRoot."
}

$contents = Get-Content -LiteralPath $blueprint.FullName -Raw
$anchor = "        self.versionInfo.setDefaultValues()"
if (-not $contents.Contains($anchor)) {
    throw "The expected setDefaultValues() anchor is missing from $($blueprint.FullName)."
}

$targetName = "firawynix-$Commit"
$replacement = @"
$anchor
        # The public CI build must resolve to the exact GitHub commit that
        # triggered the workflow. This makes the binary origin independently
        # verifiable by SignPath and by downstream users.
        self.svnTargets["$targetName"] = "https://github.com/firawynix/kdenlive.git||$Commit"
        self.defaultTarget = "$targetName"
"@

$contents = $contents.Replace($anchor, $replacement.TrimEnd())
Set-Content -LiteralPath $blueprint.FullName -Value $contents -Encoding utf8

Write-Host "Craft blueprint pinned to Firawynix Kdenlive commit $Commit"
Write-Host "Blueprint: $($blueprint.FullName)"

