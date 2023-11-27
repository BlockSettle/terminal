Name "BlockSettle Terminal"
SetCompressor /SOLID lzma

# General Symbol Definitions
!define COMPANY "BlockSettle AB"
!define URL http://blocksettle.com/
!define VERSION "0.91.2"
!define PRODUCT_NAME "BlockSettle Terminal"

# MultiUser Symbol Definitions
!define MULTIUSER_EXECUTIONLEVEL Highest
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_INSTALLMODE_INSTDIR BlockSettle
!define MULTIUSER_USE_PROGRAMFILES64

# MUI Symbol Definitions
!define MUI_ICON bs.ico
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_STARTMENUPAGE_DEFAULTFOLDER BlockSettle
!define MUI_FINISHPAGE_RUN $INSTDIR\blocksettle.exe
!define MUI_UNICON bs.ico
!define MUI_UNFINISHPAGE_NOAUTOCLOSE

# Included files
!include MultiUser.nsh
!include Sections.nsh
!include MUI2.nsh
!include logiclib.nsh
!include x64.nsh
!include "WordFunc.nsh"

#BlockSettle Branding
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "resources\nsis3-banner.bmp" ;
!define MUI_WELCOMEFINISHPAGE_BITMAP "resources\nsis3-banner.bmp" ;

#language settings
!define MUI_LANGDLL_REGISTRY_ROOT SHELL_CONTEXT
!define MUI_LANGDLL_REGISTRY_VALUENAME "NSIS:Language"
!define MUI_LANGDLL_ALLLANGUAGES
!define MUI_LANGDLL_ALWAYSSHOW

# Variables
Var StartMenuGroup

# Installer pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE LICENSE
#!define MUI_PAGE_CUSTOMFUNCTION_LEAVE "ComponentsLeave"
#!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuGroup
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

# Installer languages
!insertmacro MUI_LANGUAGE English

# Installer attributes
OutFile bsterminal_installer.exe
# No need to set InstallDir here, MultiUser init will set it ($PROGRAMFILES64 for admins, local app data for regular users)
# InstallDir "$PROGRAMFILES64\BlockSettle"
CRCCheck on
XPStyle on
Icon bs.ico
ShowInstDetails show
AutoCloseWindow true
LicenseData LICENSE
VIProductVersion "${VERSION}.0"
VIAddVersionKey ProductName "${PRODUCT_NAME}"
VIAddVersionKey ProductVersion "${VERSION}"
VIAddVersionKey CompanyName "${COMPANY}"
VIAddVersionKey CompanyWebsite "${URL}"
VIAddVersionKey Comments ""
VIAddVersionKey FileVersion "${VERSION}"
VIAddVersionKey FileDescription "BlockSettle Terminal Installer"
VIAddVersionKey LegalCopyright "Copyright (C) 2016-2023 BlockSettle AB"
UninstallIcon bs.ico
ShowUninstDetails show

#registry key for unisntalling
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

!macro CREATE_SMGROUP_SHORTCUT NAME PATH ARGS
    Push "${ARGS}"
    Push "${NAME}"
    Push "${PATH}"
    Call CreateSMGroupShortcut
!macroend

# Component selection stuff
Section "Terminal" SEC_TERM
SectionEnd


#LangString DESC_SEC_TERM ${LANG_ENGLISH} "Main terminal binary"

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_TERM} "Main terminal binary"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function ComponentsLeave
    SectionGetFlags ${SEC_TERM} $0
    StrCmp $0 1 End
    MessageBox MB_OK "You should select at least one component"
    Abort
    End:
FunctionEnd

Section "install"
    ${If} ${RunningX64}
        SetOutPath $INSTDIR
        RmDir /r $INSTDIR
        SetOverwrite on
#	${If} ${SectionIsSelected} ${SEC_TERM}
            File ..\..\build_terminal\RelWithDebInfo\bin\RelWithDebInfo\blocksettle.exe
#	${Endif}
        SetOutPath $INSTDIR\scripts
#        File ..\..\Scripts\DealerAutoQuote.qml
#        File ..\..\Scripts\RFQBot.qml
        SetOutPath $INSTDIR
        CreateShortcut "$DESKTOP\BlockSettle Terminal.lnk" $INSTDIR\blocksettle.exe
        !insertmacro CREATE_SMGROUP_SHORTCUT "BlockSettle Terminal" "$INSTDIR\blocksettle.exe" ""

        WriteUninstaller $INSTDIR\uninstall.exe
        !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
        SetOutPath $SMPROGRAMS\$StartMenuGroup
        CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Uninstall.lnk" $INSTDIR\uninstall.exe
        !insertmacro MUI_STARTMENU_WRITE_END
    ${Else}
        # 32 bit code
        MessageBox MB_OK "You cannot install this version on a 32-bit system"
    ${EndIf}
SectionEnd
#post install registry handling
Section -Post
  #To be used when running the uninstaller
  WriteRegStr SHELL_CONTEXT "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr SHELL_CONTEXT "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninstall.exe"
SectionEnd


# Uninstaller sections
!macro DELETE_SMGROUP_SHORTCUT NAME
    Push "${NAME}"
    Call un.DeleteSMGroupShortcut
!macroend

Section "Uninstall"
    Delete /REBOOTOK "$DESKTOP\BlockSettle Terminal.lnk"
    !insertmacro DELETE_SMGROUP_SHORTCUT "BlockSettle Terminal"
    Delete /REBOOTOK $INSTDIR\blocksettle.exe
    #Delete /REBOOTOK $INSTDIR\libzmq-v141-mt-4_3_2.dll
    #Delete /REBOOTOK $INSTDIR\msvcp140.dll
    #Delete /REBOOTOK $INSTDIR\vcruntime140.dll
    RmDir /r /REBOOTOK $INSTDIR

    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Uninstall.lnk"
    Delete /REBOOTOK $INSTDIR\uninstall.exe
    RmDir /r /REBOOTOK $SMPROGRAMS\$StartMenuGroup
    RmDir /r /REBOOTOK $INSTDIR
    Push $R0
    StrCpy $R0 $StartMenuGroup 1
    StrCmp $R0 ">" no_smgroup

    DeleteRegKey SHELL_CONTEXT "${PRODUCT_UNINST_KEY}"
no_smgroup:
    Pop $R0
SectionEnd

# Installer functions
Function .onInit
    ; Avoid running the installer if BlockSettle Terminal Installer is already running,
    System::Call 'kernel32::CreateMutexA(i 0, i 0, t "${PRODUCT_NAME}InstMutex") i .r1 ?e'
    Pop $R0
    StrCmp $R0 0 +3
    MessageBox MB_OK|MB_ICONEXCLAMATION \
             "The ${PRODUCT_NAME} Installer is already running."
    Abort

    ClearErrors

    InitPluginsDir
    !insertmacro MULTIUSER_INIT

FunctionEnd

Function CreateSMGroupShortcut
    Exch $R0 ;PATH
    Exch
    Exch $R1 ;NAME
    Exch 2
    Exch $R3
    Push $R2
    StrCpy $R2 $StartMenuGroup 1
    StrCmp $R2 ">" no_smgroup
    SetOutPath $SMPROGRAMS\$StartMenuGroup
    CreateShortcut "$SMPROGRAMS\$StartMenuGroup\$R1.lnk" $R0 $R3
no_smgroup:
    Pop $R2
    Pop $R1
    Pop $R0
FunctionEnd

# Uninstaller functions
Function un.onInit
    !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuGroup
    !insertmacro MULTIUSER_UNINIT
FunctionEnd

Function un.DeleteSMGroupShortcut
    Exch $R1 ;NAME
    Push $R2
    StrCpy $R2 $StartMenuGroup 1
    StrCmp $R2 ">" no_smgroup
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\$R1.lnk"
no_smgroup:
    Pop $R2
    Pop $R1
FunctionEnd
