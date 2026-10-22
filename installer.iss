#define AppVersion "1.0.0"
#define SourcePath "build\Release"

[Setup]
AppId={{B937D33A-0E31-4C67-96A4-B39C0E6C1841}
AppName=DeskPilot
AppVersion={#AppVersion}
AppPublisher=DeskPilot
DefaultDirName={autopf}\DeskPilot
DefaultGroupName=DeskPilot
UninstallDisplayIcon={app}\DeskPilot.exe
SetupIconFile=deskpilot.ico
OutputDir=build\installer
OutputBaseFilename=DeskPilot-Setup
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "startup"; Description: "Start DeskPilot when Windows starts"; GroupDescription: "Startup:"; Flags: checkedonce
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Files]
Source: "{#SourcePath}\DeskPilot.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\DeskPilot"; Filename: "{app}\DeskPilot.exe"; IconFilename: "{app}\DeskPilot.exe"
Name: "{autodesktop}\DeskPilot"; Filename: "{app}\DeskPilot.exe"; IconFilename: "{app}\DeskPilot.exe"; Tasks: desktopicon

[Registry]
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "DeskPilot"; ValueData: """{app}\DeskPilot.exe"""; Tasks: startup; Flags: uninsdeletevalue

[Run]
Filename: "{app}\DeskPilot.exe"; Description: "Launch DeskPilot"; Flags: postinstall nowait skipifsilent
