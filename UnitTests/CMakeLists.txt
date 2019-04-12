CMAKE_MINIMUM_REQUIRED( VERSION 3.3 )

SET(UNIT_TESTS unit_tests)
PROJECT( ${UNIT_TESTS} )

FILE(GLOB SOURCES *.cpp)
LIST (APPEND SOURCES ../RegtestController/RegtestController.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/BDM_mainthread.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/BDM_Server.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/BitcoinP2P.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/Blockchain.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/BlockchainScanner.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/BlockchainScanner_Super.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/BlockDataMap.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/BlockDataViewer.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/BlockObj.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/BlockUtils.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/BtcWallet.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/DatabaseBuilder.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/HistoryPager.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/LedgerEntry.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/lmdb_wrapper.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/nodeRPC.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/gtest/NodeUnitTest.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/Progress.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/ScrAddrFilter.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/ScrAddrObj.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/Server.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/SshParser.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/StoredBlockObj.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/TxClasses.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/txio.cpp)
LIST (APPEND SOURCES ../../common/cppForSwig/ZeroConf.cpp)

FILE(GLOB HEADERS *.h)
LIST (APPEND HEADERS ../RegtestController/RegtestController.h)
LIST (APPEND SOURCES ../../common/cppForSwig/BDM_mainthread.h)
LIST (APPEND SOURCES ../../common/cppForSwig/BDM_Server.h)
LIST (APPEND SOURCES ../../common/cppForSwig/BitcoinP2p.h)
LIST (APPEND SOURCES ../../common/cppForSwig/Blockchain.h)
LIST (APPEND SOURCES ../../common/cppForSwig/BlockchainScanner.h)
LIST (APPEND SOURCES ../../common/cppForSwig/BlockchainScanner_Super.h)
LIST (APPEND SOURCES ../../common/cppForSwig/BlockDataMap.h)
LIST (APPEND SOURCES ../../common/cppForSwig/BlockDataViewer.h)
LIST (APPEND SOURCES ../../common/cppForSwig/BlockObj.h)
LIST (APPEND SOURCES ../../common/cppForSwig/BlockUtils.h)
LIST (APPEND SOURCES ../../common/cppForSwig/BtcWallet.h)
LIST (APPEND SOURCES ../../common/cppForSwig/DatabaseBuilder.h)
LIST (APPEND SOURCES ../../common/cppForSwig/HistoryPager.h)
LIST (APPEND SOURCES ../../common/cppForSwig/LedgerEntry.h)
LIST (APPEND SOURCES ../../common/cppForSwig/lmdb_wrapper.h)
LIST (APPEND SOURCES ../../common/cppForSwig/nodeRPC.h)
LIST (APPEND SOURCES ../../common/cppForSwig/gtest/NodeUnitTest.h)
LIST (APPEND SOURCES ../../common/cppForSwig/Progress.h)
LIST (APPEND SOURCES ../../common/cppForSwig/ScrAddrFilter.h)
LIST (APPEND SOURCES ../../common/cppForSwig/ScrAddrObj.h)
LIST (APPEND SOURCES ../../common/cppForSwig/Server.h)
LIST (APPEND SOURCES ../../common/cppForSwig/SshParser.h)
LIST (APPEND SOURCES ../../common/cppForSwig/StoredBlockObj.h)
LIST (APPEND SOURCES ../../common/cppForSwig/TxClasses.h)
LIST (APPEND SOURCES ../../common/cppForSwig/txio.h)
LIST (APPEND SOURCES ../../common/cppForSwig/ZeroConf.h)

INCLUDE_DIRECTORIES( ../RegtestController )
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
