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

PROJECT(${TERMINAL_CORE_NAME})

FILE(GLOB SOURCES
   *.cpp
)
FILE(GLOB HEADERS
   *.h
)

INCLUDE_DIRECTORIES(${COMMON_LIB_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${COMMON_UI_LIB_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${BOTAN_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${WALLET_LIB_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${BLOCKSETTLE_UI_INCLUDE_DIR}) # temporary - until some files reside in old structure

ADD_LIBRARY(${TERMINAL_CORE_NAME}
   ${SOURCES}
   ${HEADERS}
)

TARGET_INCLUDE_DIRECTORIES(${TERMINAL_CORE_NAME}
   PUBLIC ${BLOCK_SETTLE_ROOT}/Core
   PRIVATE ${BLOCK_SETTLE_ROOT}/common/BlocksettleNetworkingLib
)

TARGET_LINK_LIBRARIES(${TERMINAL_CORE_NAME}
   ${BS_NETWORK_LIB_NAME}
   ${CRYPTO_LIB_NAME}
   Qt5::Network
   Qt5::Core
)
#   Qt5::Sql
