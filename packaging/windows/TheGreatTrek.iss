; Inno Setup script for the Windows installer (https://jrsoftware.org/isinfo.php, free).
; tools/package.sh runs it after staging the zip folder:
;   ISCC /DAppVersion=0.1.4 /DSourceDir=<staged folder> /DOutputDir=dist packaging\windows\TheGreatTrek.iss
; It installs into Program Files (or, if the user picks "just me", their own Programs folder),
; adds Start menu and optional desktop shortcuts, and an uninstaller. Saves, settings and high
; scores live in %APPDATA%\TheGreatTrek and are never touched by installing or uninstalling.
; The installer is not signed, so Windows SmartScreen asks the user to choose "Run anyway".

#ifndef AppVersion
  #error Pass /DAppVersion=x.y.z
#endif
#ifndef SourceDir
  #error Pass /DSourceDir=<the staged game folder>
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
; The AppId identifies the game to Windows; keep it the same in every version so upgrades replace the old one.
AppId={{137AB935-E2C2-4844-B88A-34A49DE0F49C}
AppName=The Great Trek
AppVersion={#AppVersion}
AppVerName=The Great Trek {#AppVersion}
AppPublisher=Joseph Colton
AppPublisherURL=https://github.com/josephcolton/TheGreatTrek
AppSupportURL=https://github.com/josephcolton/TheGreatTrek/issues
AppUpdatesURL=https://github.com/josephcolton/TheGreatTrek/releases
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\The Great Trek
DefaultGroupName=The Great Trek
DisableProgramGroupPage=yes
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=..\..\assets\icon\icon.ico
UninstallDisplayIcon={app}\TheGreatTrek.exe
UninstallDisplayName=The Great Trek
OutputDir={#OutputDir}
OutputBaseFilename=TheGreatTrek-{#AppVersion}-windows-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[InstallDelete]
; An upgrade starts with fresh game data, so files removed from a newer version don't linger.
Type: filesandordirs; Name: "{app}\data"
Type: filesandordirs; Name: "{app}\assets"

[Files]
Source: "{#SourceDir}\TheGreatTrek.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\CREDITS.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\The Great Trek"; Filename: "{app}\TheGreatTrek.exe"
Name: "{group}\{cm:UninstallProgram,The Great Trek}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\The Great Trek"; Filename: "{app}\TheGreatTrek.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\TheGreatTrek.exe"; Description: "{cm:LaunchProgram,The Great Trek}"; Flags: nowait postinstall skipifsilent
