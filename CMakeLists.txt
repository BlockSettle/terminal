CMAKE_MINIMUM_REQUIRED( VERSION 3.3 )

SET(CMAKE_CXX_STANDARD 14)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_EXTENSIONS OFF)

SET(PROJECT_NAME_STR BS_Terminal)
PROJECT(${PROJECT_NAME_STR})

IF (NOT CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE "Debug")
endif()

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
   SET(THIRD_PARTY_BUILD_MODE "debug")
ELSE(CMAKE_BUILD_TYPE STREQUAL "Debug")
   SET(THIRD_PARTY_BUILD_MODE "release")
ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")

add_definitions(-DNOMINMAX)

# Force Armory to ignore Crypto++ and use libbtc instead.
add_definitions(-DLIBBTC_ONLY)

IF (APPLE)
   add_definitions(-DMDB_USE_POSIX_SEM=1)
ENDIF(APPLE)

IF (PRODUCTION_BUILD)
   add_definitions(-DPRODUCTION_BUILD)
ENDIF (PRODUCTION_BUILD)

# Force 64-bit builds and min target for macOS. If the min target changes,
# update generate.py too.
if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12 CACHE STRING "" FORCE)
	set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "macOS architecture to build; 64-bit is expected" FORCE)
endif(APPLE)

# Enforce Windows 7 minimum target and MSVS 2017 compiler minimum.
IF(WIN32)
	SET(_WIN32_WINNT 0x0601 CACHE INTERNAL "Setting _WIN32_WINNT to 0x0601 for Windows 7 minimum APIs.")
	SET(WINVER 0x0601 CACHE INTERNAL "Setting WINVER to 0x0601 for Windows 7 minimum APIs.")

	IF(MSVC_VERSION LESS 1910)
		MESSAGE(FATAL_ERROR "MSVC 141 or higher is required to build the BlockSettle Terminal.")
	ENDIF()
ENDIF()

# setup directories
SET(BLOCK_SETTLE_ROOT ${CMAKE_SOURCE_DIR})

set(CMAKE_MODULE_PATH "${BLOCK_SETTLE_ROOT}/common/build_scripts/CMakeModules")

set(QML_IMPORT_PATH "${BLOCK_SETTLE_ROOT}/BlockSettleSigner/qml" CACHE STRING "QML import path")
set(QML2_IMPORT_PATH "${BLOCK_SETTLE_ROOT}/BlockSettleSigner/qml" CACHE STRING "QML2 import path")

# set versions info
# Terminal, PB, PuB and offline tools share same version for consistency
SET(BS_VERSION_MAJOR 0 )
SET(BS_VERSION_MINOR 13 )
SET(BS_VERSION_PATCH 6 )
SET(BS_VERSION_STRING "${BS_VERSION_MAJOR}.${BS_VERSION_MINOR}.${BS_VERSION_PATCH}")

include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)

IF( GIT_SHA1 STREQUAL GITDIR-NOTFOUND )
  MESSAGE( FATAL_ERROR "THis is not a Git repository, but it should be.")
ELSE ( GIT_SHA1 STREQUAL GITDIR-NOTFOUND )
   # we only take the 8 first characters
  string(SUBSTRING ${GIT_SHA1} 0 8 BS_REVISIOIN_SHA1)
ENDIF ( GIT_SHA1 STREQUAL GITDIR-NOTFOUND )

# set DEV_3RD_ROOT env variable to your custom location of 3rd party components
IF (DEFINED ENV{DEV_3RD_ROOT})
   GET_FILENAME_COMPONENT(FULL_3RD_PATH $ENV{DEV_3RD_ROOT} ABSOLUTE)
   SET(THIRD_PARTY_COMMON_DIR ${FULL_3RD_PATH}/${THIRD_PARTY_BUILD_MODE})
ELSE()
   GET_FILENAME_COMPONENT(TOP_DIRECTORY ${BLOCK_SETTLE_ROOT} DIRECTORY)
   SET(THIRD_PARTY_COMMON_DIR ${TOP_DIRECTORY}/3rd/${THIRD_PARTY_BUILD_MODE})
ENDIF()

SET( PROJECT_BINARY_DIR ${BLOCK_SETTLE_ROOT}/build_terminal/${CMAKE_BUILD_TYPE} )


SET( EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin )
IF( WIN32 )
   # output libs to same path as binaries on windows just to make life easier
   SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
   SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
   add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
   SET( LIBRARY_OUTPUT_PATH ${EXECUTABLE_OUTPUT_PATH} )
ELSE()
   SET( LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/libs )
ENDIF()

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

#find qt
# Instruct CMake to run moc automatically when needed.
SET(CMAKE_AUTOMOC ON)

# set required dirs
SET(QT5_ROOT ${THIRD_PARTY_COMMON_DIR}/Qt5)
SET(QT5_BIN_ROOT ${QT5_ROOT}/bin)
SET(QT5_LIB_ROOT ${QT5_ROOT}/lib)
SET(QT5_QML_ROOT ${QT5_ROOT}/qml)
SET(QT5_PLUGINS_ROOT ${QT5_ROOT}/plugins)

SET(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${QT5_LIB_ROOT}/cmake")

FIND_PACKAGE(Qt5Core REQUIRED)
FIND_PACKAGE(Qt5Gui REQUIRED)
FIND_PACKAGE(Qt5Widgets REQUIRED)
FIND_PACKAGE(Qt5Network REQUIRED)
FIND_PACKAGE(Qt5PrintSupport REQUIRED)
FIND_PACKAGE(Qt5Qml REQUIRED)
FIND_PACKAGE(Qt5Quick REQUIRED)
FIND_PACKAGE(Qt5QuickControls2 REQUIRED)
FIND_PACKAGE(Qt5Charts REQUIRED)
FIND_PACKAGE(Qt5Sql REQUIRED)
FIND_PACKAGE(Qt5Svg REQUIRED)

IF ( UNIX AND NOT APPLE )
   FIND_PACKAGE(Qt5DBus REQUIRED)
   ADD_DEFINITIONS( "-DBS_USE_DBUS" )
ELSE ()
   FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dbus_dummy.c "")
   ADD_LIBRARY(dbus_dummy ${CMAKE_CURRENT_BINARY_DIR}/dbus_dummy.c)
   ADD_LIBRARY(Qt5::DBus STATIC IMPORTED)
   IF (MSVC_IDE)
      SET_PROPERTY(TARGET Qt5::DBus PROPERTY
         IMPORTED_LOCATION ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE}/${CMAKE_STATIC_LIBRARY_PREFIX}dbus_dummy${CMAKE_STATIC_LIBRARY_SUFFIX})
   ELSE ()
      SET_PROPERTY(TARGET Qt5::DBus PROPERTY
         IMPORTED_LOCATION ${LIBRARY_OUTPUT_PATH}/${CMAKE_STATIC_LIBRARY_PREFIX}dbus_dummy${CMAKE_STATIC_LIBRARY_SUFFIX})
   ENDIF ()
ENDIF ()

ADD_DEFINITIONS( -DQT_NO_CAST_TO_ASCII -DQT_NO_CAST_FROM_ASCII )
IF(NOT WIN32)
   ADD_DEFINITIONS(-Wno-multichar -Wextra -Wall -Wformat=2)
ENDIF(NOT WIN32)

#setup zeromq
SET(ZEROMQ_ROOT         ${THIRD_PARTY_COMMON_DIR}/ZeroMQ)
SET(ZEROMQ_INCLUDE_DIR  ${ZEROMQ_ROOT}/include)
SET(ZEROMQ_LIB_DIR      ${ZEROMQ_ROOT}/lib)

INCLUDE_DIRECTORIES( ${ZEROMQ_INCLUDE_DIR} )

SET(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${ZEROMQ_ROOT}/lib)
IF( WIN32 )
   IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
      SET( ZMQ_LIB_NAME "libzmq-v140-mt-gd-4_3_1" )
   ELSE()
      SET( ZMQ_LIB_NAME "libzmq-v140-mt-4_3_1" )
   ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")
ELSE ()
   SET( ZMQ_LIB_NAME "libzmq.a" "zmq" )
ENDIF( WIN32 )
FIND_LIBRARY(ZMQ_LIB NAMES ${ZMQ_LIB_NAME} REQUIRED NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)

IF( NOT ZMQ_LIB )
   IF( WIN32 )
      IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
         SET( ZMQ_LIB_NAME "libzmq-v141-mt-gd-4_3_1" )
      ELSE()
         SET( ZMQ_LIB_NAME "libzmq-v141-mt-4_3_1" )
      ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")
   ENDIF( WIN32 )
   FIND_LIBRARY(ZMQ_LIB NAMES ${ZMQ_LIB_NAME} REQUIRED NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
   IF( NOT ZMQ_LIB)
      MESSAGE(FATAL_ERROR "Could not find ZMQ lib")
   ENDIF( NOT ZMQ_LIB)
ENDIF( NOT ZMQ_LIB)



IF( WIN32 )
   STRING(REPLACE ".lib" ".dll" ZMQ_LIB_DLL ${ZMQ_LIB})

   IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
     FILE(COPY ${ZMQ_LIB_DLL} DESTINATION ${LIBRARY_OUTPUT_PATH}/Debug)
   ELSE()
     FILE(COPY ${ZMQ_LIB_DLL} DESTINATION ${LIBRARY_OUTPUT_PATH}/Release)
   ENDIF()
ENDIF()

# OpenSSL libs
SET(OPENSSL_ROOT         ${THIRD_PARTY_COMMON_DIR}/OpenSSL)
SET(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${OPENSSL_ROOT}/lib)
SET(OPENSSL_INCLUDE_DIR ${OPENSSL_ROOT}/include)

IF( WIN32 )
   SET(OPENSSL_LIB_NAME "libssl")
   SET(OPENSSL_CRYPTO_LIB_NAME "libcrypto")
ELSE()
   SET(OPENSSL_LIB_NAME "libssl.a" "libssl" "ssl")
   SET(OPENSSL_CRYPTO_LIB_NAME "libcrypto.a" "libcrypto" "crypto")
ENDIF( WIN32 )

FIND_LIBRARY(OPENSSL_LIB NAMES ${OPENSSL_LIB_NAME} REQUIRED NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
IF ( NOT OPENSSL_LIB )
      MESSAGE(FATAL_ERROR "Could not find OpenSSL lib")
ENDIF( NOT OPENSSL_LIB)

FIND_LIBRARY(OPENSSL_CRYPTO_LIB NAMES ${OPENSSL_CRYPTO_LIB_NAME} REQUIRED NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
IF ( NOT OPENSSL_CRYPTO_LIB )
      MESSAGE(FATAL_ERROR "Could not find OpenSSL crypto lib")
ENDIF( NOT OPENSSL_CRYPTO_LIB)

SET(OPENSSL_LIBS
   ${OPENSSL_LIB}
   ${OPENSSL_CRYPTO_LIB})
INCLUDE_DIRECTORIES( ${OPENSSL_INCLUDE_DIR} )

# SET(OPENSSL_ROOT_DIR ${OPENSSL_ROOT})
# SET(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_ROOT}/lib)

# setup botan
SET(BOTAN_PACKAGE_ROOT ${THIRD_PARTY_COMMON_DIR}/botan)
SET(BOTAN_LIB_DIR ${BOTAN_PACKAGE_ROOT}/lib)
SET(BOTAN_INCLUDE_DIR ${BOTAN_PACKAGE_ROOT}/include/botan-2)

IF (WIN32)
   SET(BOTAN_LIB_NAME botan-2.lib botan.lib)
ELSE(WIN32)
   SET(BOTAN_LIB_NAME libbotan-2.a)
ENDIF(WIN32)

FIND_LIBRARY( BOTAN_LIB NAMES ${BOTAN_LIB_NAME} PATHS ${BOTAN_LIB_DIR} NO_DEFAULT_PATH )
IF( NOT BOTAN_LIB)
   MESSAGE( FATAL_ERROR "Could not find botan lib in ${BOTAN_LIB_DIR}")
ENDIF()


# setup libwebsockets
SET(WS_PACKAGE_ROOT ${THIRD_PARTY_COMMON_DIR}/libwebsockets)
SET(WS_LIB_DIR ${WS_PACKAGE_ROOT}/lib)
SET(WS_INCLUDE_DIR ${WS_PACKAGE_ROOT}/include)

IF (WIN32)
   SET(WS_LIB_NAME websockets_static)
ELSE(WIN32)
   SET(WS_LIB_NAME libwebsockets.a)
ENDIF(WIN32)

FIND_LIBRARY(WS_LIB NAMES ${WS_LIB_NAME} PATHS ${WS_LIB_DIR} NO_DEFAULT_PATH )
IF(NOT WS_LIB)
   MESSAGE(FATAL_ERROR "Could not find libwebsockets in ${WS_LIB_DIR}")
ENDIF()
INCLUDE_DIRECTORIES(${WS_INCLUDE_DIR})

#add lib qrencode
SET(QRENCODE_ROOT             ${THIRD_PARTY_COMMON_DIR}/LibQREncode)
SET(QRENCODE_LIB_DIR          ${QRENCODE_ROOT}/lib)
SET(QRENCODE_INCLUDE_DIR      ${QRENCODE_ROOT}/include)

SET(QRENCODE_LIB_NAME "qrencode")
IF (WIN32)
   IF (CMAKE_BUILD_TYPE STREQUAL "Debug")
      SET(QRENCODE_LIB_NAME "qrencoded")
   ENDIF ()
ENDIF (WIN32)
FIND_LIBRARY(QRENCODE_LIB NAMES ${QRENCODE_LIB_NAME} REQUIRED PATHS ${QRENCODE_LIB_DIR} NO_DEFAULT_PATH)
IF(NOT QRENCODE_LIB)
   MESSAGE(FATAL_ERROR "Could not find QrEncode lib")
ENDIF(NOT QRENCODE_LIB)

INCLUDE_DIRECTORIES( ${QRENCODE_INCLUDE_DIR} )

# add SPDLOG
SET(SPDLOG_INCLUDE_DIR ${THIRD_PARTY_COMMON_DIR}/SPDLog/include)
INCLUDE_DIRECTORIES( ${SPDLOG_INCLUDE_DIR} )

# setup libbtc
SET(LIBBTC_PACKAGE_ROOT ${THIRD_PARTY_COMMON_DIR}/libbtc)
SET(LIBBTC_LIB_DIR ${LIBBTC_PACKAGE_ROOT}/lib)
SET(LIBBTC_INCLUDE_DIR ${LIBBTC_PACKAGE_ROOT}/include)

IF (WIN32)
   SET(LIBBTC_LIB_NAME libbtc)
ELSE(WIN32)
   SET(LIBBTC_LIB_NAME liblibbtc.a)
ENDIF(WIN32)

FIND_LIBRARY( LIBBTC_LIB NAMES ${LIBBTC_LIB_NAME} PATHS ${LIBBTC_LIB_DIR} NO_DEFAULT_PATH )
IF( NOT LIBBTC_LIB)
   MESSAGE( FATAL_ERROR "Could not find libbtc in ${LIBBTC_LIB_DIR}")
ENDIF()
INCLUDE_DIRECTORIES( ${LIBBTC_INCLUDE_DIR} )

# setup libchacha20poly1305
SET(LIBCP_PACKAGE_ROOT ${THIRD_PARTY_COMMON_DIR}/libchacha20poly1305)
SET(LIBCP_LIB_DIR ${LIBCP_PACKAGE_ROOT}/lib)
SET(LIBCP_INCLUDE_DIR ${LIBCP_PACKAGE_ROOT}/include)

SET(LIBCP_LIB_NAME libchacha20poly1305)

FIND_LIBRARY(LIBCP_LIB NAMES ${LIBCP_LIB_NAME} PATHS ${LIBCP_LIB_DIR} NO_DEFAULT_PATH )
IF( NOT LIBCP_LIB)
   MESSAGE( FATAL_ERROR "Could not find libchacha20poly1305 in ${LIBCP_LIB_DIR}")
ENDIF()
INCLUDE_DIRECTORIES( ${LIBCP_INCLUDE_DIR} )

SET(MPIR_PACKAGE_ROOT ${THIRD_PARTY_COMMON_DIR}/mpir)
SET(MPIR_LIB_DIR ${MPIR_PACKAGE_ROOT}/lib)
IF (WIN32)
   SET(MPIR_LIB_NAME mpir)
ELSE(WIN32)
   SET(MPIR_LIB_NAME libgmp.a)
ENDIF(WIN32)
FIND_LIBRARY( MPIR_LIB NAMES ${MPIR_LIB_NAME} PATHS ${MPIR_LIB_DIR} NO_DEFAULT_PATH )
IF( NOT MPIR_LIB)
   MESSAGE( FATAL_ERROR "Could not find MPIR lib in ${MPIR_LIB_DIR}")
ENDIF()

# setup protobuf
SET( CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${THIRD_PARTY_COMMON_DIR}/Protobuf )
FIND_PACKAGE( Protobuf REQUIRED )
INCLUDE_DIRECTORIES( ${PROTOBUF_INCLUDE_DIRS} )

IF ( APPLE )
   GET_FILENAME_COMPONENT(ProtoLibDir ${Protobuf_LIBRARY} DIRECTORY)
   FIND_LIBRARY( PROTO_LIB NAMES "libprotobuf.a" PATHS ${ProtoLibDir} NO_DEFAULT_PATH )
ELSE ( APPLE )
   SET( PROTO_LIB ${PROTOBUF_LIBRARIES} )
ENDIF ( APPLE )

# autheid helper sources
INCLUDE_DIRECTORIES( ${BLOCK_SETTLE_ROOT}/AuthAPI/utils/cpp )


SET( PATH_TO_GENERATED ${BLOCK_SETTLE_ROOT}/generated_proto )
FILE( MAKE_DIRECTORY ${PATH_TO_GENERATED} )

SET( AUTH_PROTO_LIB_NAME               AuthAPI )
SET( BS_PROTO_LIB_NAME                 BsProtoLib )
SET( CELER_PROTO_LIB_NAME              CelerProtoLib )
SET( BLOCKSETTLE_APP_NAME              blocksettle )
SET( SIGNER_APP_NAME                   blocksettle_signer )
SET( BLOCKSETTLE_UI_LIBRARY_NAME       bsuilib )
SET( CRYPTO_LIB_NAME                   ArmoryCryptoLib )
SET( CPP_WALLET_LIB_NAME               ArmoryWalletLib )
SET( BS_NETWORK_LIB_NAME               BSNetwork )
SET( COMMON_LIB_NAME                   CommonLib )
SET( COMMON_UI_LIB_NAME                CommonUI )

SET( BLOCKSETTLE_UI_INCLUDE_DIR ${BLOCK_SETTLE_ROOT}/common/BlockSettleUILib)
SET( CRYPTO_LIB_DIR ${BLOCK_SETTLE_ROOT}/common/cppForSwig )
SET( CRYPTO_LIB_INCLUDE_DIR ${CRYPTO_LIB_DIR} )
SET( WALLET_LIB_INCLUDE_DIR ${BLOCK_SETTLE_ROOT}/common/WalletsLib )
SET( BS_NETWORK_INCLUDE_DIR ${BLOCK_SETTLE_ROOT}/common/BlocksettleNetworkingLib )
SET( COMMON_LIB_INCLUDE_DIR ${BLOCK_SETTLE_ROOT}/common/CommonLib )
SET( COMMON_UI_LIB_INCLUDE_DIR ${BLOCK_SETTLE_ROOT}/common/CommonUI )

SET(MDB_DIR ${CRYPTO_LIB_DIR}/lmdb/libraries/liblmdb)

INCLUDE_DIRECTORIES( ${CRYPTOPP_EXTRA_DIR} )
INCLUDE_DIRECTORIES( ${MDB_DIR} )

SET( QT_LINUX_LIBS )

IF(WIN32)
   # setup leveldb_windows_port dirs
   SET( LEVELDB_WINDOWS_PORT_DIR  ${CRYPTO_LIB_DIR}/leveldb_windows_port/win32_posix)
   INCLUDE_DIRECTORIES(${LEVELDB_WINDOWS_PORT_DIR})

   ADD_DEFINITIONS(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
   ADD_DEFINITIONS(-DWIN32_LEAN_AND_MEAN)
 ENDIF(WIN32)

IF(WIN32)
   SET(OS_SPECIFIC_LIBS
      Ws2_32
      Wtsapi32
      Iphlpapi
      imm32
      winmm
      dwmapi
      netapi32
      crypt32
      userenv
      version
      UxTheme
      Qt5::QWindowsIntegrationPlugin
      Qt5::QICOPlugin
      Qt5::QWindowsPrinterSupportPlugin)

   IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
      SET(QT_LIBS
         qtharfbuzzd.lib
         qtpcre2d.lib
         qtlibpngd.lib
         Qt5FontDatabaseSupportd.lib
         Qt5EventDispatcherSupportd.lib
         Qt5ThemeSupportd.lib
         Qt5WindowsUIAutomationSupportd.lib
         Qt5AccessibilitySupportd.lib
         Qt5Chartsd.lib )
      LIST(APPEND OS_SPECIFIC_LIBS
         msvcrtd
         ${QT5_PLUGINS_ROOT}/sqldrivers/qsqlited.lib
      )
      SET(QT_QUICK_LIBS
         ${QT5_LIB_ROOT}/Qt5Quickd.lib
         ${QT5_LIB_ROOT}/Qt5QuickWidgetsd.lib
         ${QT5_LIB_ROOT}/Qt5QuickControls2d.lib
         ${QT5_LIB_ROOT}/Qt5QuickTemplates2d.lib
         ${QT5_QML_ROOT}/QtQml/Models.2/modelsplugind.lib
         ${QT5_QML_ROOT}/QtQuick.2/qtquick2plugind.lib
         ${QT5_QML_ROOT}/QtQuick/Controls/qtquickcontrolsplugind.lib
         ${QT5_QML_ROOT}/QtQuick/Controls.2/qtquickcontrols2plugind.lib
         ${QT5_QML_ROOT}/QtQuick/Dialogs/dialogplugind.lib
         ${QT5_QML_ROOT}/QtQuick/Dialogs/Private/dialogsprivateplugind.lib
         ${QT5_QML_ROOT}/QtQuick/Layouts/qquicklayoutsplugind.lib
         ${QT5_QML_ROOT}/QtQuick/Templates.2/qtquicktemplates2plugind.lib
         ${QT5_QML_ROOT}/QtQuick/Window.2/windowplugind.lib
         ${QT5_QML_ROOT}/Qt/Labs/folderlistmodel/qmlfolderlistmodelplugind.lib
         ${QT5_QML_ROOT}/Qt/Labs/settings/qmlsettingsplugind.lib
      )
   ELSE()
      SET(QT_LIBS
         qtharfbuzz.lib
         qtpcre2.lib
         qtlibpng.lib
         Qt5FontDatabaseSupport.lib
         Qt5EventDispatcherSupport.lib
         Qt5ThemeSupport.lib
         Qt5WindowsUIAutomationSupport.lib
         Qt5AccessibilitySupport.lib
         Qt5Charts.lib)
      LIST(APPEND OS_SPECIFIC_LIBS
         msvcrt
         ${QT5_PLUGINS_ROOT}/sqldrivers/qsqlite.lib
      )
      SET(QT_QUICK_LIBS
         ${QT5_LIB_ROOT}/Qt5Quick.lib
         ${QT5_LIB_ROOT}/Qt5QuickWidgets.lib
         ${QT5_LIB_ROOT}/Qt5QuickControls2.lib
         ${QT5_LIB_ROOT}/Qt5QuickTemplates2.lib
         ${QT5_QML_ROOT}/QtQml/Models.2/modelsplugin.lib
         ${QT5_QML_ROOT}/QtQuick.2/qtquick2plugin.lib
         ${QT5_QML_ROOT}/QtQuick/Controls/qtquickcontrolsplugin.lib
         ${QT5_QML_ROOT}/QtQuick/Controls.2/qtquickcontrols2plugin.lib
         ${QT5_QML_ROOT}/QtQuick/Dialogs/dialogplugin.lib
         ${QT5_QML_ROOT}/QtQuick/Dialogs/Private/dialogsprivateplugin.lib
         ${QT5_QML_ROOT}/QtQuick/Layouts/qquicklayoutsplugin.lib
         ${QT5_QML_ROOT}/QtQuick/Templates.2/qtquicktemplates2plugin.lib
         ${QT5_QML_ROOT}/QtQuick/Window.2/windowplugin.lib
         ${QT5_QML_ROOT}/Qt/labs/folderlistmodel/qmlfolderlistmodelplugin.lib
         ${QT5_QML_ROOT}/Qt/labs/settings/qmlsettingsplugin.lib
      )
   ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")
ELSE(WIN32)
   FIND_PACKAGE( ZLIB )
   IF ( NOT ZLIB_FOUND )
      MESSAGE(FATAL_ERROR "ZLIB not found")
   ENDIF( NOT ZLIB_FOUND )

   IF ( APPLE )
      FIND_PACKAGE( CUPS )
      IF ( NOT CUPS_FOUND )
         MESSAGE(FATAL_ERROR "CUPS not found")
      ENDIF( NOT CUPS_FOUND )

      SET(OS_SPECIFIC_LIBS
         "-framework Foundation"
         "-framework CoreFoundation"
         "-framework CoreServices"
         "-framework CoreText"
         "-framework AppKit"
         "-framework Cocoa"
         "-framework Carbon"
         "-framework IOKit"
         "-framework Security"
         "-framework SystemConfiguration"
         "-framework QuartzCore"
         "-framework Metal"
         Qt5::QCocoaIntegrationPlugin
         Qt5::PrintSupport
         Qt5::QICOPlugin
         Qt5::QCocoaPrinterSupportPlugin
         ${ZLIB_LIBRARIES}
         ${CUPS_LIBRARIES})

      IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
         SET(QT_LIBS
            libqtharfbuzz_debug.a
            libqtpcre2_debug.a
            libqtlibpng_debug.a
            libQt5EventDispatcherSupport_debug.a
            libQt5ThemeSupport_debug.a
            libQt5FontDatabaseSupport_debug.a
            libQt5ClipboardSupport_debug.a
            libQt5GraphicsSupport_debug.a
            libQt5AccessibilitySupport_debug.a
            libQt5EdidSupport_debug.a
            libQt5Charts_debug.a
            )
         LIST (APPEND OS_SPECIFIC_LIBS
            ${QT5_PLUGINS_ROOT}/sqldrivers/libqsqlite_debug.a
         )
         SET(QT_QUICK_LIBS
            ${QT5_QML_ROOT}/QtQml/Models.2/libmodelsplugin_debug.a
            ${QT5_QML_ROOT}/QtQuick.2/libqtquick2plugin_debug.a
            ${QT5_QML_ROOT}/QtQuick/Controls/libqtquickcontrolsplugin_debug.a
            ${QT5_QML_ROOT}/QtQuick/Controls.2/libqtquickcontrols2plugin_debug.a
            ${QT5_QML_ROOT}/QtQuick/Dialogs/libdialogplugin_debug.a
            ${QT5_QML_ROOT}/QtQuick/Dialogs/Private/libdialogsprivateplugin_debug.a
            ${QT5_QML_ROOT}/QtQuick/Layouts/libqquicklayoutsplugin_debug.a
            ${QT5_QML_ROOT}/QtQuick/Templates.2/libqtquicktemplates2plugin_debug.a
            ${QT5_QML_ROOT}/QtQuick/Window.2/libwindowplugin_debug.a
            ${QT5_QML_ROOT}/Qt/labs/folderlistmodel/libqmlfolderlistmodelplugin_debug.a
            ${QT5_QML_ROOT}/Qt/labs/settings/libqmlsettingsplugin_debug.a
            ${QT5_LIB_ROOT}/libQt5Quick_debug.a
            ${QT5_LIB_ROOT}/libQt5QuickWidgets_debug.a
            ${QT5_LIB_ROOT}/libQt5QuickControls2_debug.a
            ${QT5_LIB_ROOT}/libQt5QuickTemplates2_debug.a
         )
      ELSE()
         SET(QT_LIBS
            libqtharfbuzz.a
            libqtpcre2.a
            libqtlibpng.a
            libQt5EventDispatcherSupport.a
            libQt5ThemeSupport.a
            libQt5FontDatabaseSupport.a
            libQt5ClipboardSupport.a
            libQt5GraphicsSupport.a
            libQt5AccessibilitySupport.a
            libQt5EdidSupport.a
            libQt5Charts.a
            )
         LIST (APPEND OS_SPECIFIC_LIBS
            ${QT5_PLUGINS_ROOT}/sqldrivers/libqsqlite.a
         )
         SET(QT_QUICK_LIBS
            ${QT5_QML_ROOT}/QtQml/Models.2/libmodelsplugin.a
            ${QT5_QML_ROOT}/QtQuick.2/libqtquick2plugin.a
            ${QT5_QML_ROOT}/QtQuick/Controls/libqtquickcontrolsplugin.a
            ${QT5_QML_ROOT}/QtQuick/Controls.2/libqtquickcontrols2plugin.a
            ${QT5_QML_ROOT}/QtQuick/Dialogs/libdialogplugin.a
            ${QT5_QML_ROOT}/QtQuick/Dialogs/Private/libdialogsprivateplugin.a
            ${QT5_QML_ROOT}/QtQuick/Layouts/libqquicklayoutsplugin.a
            ${QT5_QML_ROOT}/QtQuick/Templates.2/libqtquicktemplates2plugin.a
            ${QT5_QML_ROOT}/QtQuick/Window.2/libwindowplugin.a
            ${QT5_QML_ROOT}/Qt/labs/folderlistmodel/libqmlfolderlistmodelplugin.a
            ${QT5_QML_ROOT}/Qt/labs/settings/libqmlsettingsplugin.a
            ${QT5_LIB_ROOT}/libQt5QuickControls2.a
            ${QT5_LIB_ROOT}/libQt5QuickTemplates2.a
         )
      ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")

   ELSE(APPLE)
      FIND_PACKAGE( Threads REQUIRED )
      FIND_PACKAGE( X11 REQUIRED )
      FIND_PACKAGE( XCB REQUIRED )
      FIND_PACKAGE( Freetype REQUIRED )
      FIND_PACKAGE( Fontconfig REQUIRED )

      SET( QT_LIBS
         libqtharfbuzz.a
         libqtpcre2.a )

      SET( TMP_LIBS
         libQt5XcbQpa.a
         libQt5ServiceSupport.a
         libQt5ThemeSupport.a
         libQt5EventDispatcherSupport.a
         libQt5FontDatabaseSupport.a
         libQt5LinuxAccessibilitySupport.a
         libQt5AccessibilitySupport.a
         libQt5EdidSupport.a )

      LIST(APPEND QT_LINUX_LIBS
         ${QT5_PLUGINS_ROOT}/platforms/libqxcb.a
         ${QT5_PLUGINS_ROOT}/sqldrivers/libqsqlite.a
      )

      IF(NOT EXISTS ${QT5_LIB_ROOT}/../plugins/platforms/libqxcb.a)
         MESSAGE(FATAL_ERROR "Could not locate required Qt lib ${QT5_LIB_ROOT}/../plugins/platforms/libqxcb.a")
      ENDIF()

      FOREACH(lib_name ${TMP_LIBS})
         IF(NOT EXISTS ${QT5_LIB_ROOT}/${lib_name})
            MESSAGE(FATAL_ERROR "Could not locate required Qt lib ${QT5_LIB_ROOT}/${lib_name}")
         ENDIF()

         LIST(APPEND QT_LINUX_LIBS ${QT5_LIB_ROOT}/${lib_name})
      ENDFOREACH(lib_name)

      SET( OS_SPECIFIC_LIBS
         Qt5::QXcbIntegrationPlugin
         Qt5::QICOPlugin
         Qt5::QCupsPrinterSupportPlugin
         fontconfig
         freetype
         X11-xcb
         X11
         Xi
         SM
         ICE
         Xrender
         xcb-xkb
         xcb-render-util
         xcb-sync
         xcb-xfixes
         xcb-xinerama
         xcb-randr
         xcb-render
         xcb-image
         xcb-shm
         xcb-keysyms
         xcb-icccm
         xcb-shape
         xcb
         xkbcommon-x11
         xkbcommon
         dbus-1
         ${ZLIB_LIBRARIES}
         png
         m
         double-conversion
         dl
         cups
         ${CMAKE_THREAD_LIBS_INIT} )

      SET(QT_QUICK_LIBS
         ${QT5_QML_ROOT}/QtQml/Models.2/libmodelsplugin.a
         ${QT5_QML_ROOT}/QtQuick.2/libqtquick2plugin.a
         ${QT5_QML_ROOT}/QtQuick/Controls/libqtquickcontrolsplugin.a
         ${QT5_QML_ROOT}/QtQuick/Controls.2/libqtquickcontrols2plugin.a
         ${QT5_QML_ROOT}/QtQuick/Dialogs/libdialogplugin.a
         ${QT5_QML_ROOT}/QtQuick/Dialogs/Private/libdialogsprivateplugin.a
         ${QT5_QML_ROOT}/QtQuick/PrivateWidgets/libwidgetsplugin.a
         ${QT5_QML_ROOT}/QtQuick/Layouts/libqquicklayoutsplugin.a
         ${QT5_QML_ROOT}/QtQuick/Templates.2/libqtquicktemplates2plugin.a
         ${QT5_QML_ROOT}/QtQuick/Window.2/libwindowplugin.a
         ${QT5_QML_ROOT}/Qt/labs/folderlistmodel/libqmlfolderlistmodelplugin.a
         ${QT5_QML_ROOT}/Qt/labs/settings/libqmlsettingsplugin.a
         ${QT5_LIB_ROOT}/libQt5QuickControls2.a
         ${QT5_LIB_ROOT}/libQt5QuickTemplates2.a
      )
   ENDIF( APPLE )
ENDIF(WIN32)

FOREACH(lib_name ${QT_LIBS})
   IF(NOT EXISTS ${QT5_LIB_ROOT}/${lib_name})
      MESSAGE(FATAL_ERROR "Could not locate required Qt lib ${QT5_LIB_ROOT}/${lib_name}")
   ENDIF()

   LIST(INSERT OS_SPECIFIC_LIBS 0 ${QT5_LIB_ROOT}/${lib_name})
ENDFOREACH(lib_name)

ADD_SUBDIRECTORY( Celer )
ADD_SUBDIRECTORY( common/Blocksettle_proto )
ADD_SUBDIRECTORY( AuthAPI )

ADD_SUBDIRECTORY(BlockSettleApp)
ADD_SUBDIRECTORY(BlockSettleSigner)

ADD_SUBDIRECTORY(common/BlockSettleUILib)
ADD_SUBDIRECTORY(common/BlocksettleNetworkingLib)
ADD_SUBDIRECTORY(common/WalletsLib)
ADD_SUBDIRECTORY(common/cppForSwig)
ADD_SUBDIRECTORY(common/CommonLib)
ADD_SUBDIRECTORY(common/CommonUI)

MESSAGE("3rd party root   : ${THIRD_PARTY_COMMON_DIR}")
MESSAGE("CMAKE_BUILD_TYPE : ${CMAKE_BUILD_TYPE}")
