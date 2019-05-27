CMAKE_MINIMUM_REQUIRED( VERSION 3.3 )

SET(UNIT_TESTS unit_tests)
PROJECT( ${UNIT_TESTS} )

FILE(GLOB SOURCES 
   *.cpp
   ../common/cppForSwig/BDM_Server.cpp
   ../common/cppForSwig/BDM_mainthread.cpp
   ../common/cppForSwig/BitcoinP2P.cpp
   ../common/cppForSwig/Blockchain.cpp
   ../common/cppForSwig/BlockchainScanner.cpp
   ../common/cppForSwig/BlockchainScanner_Super.cpp
   ../common/cppForSwig/BlockDataMap.cpp
   ../common/cppForSwig/BlockDataViewer.cpp
   ../common/cppForSwig/BlockObj.cpp
   ../common/cppForSwig/BlockUtils.cpp
   ../common/cppForSwig/BtcWallet.cpp
   ../common/cppForSwig/DatabaseBuilder.cpp
   ../common/cppForSwig/HistoryPager.cpp
   ../common/cppForSwig/LedgerEntry.cpp
   ../common/cppForSwig/lmdb_wrapper.cpp
   ../common/cppForSwig/nodeRPC.cpp
   ../common/cppForSwig/gtest/NodeUnitTest.cpp
   ../common/cppForSwig/Progress.cpp
   ../common/cppForSwig/ScrAddrFilter.cpp
   ../common/cppForSwig/ScrAddrObj.cpp
   ../common/cppForSwig/Server.cpp
   ../common/cppForSwig/SshParser.cpp
   ../common/cppForSwig/StoredBlockObj.cpp
   ../common/cppForSwig/txio.cpp
   ../common/cppForSwig/ZeroConf.cpp
)

FILE(GLOB HEADERS *.h)

INCLUDE_DIRECTORIES( ${BLOCKSETTLE_UI_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_NETWORK_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${COMMON_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${CRYPTO_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${WALLET_LIB_INCLUDE_DIR} )

INCLUDE_DIRECTORIES( ${BS_COMMUNICATION_INCLUDE_DIR} )

INCLUDE_DIRECTORIES( ${PATH_TO_GENERATED} )
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
   ${BLOCKSETTLE_UI_LIBRARY_NAME}
   ${BS_NETWORK_LIB_NAME}
   ${CPP_WALLET_LIB_NAME}
   ${CRYPTO_LIB_NAME}
   ${BOTAN_LIB}
   ${OS_SPECIFIC_LIBS}
   ${PROTO_LIB}
   ${ZMQ_LIB}
   ${WS_LIB}
   ${GTEST_LIB}
   Qt5::Core
   Qt5::Gui
   Qt5::Network
   Qt5::Widgets
   ${OPENSSL_LIBS}
   )

TARGET_INCLUDE_DIRECTORIES( ${UNIT_TESTS}
   PRIVATE ${BOTAN_INCLUDE_DIR}
)
