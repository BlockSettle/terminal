#
#
# ***********************************************************************************
# * Copyright (C) 2018 - 2021, BlockSettle AB
# * Distributed under the GNU Affero General Public License (AGPL v3)
# * See LICENSE or http://www.gnu.org/licenses/agpl.html
# *
# **********************************************************************************
#
#
CMAKE_MINIMUM_REQUIRED( VERSION 3.3 )

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_EXTENSIONS OFF)
SET(QT_USE_QTDBUS ON)

SET(PROJECT_NAME_STR BS_Terminal)
PROJECT(${PROJECT_NAME_STR})

IF (NOT CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE "Debug")
endif()

option(BSTERMINAL_SHARED_LIBS "Build shared libraries" OFF)

add_definitions(-DSTATIC_BUILD)
add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
   IF(BSTERMINAL_SHARED_LIBS)
      SET(THIRD_PARTY_BUILD_MODE "debug-shared")
   ELSE()
      SET(THIRD_PARTY_BUILD_MODE "debug")
   ENDIF()
ELSE()
   IF(BSTERMINAL_SHARED_LIBS)
      SET(THIRD_PARTY_BUILD_MODE "release-shared")
   ELSE()
      SET(THIRD_PARTY_BUILD_MODE "release")
   ENDIF()
ENDIF()

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
SET(TERMINAL_GUI_ROOT ${CMAKE_SOURCE_DIR})

set(CMAKE_MODULE_PATH "${TERMINAL_GUI_ROOT}/common/build_scripts/CMakeModules")

set(QML_IMPORT_PATH "${BLOCK_SETTLE_ROOT}/BlockSettleSigner/qml" CACHE STRING "QML import path")
set(QML2_IMPORT_PATH "${BLOCK_SETTLE_ROOT}/BlockSettleSigner/qml" CACHE STRING "QML2 import path")

# set versions info
# Terminal, PB, PuB and offline tools share same version for consistency
SET(BS_VERSION_MAJOR 0 )
SET(BS_VERSION_MINOR 91 )
SET(BS_VERSION_PATCH 2 )
SET(BS_VERSION_STRING "${BS_VERSION_MAJOR}.${BS_VERSION_MINOR}.${BS_VERSION_PATCH}")

include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)

IF( GIT_SHA1 STREQUAL GITDIR-NOTFOUND )
    IF (NOT DEFINED ENV{BS_GIT_REVISION})
	MESSAGE( FATAL_ERROR "Building from a none git repository, please define the shell veriable revision BS_GIT_REVISION=XXX")
    ELSE()
	SET(GIT_SHA1 "$ENV{BS_GIT_REVISION}")
    ENDIF()
ENDIF ()

# we only take the 8 first characters
string(SUBSTRING ${GIT_SHA1} 0 8 BS_REVISIOIN_SHA1)

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

include(GCCToolchain)
IF(NOT MSVC)
   include(LLVMToolchain)
ENDIF(MSVC)
include(LTO)
include(CompilerWarnings)
include(CompilerColorDiagnostics)
include(LinkerFlags)

IF( WIN32 )
   # output libs to same path as binaries on windows just to make life easier
   IF(BSTERMINAL_SHARED_LIBS)
      SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
      SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
   ELSE()
      SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /Zl /O2 /Ob2 /DNDEBUG")
      SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_DEBUG -DBUILD_MT_RELEASE=OFF /MTd /Zi /Zl /Ob0 /Od /RTC1")
   ENDIF()
   # Speedup compilation
   add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
   SET( LIBRARY_OUTPUT_PATH ${EXECUTABLE_OUTPUT_PATH} )
ELSE()
   SET( LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/libs )
   SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
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

SET(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${QT5_ROOT})
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${QT5_LIB_ROOT}/cmake)

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

INCLUDE_DIRECTORIES(${Qt5Core_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${Qt5Gui_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${Qt5Widgets_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${Qt5Network_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${Qt5PrintSupport_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${Qt5Qml_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${Qt5Quick_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${Qt5QuickControls2_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${Qt5Charts_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${Qt5Sql_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${Qt5Svg_INCLUDE_DIRS})

IF ( UNIX AND NOT APPLE )
   FIND_PACKAGE(Qt5DBus REQUIRED)
   ADD_DEFINITIONS( "-DBS_USE_DBUS" )
   INCLUDE_DIRECTORIES(${Qt5DBus_INCLUDE_DIRS})
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
      SET( ZMQ_LIB_NAME "libzmq-v142-mt-gd-4_3_3" )
   ELSE()
      SET( ZMQ_LIB_NAME "libzmq-v142-mt-4_3_3" )
   ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")
ELSE ()
   IF(BSTERMINAL_SHARED_LIBS)
      SET( ZMQ_LIB_NAME "libzmq.so" "zmq" )
   ELSE()
      SET( ZMQ_LIB_NAME "libzmq.a" "zmq" )
   ENDIF()
ENDIF( WIN32 )
FIND_LIBRARY(ZMQ_LIB NAMES ${ZMQ_LIB_NAME} NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)

IF( NOT ZMQ_LIB )
   IF( WIN32 )
      IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
         SET( ZMQ_LIB_NAME "libzmq-v141-mt-gd-4_3_3" )
      ELSE()
         SET( ZMQ_LIB_NAME "libzmq-v141-mt-4_3_3" )
      ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")
   ENDIF( WIN32 )
   FIND_LIBRARY(ZMQ_LIB NAMES ${ZMQ_LIB_NAME} NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
   IF( NOT ZMQ_LIB)
      MESSAGE(FATAL_ERROR "Could not find ZMQ lib")
   ENDIF( NOT ZMQ_LIB)
ENDIF( NOT ZMQ_LIB)

# OpenSSL libs
SET(OPENSSL_ROOT_DIR        ${THIRD_PARTY_COMMON_DIR}/OpenSSL)
SET(CMAKE_LIBRARY_PATH      ${CMAKE_LIBRARY_PATH} ${OPENSSL_ROOT_DIR}/lib)
set(OPENSSL_USE_STATIC_LIBS TRUE)

find_package(OpenSSL REQUIRED)

include(InstallRequiredSystemLibraries)
INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin COMPONENT Libraries)

SET(OPENSSL_LIBS
    OpenSSL::SSL
    OpenSSL::Crypto
)
INCLUDE_DIRECTORIES( ${OPENSSL_INCLUDE_DIR} )

# setup botan
SET(BOTAN_PACKAGE_ROOT ${THIRD_PARTY_COMMON_DIR}/botan)
SET(BOTAN_LIB_DIR ${BOTAN_PACKAGE_ROOT}/lib)
SET(BOTAN_INCLUDE_DIR ${BOTAN_PACKAGE_ROOT}/include/botan-2)

IF (WIN32)
   SET(BOTAN_LIB_NAME botan-2.lib botan.lib)
ELSE(WIN32)
   IF(BSTERMINAL_SHARED_LIBS)
      SET(BOTAN_LIB_NAME libbotan-2.so)
   ELSE()
      SET(BOTAN_LIB_NAME libbotan-2.a)
   ENDIF()
ENDIF(WIN32)

FIND_LIBRARY( BOTAN_LIB NAMES ${BOTAN_LIB_NAME} PATHS ${BOTAN_LIB_DIR} NO_DEFAULT_PATH )
IF( NOT BOTAN_LIB)
   MESSAGE( FATAL_ERROR "Could not find botan lib in ${BOTAN_LIB_DIR}")
ENDIF()

##setup libusb
if (UNIX)
    SET(LIBUSB_PACKAGE_ROOT ${THIRD_PARTY_COMMON_DIR}/libusb)
    SET(LIBUSB_LIB_DIR ${LIBUSB_PACKAGE_ROOT}/lib)
    SET(LIBUSB_INCLUDE_DIR ${LIBUSB_PACKAGE_ROOT}/include)

    SET(LIBUSB_LIB_NAME libusb-1.0.a)
    FIND_LIBRARY( LIBUSB_LIB NAMES ${LIBUSB_LIB_NAME} PATHS ${LIBUSB_LIB_DIR} NO_DEFAULT_PATH )
    IF( NOT LIBUSB_LIB)
       MESSAGE( FATAL_ERROR "Could not find libusb lib in ${LIBUSB_LIB_DIR}")
    ENDIF()
endif (UNIX)


# setup libwebsockets
SET(WS_PACKAGE_ROOT ${THIRD_PARTY_COMMON_DIR}/libwebsockets)
SET(WS_LIB_DIR ${WS_PACKAGE_ROOT}/lib)
SET(WS_INCLUDE_DIR ${WS_PACKAGE_ROOT}/include)

IF (WIN32)
   IF(BSTERMINAL_SHARED_LIBS)
      SET(WS_LIB_NAME websockets)
   ELSE()
      SET(WS_LIB_NAME websockets_static)
   ENDIF()
ELSE(WIN32)
   IF(BSTERMINAL_SHARED_LIBS)
      SET(WS_LIB_NAME libwebsockets.so)
   ELSE(BSTERMINAL_SHARED_LIBS)
      SET(WS_LIB_NAME libwebsockets.a)
   ENDIF()
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

# add JSON
SET(JSON_INCLUDE_DIR ${THIRD_PARTY_COMMON_DIR}/JSON/single_include)
INCLUDE_DIRECTORIES( ${JSON_INCLUDE_DIR} )

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
   IF(BSTERMINAL_SHARED_LIBS)
      SET(MPIR_LIB_NAME libgmp.so)
   ELSE()
      SET(MPIR_LIB_NAME libgmp.a)
   ENDIF()
ENDIF(WIN32)
FIND_LIBRARY( MPIR_LIB NAMES ${MPIR_LIB_NAME} PATHS ${MPIR_LIB_DIR} NO_DEFAULT_PATH )
IF( NOT MPIR_LIB)
   MESSAGE( FATAL_ERROR "Could not find MPIR lib in ${MPIR_LIB_DIR}")
ENDIF()

IF(BUILD_TESTS)
   SET(GTEST_PACKAGE_ROOT ${THIRD_PARTY_COMMON_DIR}/Gtest)
   INCLUDE_DIRECTORIES( ${GTEST_PACKAGE_ROOT}/include )
   SET(GTEST_LIB_DIR ${GTEST_PACKAGE_ROOT}/lib)
   IF (WIN32)
      IF (CMAKE_BUILD_TYPE STREQUAL "Debug")
         SET(GTEST_LIB_NAME gtestd gtest_maind)
      ELSE ("Debug")
         SET(GTEST_LIB_NAME gtest gtest_main)
      ENDIF ("Debug")
   ELSE(WIN32)
      SET(GTEST_LIB_NAME libgtest.a libgtest_main.a)
   ENDIF(WIN32)
   FIND_LIBRARY( GTEST_LIB NAMES ${GTEST_LIB_NAME} PATHS ${GTEST_LIB_DIR} NO_DEFAULT_PATH )
   IF( NOT GTEST_LIB)
      MESSAGE( FATAL_ERROR "Could not find Gtest lib in ${GTEST_LIB_DIR}")
   ENDIF()
ENDIF(BUILD_TESTS)

# setup protobuf
SET( CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${THIRD_PARTY_COMMON_DIR}/Protobuf )
FIND_PACKAGE( Protobuf REQUIRED )
INCLUDE_DIRECTORIES( ${PROTOBUF_INCLUDE_DIRS} )

IF ( UNIX )
   GET_FILENAME_COMPONENT(ProtoLibDir ${Protobuf_LIBRARY} DIRECTORY)
   FIND_LIBRARY( PROTO_LIB NAMES "libprotobuf.a" PATHS ${ProtoLibDir} NO_DEFAULT_PATH )
ELSE ( UNIX )
   SET( PROTO_LIB ${PROTOBUF_LIBRARIES} )
ENDIF ( UNIX )

# autheid helper sources
INCLUDE_DIRECTORIES( ${TERMINAL_GUI_ROOT}/AuthAPI/utils/cpp )
SET( AUTHEID_UTILS ${TERMINAL_GUI_ROOT}/AuthAPI/utils/cpp/autheid_utils.cpp )


SET( PROTO_ROOT_DIR ${TERMINAL_GUI_ROOT} )
SET( PATH_TO_GENERATED ${TERMINAL_GUI_ROOT}/generated_proto )
FILE( MAKE_DIRECTORY ${PATH_TO_GENERATED} )

SET( AUTH_PROTO_LIB_NAME               AuthAPI )
SET( BS_PROTO_LIB_NAME                 BsProtoLib )
SET( CELER_PROTO_LIB_NAME              CelerProtoLib )
SET( BLOCKSETTLE_APP_NAME              blocksettle )
SET( SIGNER_APP_NAME                   blocksettle_signer )
SET( TERMINAL_CORE_NAME                TerminalCore )
SET( TERMINAL_GUI_QT_NAME              TerminalGUI_Qt )
SET( BLOCKSETTLE_UI_LIBRARY_NAME       bsuilib )
SET( BLOCKSETTLE_HW_LIBRARY_NAME       HWIntegrations )
SET( CRYPTO_LIB_NAME                   ArmoryCryptoLib )
SET( CPP_WALLET_LIB_NAME               ArmoryWalletLib )
SET( BS_NETWORK_LIB_NAME               BSNetwork )
SET( COMMON_LIB_NAME                   CommonLib )
SET( COMMON_UI_LIB_NAME                CommonUI )

SET( BLOCKSETTLE_UI_INCLUDE_DIR ${TERMINAL_GUI_ROOT}/BlockSettleUILib)
SET( CRYPTO_LIB_DIR ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig )
SET( CRYPTO_LIB_INCLUDE_DIR ${CRYPTO_LIB_DIR} )
SET( WALLET_LIB_INCLUDE_DIR ${TERMINAL_GUI_ROOT}/common/WalletsLib )
SET( BS_NETWORK_INCLUDE_DIR ${TERMINAL_GUI_ROOT}/common/BlocksettleNetworkingLib )
SET( COMMON_UI_LIB_INCLUDE_DIR ${TERMINAL_GUI_ROOT}/CommonUI )
SET( COMMON_LIB_INCLUDE_DIR ${TERMINAL_GUI_ROOT}/common/CommonLib )
SET( BS_HW_LIB_INCLUDE_DIR ${TERMINAL_GUI_ROOT}/BlockSettleHW )

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
      shlwapi
      userenv
      version
      UxTheme
   )

   IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
      SET(QT_LIBS
         ${QT5_LIB_ROOT}/qtlibpngd.lib
         ${QT5_LIB_ROOT}/Qt5FontDatabaseSupportd.lib
         ${QT5_LIB_ROOT}/Qt5EventDispatcherSupportd.lib
         ${QT5_LIB_ROOT}/Qt5ThemeSupportd.lib
         ${QT5_LIB_ROOT}/Qt5WindowsUIAutomationSupportd.lib
         ${QT5_LIB_ROOT}/Qt5AccessibilitySupportd.lib
         ${QT5_LIB_ROOT}/Qt5Chartsd.lib
      )

      SET(QT_QUICK_LIBS
         ${QT5_LIB_ROOT}/Qt5Quickd.lib
         ${QT5_LIB_ROOT}/Qt5QuickWidgetsd.lib
         ${QT5_LIB_ROOT}/Qt5QuickControls2d.lib
         ${QT5_LIB_ROOT}/Qt5QuickTemplates2d.lib
      )

      IF(NOT BSTERMINAL_SHARED_LIBS)
      # Don't need to include - these libs included implicitly
      # Todo: consider to use libucrt to replace libcmt
#         LIST(APPEND OS_SPECIFIC_LIBS
#            libcmtd.lib
#            libcpmtd.lib
#         )

         LIST(APPEND QT_LIBS
            ${QT5_LIB_ROOT}/qtharfbuzzd.lib
            ${QT5_LIB_ROOT}/qtpcre2d.lib
            ${QT5_PLUGINS_ROOT}/sqldrivers/qsqlited.lib
         )

         LIST(APPEND QT_QUICK_LIBS
            ${QT5_QML_ROOT}/QtQml/Models.2/modelsplugind.lib
            ${QT5_QML_ROOT}/QtQuick.2/qtquick2plugind.lib
            ${QT5_QML_ROOT}/QtQuick/Controls/qtquickcontrolsplugind.lib
            ${QT5_QML_ROOT}/QtQuick/Controls.2/qtquickcontrols2plugind.lib
            ${QT5_QML_ROOT}/QtQuick/Layouts/qquicklayoutsplugind.lib
            ${QT5_QML_ROOT}/QtQuick/Templates.2/qtquicktemplates2plugind.lib
            ${QT5_QML_ROOT}/QtQuick/Window.2/windowplugind.lib
            ${QT5_QML_ROOT}/Qt/labs/folderlistmodel/qmlfolderlistmodelplugind.lib
            ${QT5_QML_ROOT}/Qt/labs/settings/qmlsettingsplugind.lib
            ${QT5_QML_ROOT}/Qt/labs/platform/qtlabsplatformplugind.lib
         )
      ELSE(NOT BSTERMINAL_SHARED_LIBS)
         LIST(APPEND OS_SPECIFIC_LIBS
            msvcrtd.lib
            msvcprtd.lib
         )
      ENDIF(NOT BSTERMINAL_SHARED_LIBS)

   ELSE(CMAKE_BUILD_TYPE STREQUAL "Debug")
      SET(QT_LIBS
         ${QT5_LIB_ROOT}/qtlibpng.lib
         ${QT5_LIB_ROOT}/Qt5FontDatabaseSupport.lib
         ${QT5_LIB_ROOT}/Qt5EventDispatcherSupport.lib
         ${QT5_LIB_ROOT}/Qt5ThemeSupport.lib
         ${QT5_LIB_ROOT}/Qt5WindowsUIAutomationSupport.lib
         ${QT5_LIB_ROOT}/Qt5AccessibilitySupport.lib
         ${QT5_LIB_ROOT}/Qt5Charts.lib
      )

      SET(QT_QUICK_LIBS
         ${QT5_LIB_ROOT}/Qt5Quick.lib
         ${QT5_LIB_ROOT}/Qt5QuickWidgets.lib
         ${QT5_LIB_ROOT}/Qt5QuickControls2.lib
         ${QT5_LIB_ROOT}/Qt5QuickTemplates2.lib
      )

      IF(NOT BSTERMINAL_SHARED_LIBS)
         LIST(APPEND QT_LIBS
            ${QT5_LIB_ROOT}/qtharfbuzz.lib
            ${QT5_LIB_ROOT}/qtpcre2.lib
            ${QT5_PLUGINS_ROOT}/sqldrivers/qsqlite.lib
         )
      # Don't need to include - these libs included implicitly
      # Todo: consider to use libucrt to replace libcmt
#         LIST(APPEND OS_SPECIFIC_LIBS
#            libcmt.lib
#            libcpmt.lib
#         )

         LIST(APPEND QT_QUICK_LIBS
            ${QT5_QML_ROOT}/QtQml/Models.2/modelsplugin.lib
            ${QT5_QML_ROOT}/QtQuick.2/qtquick2plugin.lib
            ${QT5_QML_ROOT}/QtQuick/Controls/qtquickcontrolsplugin.lib
            ${QT5_QML_ROOT}/QtQuick/Controls.2/qtquickcontrols2plugin.lib
            ${QT5_QML_ROOT}/QtQuick/Layouts/qquicklayoutsplugin.lib
            ${QT5_QML_ROOT}/QtQuick/Templates.2/qtquicktemplates2plugin.lib
            ${QT5_QML_ROOT}/QtQuick/Window.2/windowplugin.lib
            ${QT5_QML_ROOT}/Qt/labs/folderlistmodel/qmlfolderlistmodelplugin.lib
            ${QT5_QML_ROOT}/Qt/labs/settings/qmlsettingsplugin.lib
            ${QT5_QML_ROOT}/Qt/labs/platform/qtlabsplatformplugin.lib
         )
      ELSE(NOT BSTERMINAL_SHARED_LIBS)
         LIST(APPEND OS_SPECIFIC_LIBS
            msvcrt.lib
            msvcprt.lib
         )
      ENDIF(NOT BSTERMINAL_SHARED_LIBS)

   ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")

   IF(NOT BSTERMINAL_SHARED_LIBS)
      LIST(APPEND QT_LIBS
         Qt5::QWindowsIntegrationPlugin
         Qt5::QICOPlugin
         Qt5::QWindowsPrinterSupportPlugin
      )
      add_definitions(-DUSE_QWindowsIntegrationPlugin)
   ENDIF()
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
         "-framework IOSurface"
         ${ZLIB_LIBRARIES}
         ${CUPS_LIBRARIES})

      add_definitions(-DUSE_QCocoaIntegrationPlugin)

      IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
         SET(QT_LIBS
            ${QT5_LIB_ROOT}/libqtharfbuzz_debug.a
            ${QT5_LIB_ROOT}/libqtpcre2_debug.a
            ${QT5_LIB_ROOT}/libqtlibpng_debug.a
            ${QT5_LIB_ROOT}/libQt5EventDispatcherSupport_debug.a
            ${QT5_LIB_ROOT}/libQt5ThemeSupport_debug.a
            ${QT5_LIB_ROOT}/libQt5FontDatabaseSupport_debug.a
            ${QT5_LIB_ROOT}/libQt5ClipboardSupport_debug.a
            ${QT5_LIB_ROOT}/libQt5GraphicsSupport_debug.a
            ${QT5_LIB_ROOT}/libQt5AccessibilitySupport_debug.a
            ${QT5_LIB_ROOT}/libQt5EdidSupport_debug.a
            ${QT5_LIB_ROOT}/libQt5Charts_debug.a
            Qt5::QCocoaIntegrationPlugin
            Qt5::PrintSupport
            Qt5::QICOPlugin
            Qt5::QCocoaPrinterSupportPlugin
            )
         LIST (APPEND OS_SPECIFIC_LIBS
            ${QT5_PLUGINS_ROOT}/sqldrivers/libqsqlite_debug.a
         )
         SET(QT_QUICK_LIBS
            ${QT5_QML_ROOT}/QtQml/Models.2/libmodelsplugin_debug.a
            ${QT5_QML_ROOT}/QtQuick.2/libqtquick2plugin_debug.a
            ${QT5_QML_ROOT}/QtQuick/Controls/libqtquickcontrolsplugin_debug.a
            ${QT5_QML_ROOT}/QtQuick/Controls.2/libqtquickcontrols2plugin_debug.a
            ${QT5_QML_ROOT}/QtQuick/Layouts/libqquicklayoutsplugin_debug.a
            ${QT5_QML_ROOT}/QtQuick/Templates.2/libqtquicktemplates2plugin_debug.a
            ${QT5_QML_ROOT}/QtQuick/Window.2/libwindowplugin_debug.a
            ${QT5_QML_ROOT}/Qt/labs/folderlistmodel/libqmlfolderlistmodelplugin_debug.a
            ${QT5_QML_ROOT}/Qt/labs/settings/libqmlsettingsplugin_debug.a
            ${QT5_QML_ROOT}/Qt/labs/platform/libqtlabsplatformplugin_debug.a
            ${QT5_LIB_ROOT}/libQt5Quick_debug.a
            ${QT5_LIB_ROOT}/libQt5QuickWidgets_debug.a
            ${QT5_LIB_ROOT}/libQt5QuickControls2_debug.a
            ${QT5_LIB_ROOT}/libQt5QuickTemplates2_debug.a
         )
      ELSE()
         SET(QT_LIBS
            ${QT5_LIB_ROOT}/libqtharfbuzz.a
            ${QT5_LIB_ROOT}/libqtpcre2.a
            ${QT5_LIB_ROOT}/libqtlibpng.a
            ${QT5_LIB_ROOT}/libQt5EventDispatcherSupport.a
            ${QT5_LIB_ROOT}/libQt5ThemeSupport.a
            ${QT5_LIB_ROOT}/libQt5FontDatabaseSupport.a
            ${QT5_LIB_ROOT}/libQt5ClipboardSupport.a
            ${QT5_LIB_ROOT}/libQt5GraphicsSupport.a
            ${QT5_LIB_ROOT}/libQt5AccessibilitySupport.a
            ${QT5_LIB_ROOT}/libQt5EdidSupport.a
            ${QT5_LIB_ROOT}/libQt5Charts.a
            Qt5::QCocoaIntegrationPlugin
            Qt5::PrintSupport
            Qt5::QICOPlugin
            Qt5::QCocoaPrinterSupportPlugin
            )
         LIST (APPEND QT_LIBS
            ${QT5_PLUGINS_ROOT}/sqldrivers/libqsqlite.a
         )
         SET(QT_QUICK_LIBS
            ${QT5_QML_ROOT}/QtQml/Models.2/libmodelsplugin.a
            ${QT5_QML_ROOT}/QtQuick.2/libqtquick2plugin.a
            ${QT5_QML_ROOT}/QtQuick/Controls/libqtquickcontrolsplugin.a
            ${QT5_QML_ROOT}/QtQuick/Controls.2/libqtquickcontrols2plugin.a
            ${QT5_QML_ROOT}/QtQuick/Layouts/libqquicklayoutsplugin.a
            ${QT5_QML_ROOT}/QtQuick/Templates.2/libqtquicktemplates2plugin.a
            ${QT5_QML_ROOT}/QtQuick/Window.2/libwindowplugin.a
            ${QT5_QML_ROOT}/Qt/labs/folderlistmodel/libqmlfolderlistmodelplugin.a
            ${QT5_QML_ROOT}/Qt/labs/settings/libqmlsettingsplugin.a
            ${QT5_QML_ROOT}/Qt/labs/platform/libqtlabsplatformplugin.a
            ${QT5_LIB_ROOT}/libQt5QuickControls2.a
            ${QT5_LIB_ROOT}/libQt5QuickTemplates2.a
         )
      ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")

   ELSE(APPLE)
      FIND_PACKAGE( Threads REQUIRED )
      FIND_PACKAGE( X11 REQUIRED )
      FIND_PACKAGE( X11_XCB REQUIRED )
      FIND_PACKAGE( XCB REQUIRED
         COMPONENTS XKB RENDERUTIL SYNC XFIXES XINERAMA RANDR RENDER IMAGE SHM KEYSYMS ICCCM SHAPE
         OPTIONAL_COMPONENTS XINPUT )
      FIND_PACKAGE( Freetype REQUIRED )
      FIND_PACKAGE( Fontconfig REQUIRED )

      IF(BSTERMINAL_SHARED_LIBS)

      ELSE(BSTERMINAL_SHARED_LIBS)
         SET(QT_LIBS
            ${QT5_LIB_ROOT}/libqtharfbuzz.a
            ${QT5_LIB_ROOT}/libqtpcre2.a
         )

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

         LIST(APPEND QT_LINUX_LIBS
            Qt5::QXcbIntegrationPlugin
            Qt5::QICOPlugin
            Qt5::QCupsPrinterSupportPlugin
         )

         SET( OS_SPECIFIC_LIBS
            Fontconfig::Fontconfig
            freetype
            X11::XCB
            X11
            Xi
            SM
            ICE
            Xrender
            ${XCB_LIBRARIES}
            xkbcommon-x11
            xkbcommon
            dbus-1
            ${ZLIB_LIBRARIES}
            png
            m
            double-conversion.a
            dl
            cups
            udev
            ${CMAKE_THREAD_LIBS_INIT} )

         add_definitions(-DUSE_QXcbIntegrationPlugin)

         SET(QT_QUICK_LIBS
            ${QT5_QML_ROOT}/QtQml/Models.2/libmodelsplugin.a
            ${QT5_QML_ROOT}/QtQuick.2/libqtquick2plugin.a
            ${QT5_QML_ROOT}/QtQuick/Controls/libqtquickcontrolsplugin.a
            ${QT5_QML_ROOT}/QtQuick/Controls.2/libqtquickcontrols2plugin.a
            ${QT5_QML_ROOT}/QtQuick/PrivateWidgets/libwidgetsplugin.a
            ${QT5_QML_ROOT}/QtQuick/Layouts/libqquicklayoutsplugin.a
            ${QT5_QML_ROOT}/QtQuick/Templates.2/libqtquicktemplates2plugin.a
            ${QT5_QML_ROOT}/QtQuick/Window.2/libwindowplugin.a
            ${QT5_QML_ROOT}/Qt/labs/folderlistmodel/libqmlfolderlistmodelplugin.a
            ${QT5_QML_ROOT}/Qt/labs/settings/libqmlsettingsplugin.a
            ${QT5_QML_ROOT}/Qt/labs/platform/libqtlabsplatformplugin.a
            ${QT5_LIB_ROOT}/libQt5QuickControls2.a
            ${QT5_LIB_ROOT}/libQt5QuickTemplates2.a
         )
      ENDIF(BSTERMINAL_SHARED_LIBS)
   ENDIF( APPLE )
ENDIF(WIN32)

LIST(REMOVE_DUPLICATES QT_LIBS)


IF(WIN32)
file(COPY ${BLOCK_SETTLE_ROOT}/Scripts/RFQBot.qml
     DESTINATION ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_BUILD_TYPE}/)
ENDIF(WIN32)


IF(BSTERMINAL_SHARED_LIBS)
   unset(OPENSSL_USE_STATIC_LIBS)
   remove_definitions(-DSTATIC_BUILD)

   IF(WIN32)
      # Add new project only for copying libraries
      set(COPY_SHARED_LIBS_NAME CopySharedLibs)
      add_custom_target(${COPY_SHARED_LIBS_NAME} ALL)

      # Copy dll's
      # Qt
      get_target_property(QT5_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
      get_filename_component(QT5_WINDEPLOYQT_EXECUTABLE ${QT5_QMAKE_EXECUTABLE} PATH)
      set(QT5_WINDEPLOYQT_EXECUTABLE "${QT5_WINDEPLOYQT_EXECUTABLE}/windeployqt.exe")
      add_custom_command(TARGET ${COPY_SHARED_LIBS_NAME} POST_BUILD
         COMMAND ${QT5_WINDEPLOYQT_EXECUTABLE} --no-opengl-sw --compiler-runtime --no-angle --qmldir ${CMAKE_SOURCE_DIR} ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})
      add_dependencies(${COPY_SHARED_LIBS_NAME} ${SIGNER_APP_NAME} ${BLOCKSETTLE_APP_NAME})

      # libzmq
      STRING(REPLACE ".lib" ".dll" ZMQ_LIB_DLL ${ZMQ_LIB})
      add_custom_command(TARGET ${COPY_SHARED_LIBS_NAME} POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy_if_different $<$<CONFIG:Debug>:${ZMQ_LIB_DLL}> $<$<NOT:$<CONFIG:Debug>>:${ZMQ_LIB_DLL}> ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})

      # botan
      STRING(REPLACE ".lib" ".dll" BOTAN_LIB_DLL ${BOTAN_LIB})
      add_custom_command(TARGET ${COPY_SHARED_LIBS_NAME} POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy_if_different $<$<CONFIG:Debug>:${BOTAN_LIB_DLL}> $<$<NOT:$<CONFIG:Debug>>:${BOTAN_LIB_DLL}> ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})

      # qrencode
      STRING(REPLACE ".lib" ".dll" QRENCODE_LIB_DLL ${QRENCODE_LIB})
      add_custom_command(TARGET ${COPY_SHARED_LIBS_NAME} POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy_if_different $<$<CONFIG:Debug>:${QRENCODE_LIB_DLL}> $<$<NOT:$<CONFIG:Debug>>:${QRENCODE_LIB_DLL}> ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})

      # websockets
      STRING(REPLACE ".lib" ".dll" WS_LIB_DLL ${WS_LIB})
      add_custom_command(TARGET ${COPY_SHARED_LIBS_NAME} POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy_if_different $<$<CONFIG:Debug>:${WS_LIB_DLL}> $<$<NOT:$<CONFIG:Debug>>:${WS_LIB_DLL}> ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})

      string(REGEX REPLACE "([0-9])\.([0-9])\.([0-9]\.*)" "\\1;\\2" VERSION_MATCHES ${OPENSSL_VERSION})
      list(GET VERSION_MATCHES 0 version_major)
      list(GET VERSION_MATCHES 1 version_minor)
      string(REPLACE ".lib" "" CRYPTO_LIB_DLL ${OPENSSL_CRYPTO_LIBRARY})
      string(REPLACE "OpenSSL/lib/" "OpenSSL/bin/" CRYPTO_LIB_DLL ${CRYPTO_LIB_DLL})
      string(APPEND CRYPTO_LIB_DLL "-${version_major}_${version_minor}-x64.dll" )
      string(REPLACE ".lib" "" SSL_LIB_DLL ${OPENSSL_SSL_LIBRARY})
      string(REPLACE "OpenSSL/lib/" "OpenSSL/bin/" SSL_LIB_DLL ${SSL_LIB_DLL})
      string(APPEND SSL_LIB_DLL "-${version_major}_${version_minor}-x64.dll" )
      add_custom_command(TARGET ${COPY_SHARED_LIBS_NAME} POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy_if_different $<$<CONFIG:Debug>:${CRYPTO_LIB_DLL}> $<$<NOT:$<CONFIG:Debug>>:${CRYPTO_LIB_DLL}> ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})
      add_custom_command(TARGET ${COPY_SHARED_LIBS_NAME} POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy_if_different $<$<CONFIG:Debug>:${SSL_LIB_DLL}> $<$<NOT:$<CONFIG:Debug>>:${SSL_LIB_DLL}> ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})
   ENDIF()

   IF(UNIX AND NOT APPLE)
      SET( OS_SPECIFIC_LIBS udev )
   ENDIF()

   IF(APPLE AND NOT UNIX)
   ENDIF()

ELSE() # Static version
   IF(WIN32)
      set(COPY_SHARED_LIBS_NAME CopySharedLibs)
      add_custom_target(${COPY_SHARED_LIBS_NAME} ALL)

      # libzmq
      STRING(REPLACE ".lib" ".dll" ZMQ_LIB_DLL ${ZMQ_LIB})
      add_custom_command(TARGET ${COPY_SHARED_LIBS_NAME} POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy_if_different $<$<CONFIG:Debug>:${ZMQ_LIB_DLL}> $<$<NOT:$<CONFIG:Debug>>:${ZMQ_LIB_DLL}> ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})
   ENDIF()
ENDIF()


ADD_SUBDIRECTORY( common/Blocksettle_proto )
ADD_SUBDIRECTORY( Celer )
ADD_SUBDIRECTORY( AuthAPI )

ADD_SUBDIRECTORY(Core)
ADD_SUBDIRECTORY(GUI)
ADD_SUBDIRECTORY(BlockSettleUILib)
ADD_SUBDIRECTORY(common/BlocksettleNetworkingLib)
ADD_SUBDIRECTORY(common/WalletsLib)
ADD_SUBDIRECTORY(common/cppForSwig)
ADD_SUBDIRECTORY(common/CommonLib)
ADD_SUBDIRECTORY(CommonUI)
ADD_SUBDIRECTORY(BlockSettleHW)

ADD_SUBDIRECTORY(BlockSettleApp)
ADD_SUBDIRECTORY(BlockSettleSigner)

IF(BUILD_TESTS)
   ADD_SUBDIRECTORY(UnitTests)
ENDIF(BUILD_TESTS)

IF(BUILD_TRACKER)
   ADD_SUBDIRECTORY(BlockSettleTracker)
ENDIF(BUILD_TRACKER)

MESSAGE("3rd party root   : ${THIRD_PARTY_COMMON_DIR}")
MESSAGE("CMAKE_BUILD_TYPE : ${CMAKE_BUILD_TYPE}")
