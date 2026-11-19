; Installer for PeerChalk on Windows, built by deploy/windows/package.sh with Inno Setup 6.
; Installs for the current user without administrator rights unless the user chooses otherwise.

[Setup]
AppId={{8C1B7E2A-4F5D-4C33-9B0E-6A2D1F3E7C91}
AppName=PeerChalk
AppVersion={#AppVersion}
AppPublisher=Bruno Loff
AppPublisherURL=https://brunoloff.github.io/peerchalk/
AppSupportURL=https://github.com/brunoloff/peerchalk/issues
DefaultDirName={autopf}\PeerChalk
DefaultGroupName=PeerChalk
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
SetupIconFile=..\icons\peerchalk.ico
UninstallDisplayIcon={app}\peerchalk.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{autoprograms}\PeerChalk"; Filename: "{app}\peerchalk.exe"
Name: "{autodesktop}\PeerChalk"; Filename: "{app}\peerchalk.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Run]
Filename: "{app}\peerchalk.exe"; Description: "Start PeerChalk"; Flags: nowait postinstall skipifsilent
