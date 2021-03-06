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
CMAKE_MINIMUM_REQUIRED( VERSION 3.3 )

SET(CMAKE_CXX_STANDARD 14)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_EXTENSIONS OFF)

PROJECT( ${BLOCKSETTLE_UI_LIBRARY_NAME} )

configure_file(TerminalVersion.h.in TerminalVersion.h)

FILE(GLOB SOURCES
   *.cpp
   CustomControls/*.cpp
   InfoDialogs/*.cpp
   ManageEncryption/*.cpp
   Settings/*.cpp
   Trading/*.cpp
   ChatUI/*.cpp
   ChatUI/ChatWidgetStates/*.cpp
   ChatUI/OTCShieldWidgets/*.cpp
)
FILE(GLOB HEADERS
   *.h
   CustomControls/*.h
   InfoDialogs/*.h
   ManageEncryption/*.h
   Settings/*.h
   Trading/*.h
   ChatUI/*.h
   ChatUI/ChatWidgetStates/*.h
   ChatUI/OTCShieldWidgets/*.h
)
FILE(GLOB UI_FILES
   *.ui
   CustomControls/*.ui
   InfoDialogs/*.ui
   ManageEncryption/*.ui
   Settings/*.ui
   Trading/*.ui
   ChatUI/*.ui
   ChatUI/OTCShieldWidgets/*.ui
)

FILE(GLOB RESOURCE_FILES *.qrc)

INCLUDE_DIRECTORIES( ${WALLET_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${CRYPTO_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_NETWORK_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${COMMON_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${COMMON_UI_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( Settings )
INCLUDE_DIRECTORIES( Trading )

foreach(RESOURCE_FILE ${RESOURCE_FILES})
	qt5_add_resources( GENERATED_RESOURCES ${RESOURCE_FILE})
endforeach()
qt5_wrap_ui( GENERATED_UI ${UI_FILES} )

INCLUDE_DIRECTORIES( ${Qt5Gui_INCLUDE_DIRS} )
INCLUDE_DIRECTORIES( ${Qt5Widgets_INCLUDE_DIRS} )
INCLUDE_DIRECTORIES( ${Qt5Core_INCLUDE_DIRS})
INCLUDE_DIRECTORIES( ${Qt5Network_INCLUDE_DIRS} )
INCLUDE_DIRECTORIES( ${Qt5Qml_INCLUDE_DIRS} )
INCLUDE_DIRECTORIES( ${Qt5DBus_INCLUDE_DIRS} )
INCLUDE_DIRECTORIES( ${Qt5Charts_INCLUDE_DIRS} )
INCLUDE_DIRECTORIES( ${Qt5Sql_INCLUDE_DIRS} )

INCLUDE_DIRECTORIES( ${BOTAN_INCLUDE_DIR} )

ADD_LIBRARY( ${BLOCKSETTLE_UI_LIBRARY_NAME}
   ${SOURCES}
   ${HEADERS}
   ${GENERATED_RESOURCES}
   ${GENERATED_UI}
)

TARGET_LINK_LIBRARIES(${BLOCKSETTLE_UI_LIBRARY_NAME}
   ${CRYPTO_LIB_NAME}
   ${BOTAN_LIB}
   ${OS_SPECIFIC_LIBS}
   ${COMMON_LIB_NAME}
   ${COMMON_UI_LIB_NAME}
   ${QRENCODE_LIB}
   ${AUTH_PROTO_LIB_NAME}
   ${BS_PROTO_LIB_NAME}
   ${CELER_PROTO_LIB_NAME}
   Qt5::Core
   Qt5::Widgets
   Qt5::Gui
   Qt5::Network
   Qt5::Qml
   Qt5::Charts
)
