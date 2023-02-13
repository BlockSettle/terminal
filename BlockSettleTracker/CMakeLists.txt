#
#
# ***********************************************************************************
# * Copyright (C) 2020 - 2021, BlockSettle AB
# * Distributed under the GNU Affero General Public License (AGPL v3)
# * See LICENSE or http://www.gnu.org/licenses/agpl.html
# *
# **********************************************************************************
#
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.3)

SET(BLOCKSETTLE_TRACKER blocksettle_tracker)
PROJECT(${BLOCKSETTLE_TRACKER})

configure_file(BsTrackerVersion.h.in BsTrackerVersion.h)

SET(TRACKER_SOURCES
   main.cpp
)

INCLUDE_DIRECTORIES(${WALLET_LIB_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${BS_NETWORK_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${BS_COMMUNICATION_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${COMMON_LIB_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${BOTAN_INCLUDE_DIR})

ADD_EXECUTABLE(${BLOCKSETTLE_TRACKER}
   ${TRACKER_SOURCES}
)

IF(UNIX AND NOT APPLE)
   SET(OS_SPECIFIC_LIBS_TRACKER dl)
ELSE()
   SET(OS_SPECIFIC_LIBS_TRACKER ${OS_SPECIFIC_LIBS})
ENDIF()

TARGET_LINK_LIBRARIES(${BLOCKSETTLE_TRACKER}
   ${BS_NETWORK_LIB_NAME}
   ${WS_LIB}
   ${OPENSSL_LIBS}
   ${OS_SPECIFIC_LIBS_TRACKER}
   ${CMAKE_THREAD_LIBS_INIT}
)
