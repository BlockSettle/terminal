#
#
# ***********************************************************************************
# * Copyright (C) 2019 - 2020, BlockSettle AB
# * Distributed under the GNU Affero General Public License (AGPL v3)
# * See LICENSE or http://www.gnu.org/licenses/agpl.html
# *
# **********************************************************************************
#
#
CMAKE_MINIMUM_REQUIRED( VERSION 3.3 )

SET(UNIT_TESTS unit_tests)
PROJECT( ${UNIT_TESTS} )

FILE(GLOB SOURCES *.cpp)
FILE(GLOB HEADERS *.h)

# These files are not included even if terminalGUI is built with BUILD_TEST_TOOLS=1,
# including them once again for now as a workaround
LIST (APPEND SOURCES
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/BDM_mainthread.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/BDM_Server.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/BitcoinP2P.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/Blockchain.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/BlockchainScanner.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/BlockchainScanner_Super.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/BlockDataMap.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/BlockDataViewer.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/BlockObj.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/BlockUtils.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/BtcWallet.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/DatabaseBuilder.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/HistoryPager.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/LedgerEntry.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/nodeRPC.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/Progress.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/ScrAddrFilter.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/ScrAddrObj.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/Server.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/SshParser.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/StoredBlockObj.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/txio.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/ZeroConf.cpp
   ${TERMINAL_GUI_ROOT}/common/ArmoryDB/cppForSwig/gtest/NodeUnitTest.cpp
   )

INCLUDE_DIRECTORIES( ${BLOCKSETTLE_UI_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_NETWORK_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${COMMON_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${CRYPTO_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${WALLET_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_COMMUNICATION_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${PATH_TO_GENERATED} )
INCLUDE_DIRECTORIES( ${BLOCK_SETTLE_ROOT}/Core )
INCLUDE_DIRECTORIES( ${NETTY_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_COMMON_ENUMS_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_TERMINAL_API_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${MARKET_ENUMS_INCLUDE_DIR} )

ADD_EXECUTABLE( ${UNIT_TESTS}
   ${SOURCES}
   ${HEADERS}
)

TARGET_COMPILE_DEFINITIONS( ${UNIT_TESTS} PRIVATE
   SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG
   COINBASE_MATURITY_TESTS
)

TARGET_LINK_LIBRARIES( ${UNIT_TESTS}
   ${TERMINAL_CORE_NAME}
   ${BLOCKSETTLE_UI_LIBRARY_NAME}
   ${CPP_WALLET_LIB_NAME}
   ${BS_NETWORK_LIB_NAME}
   ${CRYPTO_LIB_NAME}
   ${LIBBTC_LIB}
   ${MPIR_LIB}
   ${BOTAN_LIB}
   ${COMMON_LIB}
   ${PROTO_LIB}
   ${GTEST_LIB}
   ${BS_PROTO_LIB_NAME}
   ${AUTH_PROTO_LIB}
   ${BS_PROTO_LIB}
   ${CELER_PROTO_LIB}

   ${QT_LINUX_LIBS}
   ${WS_LIB}
   Qt5::Qml
   Qt5::Core
   Qt5::Widgets
   Qt5::Gui
   Qt5::Network
   Qt5::PrintSupport
   Qt5::Core
   Qt5::Svg
   Qt5::DBus
   ${QT_LIBS}
   ${OPENSSL_LIBS}
   ${OS_SPECIFIC_LIBS}
)

TARGET_INCLUDE_DIRECTORIES( ${UNIT_TESTS}
   PRIVATE ${BOTAN_INCLUDE_DIR}
)
