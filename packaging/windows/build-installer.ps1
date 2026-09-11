param(
    [string]$Version = "26.11.70-firaw.4",
    [string]$BuildDirectory = "C:\_\3377f5a\build",
    [string]$CraftRoot = "C:\CraftRoot",
    [string]$PayloadArchive = ""
)

$ErrorActionPreference = "Stop"
$repository = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$output = Join-Path $repository "dist\windows"
$stage = Join-Path $output "stage"
$launcherProject = Join-Path $PSScriptRoot "launcher\FirawynixKdenliveLauncher.csproj"
$launcherOutput = Join-Path $output "launcher"
$editor = Join-Path $BuildDirectory "bin\firawynix-kdenlive.exe"
$translation = Join-Path $BuildDirectory "locale\pt_BR\LC_MESSAGES\kdenlive.mo"
$splashQml = Join-Path $CraftRoot "qml\org\kde\kdenlive\Splash.qml"

if (-not (Test-Path -LiteralPath $editor)) {
    throw "Compile o Firawynix - Kdenlive antes de criar o instalador: $editor"
}
if (-not (Test-Path -LiteralPath $splashQml)) {
    throw "A tela inicial instalada pelo build não foi encontrada: $splashQml"
}

New-Item -ItemType Directory -Path $output -Force | Out-Null
if (Test-Path -LiteralPath $stage) {
    $resolvedStage = (Resolve-Path -LiteralPath $stage).Path
    if (-not $resolvedStage.StartsWith($output, [StringComparison]::OrdinalIgnoreCase)) {
        throw "A pasta temporária do instalador está fora do diretório esperado."
    }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
New-Item -ItemType Directory -Path $stage -Force | Out-Null

$versionNumbers = [regex]::Matches($Version, "\d+") | ForEach-Object { $_.Value }
while ($versionNumbers.Count -lt 4) {
    $versionNumbers += "0"
}
$assemblyVersion = ($versionNumbers | Select-Object -First 4) -join "."
dotnet publish $launcherProject -c Release -r win-x64 --self-contained true -p:Version=$Version -p:AssemblyVersion=$assemblyVersion -p:FileVersion=$assemblyVersion -p:InformationalVersion=$Version -o $launcherOutput
if ($LASTEXITCODE -ne 0) {
    throw "Falha ao compilar o launcher."
}

if ([string]::IsNullOrWhiteSpace($PayloadArchive)) {
    python (Join-Path $CraftRoot "craft\bin\craft.py") --options "[Packager]PackageType=PortablePackager" --package kde/kdemultimedia/kdenlive
    if ($LASTEXITCODE -ne 0) {
        throw "O Craft não conseguiu criar o pacote portátil."
    }
    $PayloadArchive = Get-ChildItem (Join-Path $CraftRoot "tmp") -File |
        Where-Object {
            $_.Name -match "^kdenlive-.*-windows-.*\.(7z|zip)$" -and
            $_.Name -notmatch "-(dbg|logs)\."
        } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $PayloadArchive -or -not (Test-Path -LiteralPath $PayloadArchive)) {
    throw "Pacote portátil do Craft não encontrado."
}

$sevenZip = Join-Path $CraftRoot "dev-utils\bin\7z.exe"
if (-not (Test-Path -LiteralPath $sevenZip)) {
    $sevenZip = Join-Path $CraftRoot "dev-utils\bin\7za.exe"
}
if (-not (Test-Path -LiteralPath $sevenZip)) {
    $sevenZip = Join-Path $CraftRoot "bin\7za.exe"
}
if (-not (Test-Path -LiteralPath $sevenZip)) {
    throw "7-Zip não encontrado na instalação do KDE Craft."
}
& $sevenZip x $PayloadArchive "-o$stage" -y | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Falha ao extrair o pacote portátil."
}

$nestedRoot = Get-ChildItem -LiteralPath $stage -Directory | Where-Object { Test-Path (Join-Path $_.FullName "bin") } | Select-Object -First 1
if ($nestedRoot -and -not (Test-Path (Join-Path $stage "bin"))) {
    Get-ChildItem -LiteralPath $nestedRoot.FullName -Force | Move-Item -Destination $stage
    Remove-Item -LiteralPath $nestedRoot.FullName -Force
}

$runtimeChecks = @(
    @{ Pattern = "avcodec-*.dll"; Label = "FFmpeg (avcodec)" },
    @{ Pattern = "Qt6Core.dll"; Label = "Qt 6 Core" }
)
foreach ($runtimeCheck in $runtimeChecks) {
    $runtimeMatch = Get-ChildItem -LiteralPath (Join-Path $stage "bin") -Filter $runtimeCheck.Pattern -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $runtimeMatch) {
        throw "O pacote portátil está incompleto: $($runtimeCheck.Label) não foi encontrado em bin."
    }
}

New-Item -ItemType Directory -Path (Join-Path $stage "bin\data\locale\pt_BR\LC_MESSAGES") -Force | Out-Null
Copy-Item -LiteralPath $editor -Destination (Join-Path $stage "bin\firawynix-kdenlive.exe") -Force
Copy-Item -LiteralPath $translation -Destination (Join-Path $stage "bin\data\locale\pt_BR\LC_MESSAGES\kdenlive.mo") -Force
$splashDestination = Get-ChildItem -LiteralPath $stage -Recurse -File -Filter "Splash.qml" |
    Where-Object { $_.FullName -match "[\\/]org[\\/]kde[\\/]kdenlive[\\/]Splash\.qml$" } |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $splashDestination) {
    throw "O pacote portátil não contém o módulo QML da tela inicial do Kdenlive."
}
Copy-Item -LiteralPath $splashQml -Destination $splashDestination -Force
Copy-Item -LiteralPath (Join-Path $launcherOutput "Firawynix-Kdenlive-Launcher.exe") -Destination (Join-Path $stage "Firawynix-Kdenlive-Launcher.exe") -Force
$legacyEditor = Join-Path $stage "bin\kdenlive.exe"
if (Test-Path -LiteralPath $legacyEditor) {
    Remove-Item -LiteralPath $legacyEditor -Force
}

$iscc = @(
    (Join-Path $CraftRoot "dev-utils\bin\ISCC.exe"),
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $iscc) {
    throw "Inno Setup não encontrado. Instale-o com: python C:\CraftRoot\craft\bin\craft.py innosetup"
}

$script = Join-Path $PSScriptRoot "firawynix-kdenlive.iss"
& $iscc "/DMyAppVersion=$Version" "/DStageDir=$stage" "/DOutputDir=$output" $script
if ($LASTEXITCODE -ne 0) {
    throw "Falha ao criar o instalador Inno Setup."
}

$installer = Join-Path $output "Firawynix-Kdenlive-Setup-x64.exe"
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installer).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$installer.sha256" -Value "$hash  $(Split-Path $installer -Leaf)" -Encoding ascii
@{
    version = $Version
    repository = "https://github.com/firawynix/kdenlive"
    upstream = "https://github.com/KDE/kdenlive"
    asset = (Split-Path $installer -Leaf)
    sha256 = $hash
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output "update.json") -Encoding utf8

Write-Host "Instalador criado: $installer"
Write-Host "SHA-256: $hash"
