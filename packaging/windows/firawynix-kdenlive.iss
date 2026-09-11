#ifndef MyAppVersion
  #define MyAppVersion "26.11.70-firaw.4"
#endif
#ifndef StageDir
  #define StageDir "..\\..\\dist\\windows\\stage"
#endif
#ifndef OutputDir
  #define OutputDir "..\\..\\dist\\windows"
#endif

[Setup]
AppId={{C5D58DF5-57CC-49D6-AF4A-6F7E88C8F106}
AppName=Firawynix - Kdenlive
AppVersion={#MyAppVersion}
AppVerName=Firawynix - Kdenlive {#MyAppVersion}
AppPublisher=Firawynix e colaboradores do Kdenlive
AppPublisherURL=https://github.com/firawynix/kdenlive
AppSupportURL=https://github.com/firawynix/kdenlive/issues
AppUpdatesURL=https://github.com/firawynix/kdenlive/releases
VersionInfoCompany=Firawynix / KDE Community
VersionInfoDescription=Instalador do Firawynix - Kdenlive
VersionInfoProductName=Firawynix - Kdenlive
VersionInfoVersion=26.11.70.4
DefaultDirName={autopf}\Firawynix Kdenlive
DefaultGroupName=Firawynix - Kdenlive
DisableProgramGroupPage=yes
LicenseFile=..\..\COPYING
OutputDir={#OutputDir}
OutputBaseFilename=Firawynix-Kdenlive-Setup-x64
SetupIconFile=..\..\data\icons\kdenlive.ico
UninstallDisplayIcon={app}\Firawynix-Kdenlive-Launcher.exe
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
CloseApplications=yes
RestartApplications=no
ChangesAssociations=yes

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Criar um atalho na área de trabalho"; GroupDescription: "Atalhos adicionais:"
Name: "associate"; Description: "Associar projetos .kdenlive ao Firawynix - Kdenlive"; GroupDescription: "Associação de arquivos:"; Flags: checkedonce

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Firawynix - Kdenlive"; Filename: "{app}\Firawynix-Kdenlive-Launcher.exe"; IconFilename: "{app}\bin\firawynix-kdenlive.exe"
Name: "{autodesktop}\Firawynix - Kdenlive"; Filename: "{app}\Firawynix-Kdenlive-Launcher.exe"; IconFilename: "{app}\bin\firawynix-kdenlive.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.kdenlive\OpenWithProgids"; ValueType: string; ValueName: "Firawynix.Kdenlive.Project"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Firawynix.Kdenlive.Project"; ValueType: string; ValueName: ""; ValueData: "Projeto do Firawynix - Kdenlive"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Firawynix.Kdenlive.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\bin\firawynix-kdenlive.exe,0"; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Firawynix.Kdenlive.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Firawynix-Kdenlive-Launcher.exe"" ""%1"""; Tasks: associate

[Run]
Filename: "{app}\Firawynix-Kdenlive-Launcher.exe"; Description: "Abrir o Firawynix - Kdenlive"; Flags: nowait postinstall skipifsilent
