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
; The binaries are 64-bit, so run the installer in 64-bit mode. Without this,
; the 32-bit Inno stub stays in 32-bit mode and {autopf} resolves to
; "Program Files (x86)" instead of the real "Program Files".
; (Requires Inno Setup 6.3+ for the x64compatible/arm64 identifiers.)
#if ARCH == "aarch64"
ArchitecturesAllowed=arm64
ArchitecturesInstallIn64BitMode=arm64
#else
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#endif
; Output goes to the repo root (script is in packaging/windows/)
OutputDir=..\..\
OutputBaseFilename=dealers-choice-{#VERSION}-windows-{#ARCH}-setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\..\icons\dealers-choice.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Executables
Source: "..\..\_staging\{#AppExe}";              DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\_staging\dealers-choice-bot.exe"; DestDir: "{app}"; Flags: ignoreversion
; Runtime DLLs collected by collect-dlls.sh
Source: "..\..\_staging\*.dll";     DestDir: "{app}"; Flags: ignoreversion
; Game data (fonts, images, sounds, server.conf)
Source: "..\..\_staging\data\*"; DestDir: "{app}\data"; \
  Flags: ignoreversion recursesubdirs createallsubdirs
; Icon
Source: "..\..\icons\dealers-choice.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";           Filename: "{app}\{#AppExe}"; IconFilename: "{app}\dealers-choice.ico"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#AppExe}"; \
  Description: "Launch {#AppName}"; \
  Flags: nowait postinstall skipifsilent
