CMAKE_MINIMUM_REQUIRED( VERSION 3.3 )

SET (CMAKE_CXX_STANDARD 11)

SET( PROJECT_NAME_STR BlockSettle )
PROJECT( ${PROJECT_NAME_STR} )

IF (NOT CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE "Debug")
endif()

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
   SET(THIRD_PARTY_BUILD_MODE "debug")
ELSE(CMAKE_BUILD_TYPE STREQUAL "Debug")
   SET(THIRD_PARTY_BUILD_MODE "release")
ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")

# setup directories
SET(BLOCK_SETTLE_ROOT ${CMAKE_SOURCE_DIR})

set(CMAKE_MODULE_PATH "${BLOCK_SETTLE_ROOT}/common/build_scripts/CMakeModules")

set(QML_IMPORT_PATH "${BLOCK_SETTLE_ROOT}/BlockSettleSigner/qml" CACHE STRING "QML import path")
set(QML2_IMPORT_PATH "${BLOCK_SETTLE_ROOT}/BlockSettleSigner/qml" CACHE STRING "QML2 import path")

# set versions info
# Terminal, PB, PuB and offline tools share same version for consistency
SET(BS_VERSION_MAJOR 0 )
SET(BS_VERSION_MINOR 8 )
SET(BS_VERSION_PATCH 0 )
SET(BS_VERSION_STRING "${BS_VERSION_MAJOR}.${BS_VERSION_MINOR}.${BS_VERSION_PATCH}")

include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
STRING(REPLACE "refs/heads/" "" GIT_BRANCH ${GIT_REFSPEC})

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
      SET( ZMQ_LIB_NAME "libzmq-v140-mt-gd-4_2_5" )
   ELSE()
      SET( ZMQ_LIB_NAME "libzmq-v140-mt-4_2_5" )
   ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")
ELSE ()
   SET( ZMQ_LIB_NAME "libzmq.a" "zmq" )
ENDIF( WIN32 )
FIND_LIBRARY(ZMQ_LIB NAMES ${ZMQ_LIB_NAME} REQUIRED NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)

IF( NOT ZMQ_LIB )
   IF( WIN32 )
      IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
         SET( ZMQ_LIB_NAME "libzmq-v141-mt-gd-4_2_5" )
      ELSE()
         SET( ZMQ_LIB_NAME "libzmq-v141-mt-4_2_5" )
      ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")
   ENDIF( WIN32 )
   FIND_LIBRARY(ZMQ_LIB NAMES ${ZMQ_LIB_NAME} REQUIRED NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
   IF( NOT ZMQ_LIB)
      MESSAGE(FATAL_ERROR "Could not find ZMQ lib")
   ENDIF( NOT ZMQ_LIB)
ENDIF( NOT ZMQ_LIB)

IF( WIN32 )
   FILE(COPY ${ZEROMQ_ROOT}/lib/${ZMQ_LIB_NAME}.dll DESTINATION ${LIBRARY_OUTPUT_PATH}/Debug)
   FILE(COPY ${ZEROMQ_ROOT}/lib/${ZMQ_LIB_NAME}.dll DESTINATION ${LIBRARY_OUTPUT_PATH}/Release)
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

# setup cryptopp
SET(CRYPTOPP_PACKAGE_ROOT ${THIRD_PARTY_COMMON_DIR}/cryptopp)
SET(CRYPTOPP_LIB_DIR ${CRYPTOPP_PACKAGE_ROOT}/lib)
SET(CRYPTOPP_INCLUDE_DIR ${CRYPTOPP_PACKAGE_ROOT}/include)

IF (WIN32)
   SET(CRYPTOPP_LIB_NAME cryptopp-static)
ELSE(WIN32)
   SET(CRYPTOPP_LIB_NAME libcryptopp.a)
ENDIF(WIN32)

FIND_LIBRARY( CRYPTOPP_LIB NAMES ${CRYPTOPP_LIB_NAME} PATHS ${CRYPTOPP_LIB_DIR} NO_DEFAULT_PATH )
IF( NOT CRYPTOPP_LIB)
   MESSAGE( FATAL_ERROR "Could not find cryptopp lib in ${CRYPTOPP_LIB_DIR}")
ENDIF()
INCLUDE_DIRECTORIES( ${CRYPTOPP_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${CRYPTOPP_INCLUDE_DIR}/cryptopp )


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
INCLUDE_DIRECTORIES( ${LIBBTC_INCLUDE_DIR}/include )

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


SET( PATH_TO_GENERATED ${BLOCK_SETTLE_ROOT}/generated_proto/ )
FILE( MAKE_DIRECTORY ${PATH_TO_GENERATED} )

# setup Blocksettle communication proto files
SET( BS_COMMUNICATION_GENERATED_PATH ${PATH_TO_GENERATED}/BS/Communication)
FILE( MAKE_DIRECTORY ${BS_COMMUNICATION_GENERATED_PATH} )

SET(BS_COMMUNICATION_PROTO_FILES_DIR ${BLOCK_SETTLE_ROOT}/common/Blocksettle_proto/Communication)
FILE(GLOB BS_COMMUNICATION_PROTO_FILES ${BS_COMMUNICATION_PROTO_FILES_DIR}/*.proto)
EXECUTE_PROCESS( COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ${BS_COMMUNICATION_PROTO_FILES} --cpp_out=${BS_COMMUNICATION_GENERATED_PATH} --proto_path=${BS_COMMUNICATION_PROTO_FILES_DIR})

SET( BS_COMMUNICATION_INCLUDE_DIR ${BS_COMMUNICATION_GENERATED_PATH} )
FILE( GLOB BS_COMMUNICATION_PROTO_SOURCES ${BS_COMMUNICATION_GENERATED_PATH}/*.cc )
FILE( GLOB BS_COMMUNICATION_PROTO_HEADERS ${BS_COMMUNICATION_GENERATED_PATH}/*.h )

# setup Blocksettle storage proto files
SET( BS_STORAGE_GENERATED_PATH ${PATH_TO_GENERATED}/BS/Storage)
FILE( MAKE_DIRECTORY ${BS_STORAGE_GENERATED_PATH} )

SET(BS_STORAGE_PROTO_FILES_DIR ${BLOCK_SETTLE_ROOT}/common/Blocksettle_proto/Storage)
FILE(GLOB BS_STORAGE_PROTO_FILES ${BS_STORAGE_PROTO_FILES_DIR}/*.proto)
EXECUTE_PROCESS( COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ${BS_STORAGE_PROTO_FILES} --cpp_out=${BS_STORAGE_GENERATED_PATH} --proto_path=${BS_STORAGE_PROTO_FILES_DIR})

SET( BS_STORAGE_INCLUDE_DIR ${BS_STORAGE_GENERATED_PATH} )
FILE( GLOB BS_STORAGE_PROTO_SOURCES ${BS_STORAGE_GENERATED_PATH}/*.cc )
FILE( GLOB BS_STORAGE_PROTO_HEADERS ${BS_STORAGE_GENERATED_PATH}/*.h )

# setup CELER proto files
SET(PROTO_BASE_PATH ${BLOCK_SETTLE_ROOT}/Celer/)

FUNCTION( ADD_PROTO_DIR NAME PATH )
   SET( PROTO_FILES_PATH ${PROTO_BASE_PATH}/${PATH} )
   FILE( GLOB PROTO_FILES_LIST ${PROTO_FILES_PATH}/*.proto )

   EXECUTE_PROCESS( COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ${PROTO_FILES_LIST} --cpp_out=${PATH_TO_GENERATED} --proto_path=${PROTO_BASE_PATH})

   FILE( GLOB GENERATED_PROTO_SOURCES ${PATH_TO_GENERATED}/${PATH}/*.cc)
   FILE( GLOB GENERATED_PROTO_HEADERS ${PATH_TO_GENERATED}/${PATH}/*.h)

   SET( ${NAME}_INCLUDE_DIR ${PATH_TO_GENERATED}/${PATH} PARENT_SCOPE)
   SET( ${NAME}_PROTO_SOURCES ${GENERATED_PROTO_SOURCES} PARENT_SCOPE )
   SET( ${NAME}_PROTO_HEADERS ${GENERATED_PROTO_HEADERS} PARENT_SCOPE )

ENDFUNCTION(ADD_PROTO_DIR)

ADD_PROTO_DIR(LOGIN                         com/celertech/baseserver/communication/login)
ADD_PROTO_DIR(NETTY                         com/celertech/baseserver/communication/protobuf)
ADD_PROTO_DIR(ENUMS                         com/celertech/baseserver/api/enums)
ADD_PROTO_DIR(API_USER                      com/celertech/baseserver/api/user)
ADD_PROTO_DIR(API_SESSION                   com/celertech/baseserver/api/session)
ADD_PROTO_DIR(API_SOCKET                    com/celertech/baseserver/api/socket)
ADD_PROTO_DIR(CELER_BASESERVER_EXCEPTION    com/celertech/baseserver/api/exception)

ADD_PROTO_DIR(MARKET_ENUMS                  com/celertech/marketmerchant/api/enums)
ADD_PROTO_DIR(MARKET_SECURITY_DEFINITION    com/celertech/marketmerchant/api/securitydefinition)
ADD_PROTO_DIR(MARKET_DATA                   com/celertech/marketmerchant/api/marketdata)
ADD_PROTO_DIR(MARKET_STATS                  com/celertech/marketmerchant/api/marketstatistic)
ADD_PROTO_DIR(STATIC_USER_PROPERTY          com/celertech/staticdata/api/user/property)

ADD_PROTO_DIR(BS_COMMON_ENUMS               com/blocksettle/enums)
ADD_PROTO_DIR(BS_TERMINAL_API               com/blocksettle/terminal)

ADD_PROTO_DIR(QUOTE                         com/celertech/marketmerchant/api/quote)
ADD_PROTO_DIR(ORDER                         com/celertech/marketmerchant/api/order)
ADD_PROTO_DIR(SIGN                          com/celertech/marketmerchant/api/order/bitcoin)
ADD_PROTO_DIR(CELER_API_ACCOUNTS            com/celertech/staticdata/api/account)
ADD_PROTO_DIR(CELER_API_USER_ACCOUNT        com/celertech/staticdata/api/user/account)
ADD_PROTO_DIR(CELER_PIGGYBANK_API_ENUMS     com/celertech/piggybank/api/enums)
ADD_PROTO_DIR(CELER_PIGGYBANK_API_SUBLEDGER com/celertech/piggybank/api/subledger)

SET(ARMORY_PROTO_FILES_DIR ${BLOCK_SETTLE_ROOT}/common/cppForSwig/protobuf)
FILE(GLOB ARMORY_PROTO_FILES ${ARMORY_PROTO_FILES_DIR}/*.proto)
EXECUTE_PROCESS(COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ${ARMORY_PROTO_FILES} --cpp_out=${ARMORY_PROTO_FILES_DIR} --proto_path=${ARMORY_PROTO_FILES_DIR})


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

SET(CRYPTOPP_EXTRA_DIR ${CRYPTO_LIB_DIR}/cryptopp)
SET(MDB_DIR ${CRYPTO_LIB_DIR}/lmdb)

INCLUDE_DIRECTORIES( ${CRYPTOPP_EXTRA_DIR} )
INCLUDE_DIRECTORIES( ${MDB_DIR} )

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
      Qt5::QICOPlugin)

   IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
      SET(QT_LIBS
         qtharfbuzzd.lib
         qtpcre2d.lib
         qtlibpngd.lib
         Qt5FontDatabaseSupportd.lib
         Qt5EventDispatcherSupportd.lib
         Qt5ThemeSupportd.lib
         Qt5WindowsUIAutomationSupportd.lib
         Qt5AccessibilitySupportd.lib )
      LIST(APPEND OS_SPECIFIC_LIBS msvcrtd)
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
         Qt5AccessibilitySupport.lib )
      LIST(APPEND OS_SPECIFIC_LIBS msvcrt)
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
         Qt5::QCocoaIntegrationPlugin
         Qt5::PrintSupport
         Qt5::QICOPlugin
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

      SET(QT_LIBS
         libqtharfbuzz.a
         libqtpcre2.a
         libQt5EventDispatcherSupport.a
         libQt5ThemeSupport.a
         libQt5ServiceSupport.a
         libQt5XcbQpa.a
         libQt5EdidSupport.a
         )

      SET(OS_SPECIFIC_LIBS
         dl
         ${CMAKE_THREAD_LIBS_INIT}
         ${ZLIB_LIBRARIES}
         ${XCB_LIBRARIES}
         ${QT5_LIB_ROOT}/libQt5FontDatabaseSupport.a
         ${FREETYPE_LIBRARIES}
         ${FONTCONFIG_LIBRARIES}
         X11-xcb
         Xrender
         png
         Qt5::QXcbIntegrationPlugin
         Qt5::QICOPlugin
         ${X11_LIBRARIES}
         ${X11_Xi_LIB}
         )

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

   LIST(APPEND OS_SPECIFIC_LIBS ${QT5_LIB_ROOT}/${lib_name})
ENDFOREACH(lib_name)

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
