[Setup]
AppId=Firawynix.CertificateBootstrap.kdenlive
AppName=Firawynix - Kdenlive
AppVersion={#MyAppVersion}
AppVerName=Firawynix - Kdenlive {#MyAppVersion}
AppPublisher=Firawynix
AppPublisherURL=https://firawynix.com.br
VersionInfoCompany=Firawynix
VersionInfoDescription=Instalador confiavel do Firawynix - Kdenlive
VersionInfoProductName=Firawynix - Kdenlive
DefaultDirName={tmp}\Firawynix\kdenlive
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
DisableFinishedPage=yes
CreateUninstallRegKey=no
Uninstallable=no
OutputDir={#OutputDir}
OutputBaseFilename=Firawynix-Kdenlive-Setup-x64
Compression=none
SolidCompression=no
WizardStyle=modern
PrivilegesRequired=admin
CloseApplications=no
RestartApplications=no
SetupLogging=yes

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Files]
Source: "{#CertificateFile}"; DestDir: "{tmp}"; DestName: "Firawynix-Internal-Code-Signing.cer"; Flags: ignoreversion deleteafterinstall
Source: "{#InnerInstaller}"; DestDir: "{tmp}"; DestName: "Firawynix-Kdenlive-Original.exe"; Flags: ignoreversion deleteafterinstall

[Run]
Filename: "{sys}\certutil.exe"; Parameters: "-addstore -f Root ""{tmp}\Firawynix-Internal-Code-Signing.cer"""; StatusMsg: "Validando a assinatura da Firawynix..."; Flags: runhidden waituntilterminated
Filename: "{sys}\certutil.exe"; Parameters: "-addstore -f TrustedPublisher ""{tmp}\Firawynix-Internal-Code-Signing.cer"""; StatusMsg: "Registrando a Firawynix como fornecedor confiavel..."; Flags: runhidden waituntilterminated
Filename: "{tmp}\Firawynix-Kdenlive-Original.exe"; Parameters: "{code:InnerParameters}"; StatusMsg: "Abrindo o instalador do Firawynix - Kdenlive..."; Flags: waituntilterminated

[Code]
function InnerParameters(Param: String): String;
var
  RequestedDir: String;
begin
  Result := '';
  if not WizardSilent then
    Exit;
  RequestedDir := ExpandConstant('{param:DIR|}');
  Result := '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-';
  if RequestedDir <> '' then
    Result := Result + ' /DIR="' + RequestedDir + '"';
end;
