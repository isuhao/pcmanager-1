

!system "if exist version.inc.nsi del version.inc.nsi"
!system "getver.exe ..\..\product\win32\ksafever.dll"
!include "version.inc.nsi"

!define PRODUCT_PARTNER  "O_CONEW"                      ;合作厂商
!define PRODUCT_OUT_FILE "..\..\product\package\setup_conew.exe"          ;生成安装包的名字
!define MACRO_CFG_PATH  "cfg_enable_mon"
!define WRITE_TRAY_REG_RUN 1
!include "leidian_install.nsh"

