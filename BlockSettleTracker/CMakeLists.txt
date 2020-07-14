#
#
# ***********************************************************************************
# * Copyright (C) 2020 - 2020, BlockSettle AB
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

ADD_EXECUTABLE(${BLOCKSETTLE_TRACKER}
   ${TRACKER_SOURCES}
)

# There is something with linking order, system ssl and crypto used as a workaround for now
TARGET_LINK_LIBRARIES(${BLOCKSETTLE_TRACKER}
   ${BS_NETWORK_LIB_NAME}
   ${ZMQ_LIB}
   ${OPENSSL_LIBS}
   ${WS_LIB}
   ssl
   crypto
   dl
   ${CMAKE_THREAD_LIBS_INIT}
)
