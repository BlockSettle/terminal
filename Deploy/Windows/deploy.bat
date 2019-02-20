set PATH=%PATH%;C:\"Program Files (x86)"\NSIS

makensis.exe bsterminal.nsi
call sign.bat
move bsterminal_installer.exe ..
