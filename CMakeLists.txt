CMAKE_MINIMUM_REQUIRED( VERSION 3.7 )

SET (CMAKE_CXX_STANDARD 14)
SET (CMAKE_CXX_EXTENSIONS OFF)

SET( PROJECT_NAME_STR BS_Tests )
PROJECT( ${PROJECT_NAME_STR} )

IF (NOT CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE "Debug")
endif()

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
   SET(THIRD_PARTY_BUILD_MODE "debug")
ELSE(CMAKE_BUILD_TYPE STREQUAL "Debug")
   SET(THIRD_PARTY_BUILD_MODE "release")
ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")

add_definitions(-DNOMINMAX)
add_definitions(-DCOINBASE_MATURITY_TESTS)

# Force Armory to ignore Crypto++ and use libbtc instead.
add_definitions(-DLIBBTC_ONLY)

# setup directories
SET(BLOCK_SETTLE_ROOT ${CMAKE_SOURCE_DIR})
SET(AUTH_EID_ROOT ${CMAKE_SOURCE_DIR})

set(CMAKE_MODULE_PATH "${BLOCK_SETTLE_ROOT}/common/build_scripts/CMakeModules")
set(BUILD_TEST_TOOLS 1)

# set versions info
# Terminal, PB, PuB and offline tools share same version for consistency
SET(BS_VERSION_MAJOR 0 )
SET(BS_VERSION_MINOR 9 )
SET(BS_VERSION_PATCH 1 )
SET(BS_VERSION_STRING "${BS_VERSION_MAJOR}.${BS_VERSION_MINOR}.${BS_VERSION_PATCH}")

include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)

STRING(FIND ${GIT_REFSPEC} "refs/heads/" BRANCH_POSITION)

IF( GIT_SHA1 STREQUAL GITDIR-NOTFOUND )
  MESSAGE( FATAL_ERROR "This is not a Git repository, but it should be.")
ELSE ( GIT_SHA1 STREQUAL GITDIR-NOTFOUND )
   # we only take the 8 first characters
  STRING(SUBSTRING ${GIT_SHA1} 0 8 BS_REVISIOIN_SHA1)
ENDIF ( GIT_SHA1 STREQUAL GITDIR-NOTFOUND )

# set DEV_3RD_ROOT env variable to your custom location of 3rd party components
IF (DEFINED ENV{DEV_3RD_ROOT})
   GET_FILENAME_COMPONENT(FULL_3RD_PATH $ENV{DEV_3RD_ROOT} ABSOLUTE)
   SET(THIRD_PARTY_COMMON_DIR ${FULL_3RD_PATH}/${THIRD_PARTY_BUILD_MODE})
ELSE()
   GET_FILENAME_COMPONENT(TOP_DIRECTORY ${BLOCK_SETTLE_ROOT} PATH)
   SET(THIRD_PARTY_COMMON_DIR ${TOP_DIRECTORY}/3rd/${THIRD_PARTY_BUILD_MODE})
ENDIF()

# configure output dirs
SET( PROJECT_BINARY_DIR ${BLOCK_SETTLE_ROOT}/build_tests/${CMAKE_BUILD_TYPE} )

SET( EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin )
IF( WIN32 )
    # output libs to same path as binaries on windows just to make life easier
   SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /MP")
   SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /MP")
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

IF (WIN32)
   SET(MYSQL_ROOT "C:/Program Files/MySQL/MySQL Connector C 6.1")
ENDIF (WIN32)

SET(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${QT5_LIB_ROOT}/cmake")

FIND_PACKAGE(Qt5Core REQUIRED)
FIND_PACKAGE(Qt5Gui REQUIRED)
FIND_PACKAGE(Qt5Widgets REQUIRED)
FIND_PACKAGE(Qt5Network REQUIRED)
FIND_PACKAGE(Qt5PrintSupport REQUIRED)
FIND_PACKAGE(Qt5Sql REQUIRED)
FIND_PACKAGE(Qt5Charts REQUIRED)
FIND_PACKAGE(Qt5Qml REQUIRED)
FIND_PACKAGE(Qt5Charts REQUIRED)

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
      SET( ZMQ_LIB_NAME "libzmq-v140-mt-gd-4_3_2" )
   ELSE()
      SET( ZMQ_LIB_NAME "libzmq-v140-mt-4_3_2" )
   ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")
ELSE ()
   SET( ZMQ_LIB_NAME "libzmq.a" "zmq" )
ENDIF( WIN32 )
FIND_LIBRARY(ZMQ_LIB NAMES ${ZMQ_LIB_NAME} REQUIRED NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)

IF( NOT ZMQ_LIB )
   IF( WIN32 )
      IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
         SET( ZMQ_LIB_NAME "libzmq-v141-mt-gd-4_3_2" )
      ELSE()
         SET( ZMQ_LIB_NAME "libzmq-v141-mt-4_3_2" )
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

# setup botan
SET(BOTAN_PACKAGE_ROOT ${THIRD_PARTY_COMMON_DIR}/botan)
SET(BOTAN_LIB_DIR ${BOTAN_PACKAGE_ROOT}/lib)
SET(BOTAN_INCLUDE_DIR ${BOTAN_PACKAGE_ROOT}/include/botan-2)

IF (WIN32)
   SET(BOTAN_LIB_NAME botan.lib)
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
# we need UI lib for unit tests, and UI lib need QrEncode lib
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
   MESSAGE( FATAL_ERROR "Could not find ${LIBCP_LIB_NAME} in ${LIBCP_LIB_DIR}")
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

SET( PATH_TO_GENERATED ${BLOCK_SETTLE_ROOT}/generated_proto )
FILE( MAKE_DIRECTORY ${PATH_TO_GENERATED} )

INCLUDE_DIRECTORIES( ${BLOCK_SETTLE_ROOT}/AuthAPI/utils/cpp )

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

SET( PB_SOURCES_DIR ${BLOCK_SETTLE_ROOT}/BlocksettleServer)

SET(MDB_DIR ${CRYPTO_LIB_DIR}/lmdb/libraries/liblmdb)

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
      Qt5::QWindowsIntegrationPlugin
      )

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
      LIST(APPEND OS_SPECIFIC_LIBS
         msvcrtd
         ${QT5_PLUGINS_ROOT}/sqldrivers/qsqlited.lib
         ${QT5_PLUGINS_ROOT}/sqldrivers/qsqlmysqld.lib
         ${MYSQL_ROOT}/lib/libmysql.lib
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
      LIST(APPEND OS_SPECIFIC_LIBS
         msvcrt
         ${QT5_PLUGINS_ROOT}/sqldrivers/qsqlite.lib
         ${QT5_PLUGINS_ROOT}/sqldrivers/qsqlmysql.lib
         ${MYSQL_ROOT}/lib/libmysql.lib
         )
   ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")
ELSE(WIN32)
   FIND_PACKAGE( ZLIB )
   IF ( NOT ZLIB_FOUND )
      MESSAGE(FATAL_ERROR "ZLIB not found")
   ENDIF( NOT ZLIB_FOUND )

   IF ( APPLE )
      find_library(MYSQLCLIENTLIB libmysqlclient.dylib)
      IF( NOT MYSQLCLIENTLIB )
         MESSAGE(FATAL_ERROR "MySQL client lib not found")
      ENDIF( NOT MYSQLCLIENTLIB )

      FIND_PACKAGE( CUPS )
      IF ( NOT CUPS_FOUND )
         MESSAGE(FATAL_ERROR "CUPS not found")
      ENDIF( NOT CUPS_FOUND )

      SET(OS_SPECIFIC_LIBS
         "-framework AppKit"
         "-framework Security"
         "-framework SystemConfiguration"
         ${ZLIB_LIBRARIES}
         ${CUPS_LIBRARIES}
         )
      LIST(APPEND OS_SPECIFIC_LIBS
         "-framework Foundation"
         "-framework CoreFoundation"
         "-framework CoreServices"
         "-framework CoreText"
         "-framework Cocoa"
         "-framework IOKit"
         "-framework Carbon"
         "-framework Metal"
         "-framework QuartzCore"
         Qt5::QCocoaIntegrationPlugin
         )

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

         LIST (APPEND OS_SPECIFIC_LIBS
            ${QT5_PLUGINS_ROOT}/sqldrivers/libqsqlite_debug.a
            ${QT5_PLUGINS_ROOT}/sqldrivers/libqsqlmysql_debug.a
            ${MYSQLCLIENTLIB}
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

         LIST (APPEND OS_SPECIFIC_LIBS
            ${QT5_PLUGINS_ROOT}/sqldrivers/libqsqlite.a
            ${QT5_PLUGINS_ROOT}/sqldrivers/libqsqlmysql.a
            ${MYSQLCLIENTLIB}
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

      SET(QT_LIBS
         libqtharfbuzz.a
         libqtpcre2.a
         libQt5EventDispatcherSupport.a
         libQt5ThemeSupport.a
         libQt5ServiceSupport.a
         libQt5XcbQpa.a
         libQt5LinuxAccessibilitySupport.a
         libQt5AccessibilitySupport.a
         libQt5DBus.a
         libQt5EdidSupport.a
         )

      SET(OS_SPECIFIC_LIBS
         Qt5::QXcbIntegrationPlugin
         dl
         m
         ${CMAKE_THREAD_LIBS_INIT}
         ${ZLIB_LIBRARIES}
         ${XCB_LIBRARIES}
         ${QT5_LIB_ROOT}/libQt5FontDatabaseSupport.a
         ${FREETYPE_LIBRARIES}
         ${FONTCONFIG_LIBRARIES}
         X11::XCB
         X11
         Xi
         Xrender
         SM
         ICE
         png
         double-conversion
         xkbcommon
         xkbcommon-x11
         dbus-1
         ${CMAKE_THREAD_LIBS_INIT}
         )

         LIST (APPEND OS_SPECIFIC_LIBS
            ${QT5_PLUGINS_ROOT}/platforms/libqxcb.a
            ${QT5_PLUGINS_ROOT}/sqldrivers/libqsqlite.a
            ${QT5_PLUGINS_ROOT}/sqldrivers/libqsqlmysql.a
            mysqlclient
         )
   ENDIF( APPLE )
ENDIF(WIN32)

FOREACH(lib_name ${QT_LIBS})
   IF(NOT EXISTS ${QT5_LIB_ROOT}/${lib_name})
      MESSAGE(FATAL_ERROR "Could not locate required Qt lib ${QT5_LIB_ROOT}/${lib_name}")
   ENDIF()

   LIST(APPEND OS_SPECIFIC_LIBS ${QT5_LIB_ROOT}/${lib_name})
ENDFOREACH(lib_name)

IF( WIN32 )
   FILE(COPY ${SERVER_CONFIGURAIOTN_FILES} DESTINATION ${EXECUTABLE_OUTPUT_PATH}/Debug)
   FILE(COPY ${SERVER_CONFIGURAIOTN_FILES} DESTINATION ${EXECUTABLE_OUTPUT_PATH}/Release)
ELSE(WIN32)
   FILE(COPY ${SERVER_CONFIGURAIOTN_FILES} DESTINATION ${EXECUTABLE_OUTPUT_PATH})
ENDIF(WIN32)


ADD_SUBDIRECTORY( AuthAPI )
ADD_SUBDIRECTORY( Celer )
ADD_SUBDIRECTORY( common/Blocksettle_proto )

ADD_SUBDIRECTORY( common/WalletsLib )
ADD_SUBDIRECTORY( common/cppForSwig )
ADD_SUBDIRECTORY( common/CommonLib )
ADD_SUBDIRECTORY( common/CommonUI )
ADD_SUBDIRECTORY( common/BlocksettleNetworkingLib )
ADD_SUBDIRECTORY( common/BlockSettleUILib )

ADD_SUBDIRECTORY( UnitTests )

MESSAGE("3rd party root   : ${THIRD_PARTY_COMMON_DIR}")
MESSAGE("CMAKE_BUILD_TYPE : ${CMAKE_BUILD_TYPE}")
