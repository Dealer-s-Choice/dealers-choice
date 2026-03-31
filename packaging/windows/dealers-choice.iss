#define AppName    "Dealer's Choice"
#define AppExe     "dealers-choice.exe"
#define Publisher  "Dealer's Choice Project"
#define AppURL     "https://dealer-s-choice.github.io/"
#define IssuesURL  "https://github.com/Dealer-s-Choice/dealers-choice/issues"

#ifndef VERSION
  #define VERSION "0.0.0"
#endif
#ifndef ARCH
  #define ARCH "x86_64"
#endif

[Setup]
AppName={#AppName}
AppVersion={#VERSION}
AppPublisher={#Publisher}
AppPublisherURL={#AppURL}
AppSupportURL={#IssuesURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
; Output goes to the repo root (script is in packaging/windows/)
OutputDir=..\..\
OutputBaseFilename=dealers-choice-{#VERSION}-{#ARCH}-setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Executable
Source: "..\..\_staging\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
; Runtime DLLs collected by collect-dlls.sh
Source: "..\..\_staging\*.dll";     DestDir: "{app}"; Flags: ignoreversion
; Game data (fonts, images, sounds, server.conf)
Source: "..\..\_staging\data\*"; DestDir: "{app}\data"; \
  Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}";           Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#AppExe}"; \
  Description: "Launch {#AppName}"; \
  Flags: nowait postinstall skipifsilent
