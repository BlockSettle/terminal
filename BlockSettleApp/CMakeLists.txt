#
#
# ***********************************************************************************
# * Copyright (C) 2018 - 2020, BlockSettle AB
# * Distributed under the GNU Affero General Public License (AGPL v3)
# * See LICENSE or http://www.gnu.org/licenses/agpl.html
# *
# **********************************************************************************
#
#
PROJECT( ${BLOCKSETTLE_APP_NAME} )

FILE(GLOB SOURCES *.cpp)
FILE(GLOB HEADERS *.h)

include(CodeSign)

INCLUDE_DIRECTORIES( ${COMMON_UI_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${COMMON_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BLOCKSETTLE_UI_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_NETWORK_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_COMMON_ENUMS_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_COMMUNICATION_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_HW_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${WALLET_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${CRYPTO_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BOTAN_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${Qt5Svg_INCLUDE_DIRS} )

IF ( APPLE )
    include(Notarize)

    SET( BUNDLE_NAME "BlockSettle Terminal" )

    SET( MACOSX_BUNDLE_BUNDLE_NAME ${BUNDLE_NAME} )
    SET( MACOSX_BUNDLE_GUI_IDENTIFIER "com.blocksettle.terminal" )
    SET( MACOSX_BUNDLE_BUNDLE_VERSION ${BS_VERSION_STRING})
    SET( MACOSX_BUNDLE_SHORT_VERSION_STRING ${BS_VERSION_STRING})
    SET( MACOSX_BUNDLE_LONG_VERSION_STRING ${BS_VERSION_STRING})

    SET( MACOSX_BUNDLE_ICON_FILE "Blocksettle.icns" )

    SET(BS_TERMINAL_ICNS_FILE "${BLOCKSETTLE_UI_INCLUDE_DIR}/resources/Blocksettle.icns")

    SET_SOURCE_FILES_PROPERTIES( ${BS_TERMINAL_ICNS_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    ADD_EXECUTABLE( ${BLOCKSETTLE_APP_NAME} MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${BS_TERMINAL_ICNS_FILE})
    SET_TARGET_PROPERTIES( ${BLOCKSETTLE_APP_NAME} PROPERTIES OUTPUT_NAME "${BUNDLE_NAME}" )

    code_sign_is_enabled(can_sign)
    if (can_sign)
        set(FILES_TO_SIGN "${EXECUTABLE_OUTPUT_PATH}/${BUNDLE_NAME}.app")
        configure_file(../common/build_scripts/CMakeModules/CodeSignScript.cmake.in sign_app.cmake)
        list(APPEND postpkg
            COMMAND "${CMAKE_COMMAND}" -E echo "Code signing ${BUNDLE_NAME}.app"
            COMMAND "${CMAKE_COMMAND}" -P "sign_app.cmake"
        )
        add_custom_target(sign
            ${postpkg}
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            VERBATIM
            USES_TERMINAL
        )
    endif ()
 
    ELSEIF ( WIN32 )
    ADD_EXECUTABLE( ${BLOCKSETTLE_APP_NAME} WIN32 ${SOURCES} ${HEADERS} blocksettle.rc )

ELSE ()
    ADD_EXECUTABLE( ${BLOCKSETTLE_APP_NAME} ${SOURCES} ${HEADERS} )
ENDIF ()

IF(UNIX AND NOT APPLE)
    IF(BUILD_APPIMAGE)
        configure_file(../common/build_scripts/CMakeModules/AppImageScript.cmake.in appimage.cmake)
	ADD_CUSTOM_COMMAND(TARGET ${BLOCKSETTLE_APP_NAME} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E echo "Building ${BUNDLE_NAME}.AppImage"
            COMMAND "${CMAKE_COMMAND}" -P "appimage.cmake"
	)
    ENDIF()
ENDIF()
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath='$ORIGIN'")

TARGET_LINK_LIBRARIES( ${BLOCKSETTLE_APP_NAME}
   ${TERMINAL_CORE_NAME}
   ${BLOCKSETTLE_HW_LIBRARY_NAME}
   ${TERMINAL_GUI_QT_NAME}
   ${TERMINAL_GUI_QTQUICK_NAME}
   ${BLOCKSETTLE_UI_LIBRARY_NAME}
   ${BS_NETWORK_LIB_NAME}
   ${CURL_LIB}
   ${CPP_WALLET_LIB_NAME}
   ${CRYPTO_LIB_NAME}
   ${BOTAN_LIB}
   ${QRENCODE_LIB}
   ${QT_LINUX_LIBS}
   ${WS_LIB}
   Qt5::Qml
   Qt5::Core
   Qt5::Widgets
   Qt5::Gui
   Qt5::QSQLiteDriverPlugin
   Qt5::Sql
   Qt5::Network
   Qt5::PrintSupport
   Qt5::Core
   Qt5::Svg
   Qt5::DBus
   ${QT_LIBS}
   ${OS_SPECIFIC_LIBS}
   ${OPENSSL_LIBS}
   )
