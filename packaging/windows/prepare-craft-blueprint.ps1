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

# Craft cleans its temporary FFmpeg tree after a failed configure, so the
# workflow's later diagnostic step cannot recover ffbuild/config.log. Capture
# it from inside the recipe while the source and build paths still exist.
$ffmpegBlueprint = Get-ChildItem -LiteralPath $CraftRoot -Recurse -File -Filter "ffmpeg.py" |
    Where-Object { $_.FullName -match "[\\/]libs[\\/]ffmpeg[\\/]ffmpeg\.py$" } |
    Select-Object -First 1

if (-not $ffmpegBlueprint) {
    throw "The KDE Craft FFmpeg blueprint was not found below $CraftRoot."
}

$ffmpegContents = (Get-Content -LiteralPath $ffmpegBlueprint.FullName -Raw) -replace "`r`n", "`n"
$configureAnchor = @'
    def configure(self):
        with utils.ScopedEnv(self._ffmpegEnv()):
            return super().configure()
'@
$configureReplacement = @'
    def configure(self):
        with utils.ScopedEnv(self._ffmpegEnv()):
            try:
                result = super().configure()
            except Exception:
                self._firaw_save_config_log()
                raise
            if not result:
                self._firaw_save_config_log()
            return result

    def _firaw_save_config_log(self):
        from pathlib import Path
        import shutil

        for directory in (self.buildDir(), self.sourceDir()):
            source = Path(directory) / "ffbuild" / "config.log"
            if not source.is_file():
                continue
            target = Path(os.environ["GITHUB_WORKSPACE"]) / "ffmpeg-config.log"
            shutil.copyfile(source, target)
            print(f"FFmpeg configure diagnostics saved to {target}", flush=True)
            for line in source.read_text(errors="replace").splitlines()[-200:]:
                print(line, flush=True)
            return
        print("FFmpeg ffbuild/config.log was not found during configure failure", flush=True)
'@
if (-not $ffmpegContents.Contains($configureAnchor)) {
    throw "The expected FFmpeg configure() anchor is missing from $($ffmpegBlueprint.FullName)."
}

$ffmpegContents = $ffmpegContents.Replace($configureAnchor, $configureReplacement)
Set-Content -LiteralPath $ffmpegBlueprint.FullName -Value $ffmpegContents -Encoding utf8
Write-Host "FFmpeg configure diagnostics enabled: $($ffmpegBlueprint.FullName)"
