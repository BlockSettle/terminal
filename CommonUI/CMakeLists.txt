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

PROJECT( ${COMMON_UI_LIB_NAME} )

FILE(GLOB SOURCES *.cpp)
FILE(GLOB HEADERS *.h)
FILE(GLOB UI_FILES *.ui )
FILE(GLOB RESOURCE_FILES *.qrc)

IF (APPLE)
   FILE(GLOB OBJS *.mm)
   SET(SOURCES ${SOURCES} ${OBJS})
ENDIF(APPLE)

INCLUDE_DIRECTORIES( ${COMMON_LIB_INCLUDE_DIR} )

qt5_add_resources( GENERATED_RESOURCES ${RESOURCE_FILES})
qt5_wrap_ui( GENERATED_UI ${UI_FILES} )

INCLUDE_DIRECTORIES( ${Qt5Gui_INCLUDE_DIRS} )
INCLUDE_DIRECTORIES( ${Qt5Widgets_INCLUDE_DIRS} )
INCLUDE_DIRECTORIES( ${Qt5Core_INCLUDE_DIRS})
INCLUDE_DIRECTORIES( ${Qt5Network_INCLUDE_DIRS} )
INCLUDE_DIRECTORIES( ${Qt5PrintSupport_INCLUDE_DIRS} )
INCLUDE_DIRECTORIES( ${Qt5DBus_INCLUDE_DIRS} )

ADD_LIBRARY( ${COMMON_UI_LIB_NAME}
   ${SOURCES}
   ${HEADERS}
   ${GENERATED_RESOURCES}
   ${GENERATED_UI})

TARGET_LINK_LIBRARIES(${COMMON_UI_LIB_NAME}
   ${OS_SPECIFIC_LIBS}
   ${COMMON_LIB_NAME}
   Qt5::Core
   Qt5::Widgets
   Qt5::Gui
   Qt5::Network
   Qt5::PrintSupport
   Qt5::DBus
   )
