Unicode true
ManifestDPIAware true
RequestExecutionLevel user
SetCompressor /SOLID lzma
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"
!include "x64.nsh"
Var UpdateMode
Var UpdatePid
Var UpdateStamp
Var UpdateProfile
Var MaintenanceMutex
Var MaintenancePhase
!ifndef HJD_TEST_PACKAGE
  !define REG_NAME "TeamHJDGameTestLauncher"
  !define SHORTCUT_GROUP "TeamHJD Game Test Launcher"
!else
  !define REG_NAME "TeamHJDGameTestLauncher-Test"
  !define SHORTCUT_GROUP "TeamHJD Game Test Launcher (검증)"
!endif
!define REG_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${REG_NAME}"
!define MUTEX_NAME "Local\${REG_NAME}-Installer"
Name "TeamHJD Game Test Launcher"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\TeamHJD Game Test Launcher"
InstallDirRegKey HKCU "${REG_KEY}" "InstallLocation"
BrandingText "TeamHJD · Happy Journey of Developers"
Icon "${APP_ICON}"
UninstallIcon "${APP_ICON}"
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "TeamHJD Game Test Launcher"
VIAddVersionKey "CompanyName" "TeamHJD"
VIAddVersionKey "FileDescription" "TeamHJD Game Test Launcher 설치"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "Copyright TeamHJD"
!define MUI_ABORTWARNING
!define MUI_CUSTOMFUNCTION_ABORT MaintenanceAbort
!define MUI_CUSTOMFUNCTION_UNABORT un.MaintenanceAbort
!define MUI_WELCOMEPAGE_TEXT "플레이테스트 런처를 이 Windows 계정에 설치합니다.$\r$\n관리자 권한은 필요하지 않습니다.$\r$\n$\r$\n기존 게임과 사용자 설정을 이어서 사용합니다. 온라인 이메일 인증과 개인정보 안내는 런처에서 별도로 진행합니다."
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipWhenUpdating
!insertmacro MUI_PAGE_WELCOME
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipWhenUpdating
!insertmacro MUI_PAGE_LICENSE "${TERMS_FILE}"
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipWhenUpdating
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\TeamHJDLauncher.exe"
!define MUI_FINISHPAGE_RUN_TEXT "런처 실행"
!define MUI_FINISHPAGE_LINK "사용 안내·개인정보·복구 방법"
!define MUI_FINISHPAGE_LINK_LOCATION "https://testlauncher.teamhjd.com/"
!define MUI_PAGE_CUSTOMFUNCTION_PRE FinishUpdate
!insertmacro MUI_PAGE_FINISH
!define MUI_UNCONFIRMPAGE_TEXT_TOP "런처 프로그램 파일과 등록된 바로가기를 제거합니다.$\r$\n$\r$\n게임 설치·사용자 설정·로그인 정보·Unity 저장 데이터와 로그는 보존됩니다. 서버의 이메일 인증 기록도 별개입니다."
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "Korean"
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

Function .onInit
  System::Call 'kernel32::CreateMutexW(p 0, i 1, w "${MUTEX_NAME}") p.r0 ?e'
  Pop $R1
  StrCpy $MaintenanceMutex $0
  ${If} $R1 == 183
  ${OrIf} $MaintenanceMutex == 0
    MessageBox MB_OK|MB_ICONSTOP "다른 설치·업데이트·제거 작업을 먼저 마쳐 주세요." /SD IDOK
    SetErrorLevel 1
    Abort
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
  ${OrIfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "Windows 10 / 11의 64비트 환경이 필요합니다." /SD IDOK
    SetErrorLevel 1
    Abort
  ${EndIf}
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/UPDATE" $R1
  ${IfNot} ${Errors}
    StrCpy $UpdateMode "1"
    ${GetOptions} $R0 "/WAITPID=" $UpdatePid
    ${GetOptions} $R0 "/WAITSTAMP=" $UpdateStamp
    ${GetOptions} $R0 "/PROFILE=" $UpdateProfile
    ${If} $UpdatePid == ""
    ${OrIf} $UpdateStamp == ""
    ${OrIf} $UpdateProfile == ""
      MessageBox MB_OK|MB_ICONSTOP "업데이트 요청 정보가 없습니다. 일반 설치로 다시 실행해 주세요." /SD IDOK
      SetErrorLevel 1
      Abort
    ${EndIf}
    SetAutoClose true
  ${EndIf}
!ifdef HJD_TEST_PACKAGE
  ${If} $UpdateProfile == ""
    GetFullPathName $UpdateProfile "$INSTDIR\..\profile"
  ${EndIf}
!endif
FunctionEnd

Function .onInstSuccess
  StrCpy $MaintenancePhase "0"
  System::Call 'kernel32::CloseHandle(p $MaintenanceMutex)'
  StrCpy $MaintenanceMutex "0"
FunctionEnd

Function .onInstFailed
  StrCpy $MaintenancePhase "0"
  ${If} $MaintenanceMutex != 0
    System::Call 'kernel32::CloseHandle(p $MaintenanceMutex)'
    StrCpy $MaintenanceMutex "0"
  ${EndIf}
FunctionEnd

Function .onGUIEnd
  ${If} $MaintenanceMutex != 0
    System::Call 'kernel32::CloseHandle(p $MaintenanceMutex)'
    StrCpy $MaintenanceMutex "0"
  ${EndIf}
FunctionEnd

Function MaintenanceAbort
  ${If} $MaintenancePhase == "1"
    Abort
  ${EndIf}
FunctionEnd

Function un.onInit
  System::Call 'kernel32::CreateMutexW(p 0, i 1, w "${MUTEX_NAME}") p.r0 ?e'
  Pop $R1
  StrCpy $MaintenanceMutex $0
  ${If} $R1 == 183
  ${OrIf} $MaintenanceMutex == 0
    MessageBox MB_OK|MB_ICONSTOP "다른 설치·업데이트 작업을 마친 뒤 제거해 주세요." /SD IDOK
    SetErrorLevel 1
    Abort
  ${EndIf}
FunctionEnd

Function un.onGUIEnd
  ${If} $MaintenanceMutex != 0
    System::Call 'kernel32::CloseHandle(p $MaintenanceMutex)'
  ${EndIf}
FunctionEnd

Function un.MaintenanceAbort
  ${If} $MaintenancePhase == "1"
    Abort
  ${EndIf}
FunctionEnd

Function SkipWhenUpdating
  ${If} $UpdateMode == "1"
    Abort
  ${EndIf}
FunctionEnd

Function FinishUpdate
  ${If} $UpdateMode == "1"
    Quit
  ${EndIf}
FunctionEnd

Section "런처 (필수)" MainSection
  SectionIn RO
  SetShellVarContext current
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\bundle"
  File /r "${BUNDLE_ROOT}\*"
  ${If} $UpdateMode == "1"
    DetailPrint "실행 중인 런처가 종료되면 업데이트를 적용합니다."
    nsExec::ExecToStack /TIMEOUT=125000 '"$PLUGINSDIR\bundle\TeamHJDLauncher.exe" --wait-client "$INSTDIR" --wait-pid $UpdatePid --wait-stamp $UpdateStamp --noninteractive-test'
    Pop $0
    Pop $1
    ${If} $0 != 0
      DetailPrint "$1"
      MessageBox MB_OK|MB_ICONSTOP "업데이트 요청자를 확인하지 못했습니다.$\r$\n$1" /SD IDOK
      SetErrorLevel 1
      Abort
    ${EndIf}
  ${EndIf}
  DetailPrint "설치 파일을 검증하고 기존 사용자 데이터를 보존합니다."
  StrCpy $MaintenancePhase "1"
  StrCpy $R2 ""
  ${If} $UpdateProfile != ""
    StrCpy $R2 '--data-root "$UpdateProfile"'
  ${EndIf}
  ${If} $UpdateMode == "1"
    StrCpy $R2 '$R2 --require-health'
  ${EndIf}
  nsExec::ExecToStack /TIMEOUT=180000 '"$PLUGINSDIR\bundle\TeamHJDLauncher.exe" --install-to "$INSTDIR" $R2 --noninteractive-test'
  Pop $0
  Pop $1
  ${If} $0 != 0
    DetailPrint "$1"
    MessageBox MB_OK|MB_ICONSTOP "설치를 완료하지 못했습니다.$\r$\n실행 중인 런처를 종료한 뒤 다시 시도해 주세요.$\r$\n$\r$\n$1" /SD IDOK
    SetErrorLevel 1
    Abort
  ${EndIf}
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "${REG_KEY}" "DisplayName" "TeamHJD Game Test Launcher"
  WriteRegStr HKCU "${REG_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${REG_KEY}" "Publisher" "TeamHJD"
  WriteRegStr HKCU "${REG_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${REG_KEY}" "DisplayIcon" "$INSTDIR\TeamHJDLauncher.exe,0"
  WriteRegStr HKCU "${REG_KEY}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKCU "${REG_KEY}" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
  WriteRegStr HKCU "${REG_KEY}" "URLInfoAbout" "https://testlauncher.teamhjd.com/"
  WriteRegStr HKCU "${REG_KEY}" "HelpLink" "https://testlauncher.teamhjd.com/#faq"
  WriteRegDWORD HKCU "${REG_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${REG_KEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  WriteRegDWORD HKCU "${REG_KEY}" "EstimatedSize" $0
!ifndef HJD_TEST_PACKAGE
  CreateDirectory "$SMPROGRAMS\${SHORTCUT_GROUP}"
  CreateShortcut "$SMPROGRAMS\${SHORTCUT_GROUP}\TeamHJD Game Test Launcher.lnk" "$INSTDIR\TeamHJDLauncher.exe"
  CreateShortcut "$SMPROGRAMS\${SHORTCUT_GROUP}\런처 제거.lnk" "$INSTDIR\Uninstall.exe"
  WriteINIStr "$SMPROGRAMS\${SHORTCUT_GROUP}\사용 안내.url" "InternetShortcut" "URL" "https://testlauncher.teamhjd.com/"
!endif
SectionEnd

Section "바탕화면 바로가기" DesktopSection
!ifndef HJD_TEST_PACKAGE
  CreateShortcut "$DESKTOP\TeamHJD Game Test Launcher.lnk" "$INSTDIR\TeamHJDLauncher.exe"
!endif
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\helper"
  File /r "${BOOTSTRAP_ROOT}\*"
  StrCpy $MaintenancePhase "1"
  nsExec::ExecToStack /TIMEOUT=180000 '"$PLUGINSDIR\helper\TeamHJDLauncher.exe" --uninstall-from "$INSTDIR" --noninteractive-test'
  Pop $0
  Pop $1
  StrCpy $MaintenancePhase "0"
  ${If} $0 != 0
    DetailPrint "$1"
    MessageBox MB_OK|MB_ICONSTOP "제거를 완료하지 못했습니다. 런처를 종료한 뒤 다시 시도해 주세요.$\r$\n$\r$\n$1" /SD IDOK
    SetErrorLevel 1
    Abort
  ${EndIf}
  ReadRegStr $0 HKCU "${REG_KEY}" "InstallLocation"
  ${If} $0 == $INSTDIR
    DeleteRegKey HKCU "${REG_KEY}"
!ifndef HJD_TEST_PACKAGE
  Delete "$SMPROGRAMS\${SHORTCUT_GROUP}\TeamHJD Game Test Launcher.lnk"
  Delete "$SMPROGRAMS\${SHORTCUT_GROUP}\런처 제거.lnk"
  Delete "$SMPROGRAMS\${SHORTCUT_GROUP}\사용 안내.url"
  RMDir "$SMPROGRAMS\${SHORTCUT_GROUP}"
  Delete "$DESKTOP\TeamHJD Game Test Launcher.lnk"
!endif
  ${EndIf}
  Delete "$INSTDIR\Uninstall.exe"
  StrCpy $R2 "0"
  FindFirst $R0 $R1 "$INSTDIR\*"
  cleanup_scan:
    ${If} $R1 == ""
      Goto cleanup_done
    ${EndIf}
    ${If} $R1 != "."
    ${AndIf} $R1 != ".."
    ${AndIf} $R1 != "teamhjd-install.json"
      StrCpy $R2 "1"
    ${EndIf}
    FindNext $R0 $R1
    Goto cleanup_scan
  cleanup_done:
  FindClose $R0
  ${If} $R2 == "0"
    Delete "$INSTDIR\teamhjd-install.json"
  ${EndIf}
  RMDir "$INSTDIR"
SectionEnd
