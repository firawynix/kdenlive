param(
    [string]$Version = "26.11.70-firaw.8",
    [string]$BuildDirectory = "C:\_\3377f5a\build",
    [string]$CraftRoot = "C:\CraftRoot",
    [string]$PayloadArchive = "",
    [string]$WhisperCppDirectory = "C:\_\whispercpp-v1.9.4\build-vulkan\bin\Release",
    [string]$WhisperModel = "C:\_\whispercpp-v1.9.4\models\ggml-small.bin",
    [string]$CertificateThumbprint = "9A2EFF2483185C9A900F2797D7A9CBD5E8A12893",
    [switch]$PublicBuild
)

$ErrorActionPreference = "Stop"
$repository = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$output = Join-Path $repository "dist\windows"
$stage = Join-Path $output "stage"
$launcherProject = Join-Path $PSScriptRoot "launcher\FirawynixKdenliveLauncher.csproj"
$launcherOutput = Join-Path $output "launcher"
$editor = Join-Path $BuildDirectory "bin\firawynix-kdenlive.exe"
$visionHelper = Join-Path $BuildDirectory "bin\firawynix-kdenlive-ai-vision.exe"
$translation = Join-Path $BuildDirectory "locale\pt_BR\LC_MESSAGES\kdenlive.mo"
$splashQml = Join-Path $CraftRoot "qml\org\kde\kdenlive\Splash.qml"
$certificateFile = Join-Path $PSScriptRoot "certificates\Firawynix-Internal-Code-Signing.cer"

if (-not (Test-Path -LiteralPath $editor)) {
    throw "Compile o Firawynix - Kdenlive antes de criar o instalador: $editor"
}
if (-not (Test-Path -LiteralPath $visionHelper)) {
    throw "Compile o analisador visual local antes de criar o instalador: $visionHelper"
}
if (-not (Test-Path -LiteralPath $splashQml)) {
    throw "A tela inicial instalada pelo build não foi encontrada: $splashQml"
}
$whisperExecutable = Join-Path $WhisperCppDirectory "whisper-cli.exe"
if (-not $PublicBuild) {
    if (-not (Test-Path -LiteralPath $WhisperModel)) {
        throw "O modelo Whisper.cpp Small não foi encontrado: $WhisperModel"
    }
    if (-not (Test-Path -LiteralPath $whisperExecutable)) {
        throw "O Whisper.cpp com Vulkan não foi encontrado: $whisperExecutable"
    }
}

$signTool = $null
if (-not $PublicBuild) {
    $signTool = @(
        (Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"),
        $env:FIRAW_SIGNTOOL
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -First 1
    if (-not $signTool) {
        throw "SignTool não encontrado. Instale o Windows SDK antes de criar o pacote assinado."
    }
    $certificate = Get-Item -LiteralPath "Cert:\CurrentUser\My\$CertificateThumbprint" -ErrorAction Stop
    if (-not $certificate.HasPrivateKey -or $certificate.NotAfter -le (Get-Date)) {
        throw "O certificado Firawynix de assinatura não está válido ou não possui a chave privada."
    }
    $publicCertificate = [Security.Cryptography.X509Certificates.X509Certificate2]::new($certificateFile)
    if ($publicCertificate.Thumbprint -ne $CertificateThumbprint -or $publicCertificate.HasPrivateKey) {
        throw "O CER público não corresponde ao certificado de assinatura esperado."
    }
}

function Invoke-SignArtifact([string]$Path) {
    & $signTool sign /sha1 $CertificateThumbprint /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $Path | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Falha ao assinar $Path"
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $Path
    if ($signature.Status -ne "Valid" -or $signature.SignerCertificate.Thumbprint -ne $CertificateThumbprint -or -not $signature.TimeStamperCertificate) {
        throw "A assinatura de $Path não passou na validação final."
    }
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
    @{ Pattern = "Qt6Core.dll"; Label = "Qt 6 Core" },
    @{ Pattern = "libopencv_core*.dll"; Label = "OpenCV Core" },
    @{ Pattern = "libopencv_dnn*.dll"; Label = "OpenCV DNN" },
    @{ Pattern = "libopencv_imgproc*.dll"; Label = "OpenCV Image Processing" }
)
foreach ($runtimeCheck in $runtimeChecks) {
    $runtimeMatch = Get-ChildItem -LiteralPath (Join-Path $stage "bin") -Filter $runtimeCheck.Pattern -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $runtimeMatch) {
        throw "O pacote portátil está incompleto: $($runtimeCheck.Label) não foi encontrado em bin."
    }
}

New-Item -ItemType Directory -Path (Join-Path $stage "bin\data\locale\pt_BR\LC_MESSAGES") -Force | Out-Null
Copy-Item -LiteralPath $editor -Destination (Join-Path $stage "bin\firawynix-kdenlive.exe") -Force
Copy-Item -LiteralPath $visionHelper -Destination (Join-Path $stage "bin\firawynix-kdenlive-ai-vision.exe") -Force
Copy-Item -LiteralPath $translation -Destination (Join-Path $stage "bin\data\locale\pt_BR\LC_MESSAGES\kdenlive.mo") -Force
$splashDestination = Get-ChildItem -LiteralPath $stage -Recurse -File -Filter "Splash.qml" |
    Where-Object { $_.FullName -match "[\\/]org[\\/]kde[\\/]kdenlive[\\/]Splash\.qml$" } |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $splashDestination) {
    throw "O pacote portátil não contém o módulo QML da tela inicial do Kdenlive."
}
Copy-Item -LiteralPath $splashQml -Destination $splashDestination -Force
Copy-Item -LiteralPath (Join-Path $launcherOutput "Firawynix-Kdenlive-Launcher.exe") -Destination (Join-Path $stage "Firawynix-Kdenlive-Launcher.exe") -Force
if (-not $PublicBuild) {
    $whisperDestination = Join-Path $stage "bin\ai\whisper"
    New-Item -ItemType Directory -Path (Join-Path $whisperDestination "models") -Force | Out-Null
    Copy-Item -LiteralPath $whisperExecutable -Destination $whisperDestination -Force
    Get-ChildItem -LiteralPath $WhisperCppDirectory -Filter "*.dll" -File | Copy-Item -Destination $whisperDestination -Force
    Copy-Item -LiteralPath $WhisperModel -Destination (Join-Path $whisperDestination "models\ggml-small.bin") -Force
    Copy-Item -LiteralPath "C:\_\whispercpp-v1.9.4\LICENSE" -Destination (Join-Path $whisperDestination "LICENSE-whisper.cpp.txt") -Force
}
$legacyEditor = Join-Path $stage "bin\kdenlive.exe"
if (Test-Path -LiteralPath $legacyEditor) {
    Remove-Item -LiteralPath $legacyEditor -Force
}

if (-not $PublicBuild) {
    foreach ($binary in @(
        (Join-Path $stage "bin\firawynix-kdenlive.exe"),
        (Join-Path $stage "bin\firawynix-kdenlive-ai-vision.exe"),
        (Join-Path $stage "Firawynix-Kdenlive-Launcher.exe"),
        (Join-Path $whisperDestination "whisper-cli.exe")
    )) {
        Invoke-SignArtifact $binary
    }
}

$iscc = @(
    (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 7\ISCC.exe"),
    (Join-Path $CraftRoot "dev-utils\bin\ISCC.exe"),
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $iscc) {
    throw "Inno Setup não encontrado. Instale-o com: python C:\CraftRoot\craft\bin\craft.py innosetup"
}

$script = Join-Path $PSScriptRoot "firawynix-kdenlive.iss"
$innerOutput = if ($PublicBuild) { $output } else { Join-Path $output "inner" }
New-Item -ItemType Directory -Path $innerOutput -Force | Out-Null
& $iscc "/DMyAppVersion=$Version" "/DMyAppNumericVersion=$assemblyVersion" "/DStageDir=$stage" "/DOutputDir=$innerOutput" $script
if ($LASTEXITCODE -ne 0) {
    throw "Falha ao criar o instalador Inno Setup."
}

$installer = Join-Path $output "Firawynix-Kdenlive-Setup-x64.exe"
if (-not $PublicBuild) {
    Invoke-SignArtifact $innerInstaller
    $bootstrapTemplate = Join-Path $PSScriptRoot "certificate-bootstrap.iss"
    $bootstrapBuild = Join-Path $output "bootstrap"
    New-Item -ItemType Directory -Path $bootstrapBuild -Force | Out-Null
    $bootstrapDefinition = Join-Path $bootstrapBuild "build.iss"
    $quoteInno = { param([string]$Value) $Value.Replace('"', '""') }
    $definition = @(
        ('#define MyAppVersion "{0}"' -f (& $quoteInno $Version)),
        ('#define CertificateFile "{0}"' -f (& $quoteInno $certificateFile)),
        ('#define InnerInstaller "{0}"' -f (& $quoteInno $innerInstaller)),
        ('#define OutputDir "{0}"' -f (& $quoteInno $output)),
        ('#include "{0}"' -f (& $quoteInno $bootstrapTemplate))
    )
    [IO.File]::WriteAllLines($bootstrapDefinition, $definition, [Text.UTF8Encoding]::new($false))
    & $iscc $bootstrapDefinition
    if ($LASTEXITCODE -ne 0) {
        throw "Falha ao criar o bootstrap que instala o certificado CER."
    }
    Invoke-SignArtifact $installer
    Copy-Item -LiteralPath $certificateFile -Destination (Join-Path $output "Firawynix-Internal-Code-Signing.cer") -Force
}
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
