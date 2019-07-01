CMAKE_MINIMUM_REQUIRED( VERSION 3.3 )

PROJECT(${CRYPTO_LIB_NAME})

SET (SOURCES
   Accounts.cpp
   Addresses.cpp
   Assets.cpp
   AssetEncryption.cpp
   AsyncClient.cpp
   AuthorizedPeers.cpp
   BinaryData.cpp
   BIP150_151.cpp
   BIP32_Node.cpp
   BlockDataManagerConfig.cpp
   BtcUtils.cpp
   ClientClasses.cpp
   CoinSelection.cpp
   DBUtils.cpp
   DecryptedDataContainer.cpp
   DerivationScheme.cpp
   EncryptionUtils_libbtc.cpp
   hkdf.cpp
   HttpMessage.cpp
   JSON_codec.cpp
   KDF.cpp
   lmdb_wrapper.cpp
   log.cpp
   NetworkConfig.cpp
   ReentrantLock.cpp
   Script.cpp
   ScriptRecipient.cpp
   SecureBinaryData.cpp
   Signer.cpp
   SocketObject.cpp
   SwigClient.cpp
   StringSockets.cpp
   Transactions.cpp
   TxClasses.cpp
   TxEvalState.cpp
   UniversalTimer.cpp
   Wallets.cpp
   WalletManager.cpp
   WebSocketClient.cpp
   WebSocketMessage.cpp
   ${MDB_DIR}/mdb.c
   ${MDB_DIR}/midl.c
   ${MDB_DIR}/lmdbpp.cpp
   bech32/ref/c++/bech32.cpp
   bech32/ref/c++/segwit_addr.cpp
   chacha20poly1305/chacha.c
   chacha20poly1305/chachapoly_aead.c
   chacha20poly1305/poly1305.c
   )

IF (BUILD_TEST_TOOLS)
   LIST (APPEND SOURCES
      BDM_mainthread.cpp
      BDM_Server.cpp
      BitcoinP2P.cpp
      Blockchain.cpp
      BlockchainScanner.cpp
      BlockchainScanner_Super.cpp
      BlockDataMap.cpp
      BlockDataViewer.cpp
      BlockObj.cpp
      BlockUtils.cpp
      BtcWallet.cpp
      DatabaseBuilder.cpp
      HistoryPager.cpp
      LedgerEntry.cpp
      nodeRPC.cpp
      Progress.cpp
      ScrAddrFilter.cpp
      ScrAddrObj.cpp
      Server.cpp
      SshParser.cpp
      StoredBlockObj.cpp
      txio.cpp
      ZeroConf.cpp
      gtest/NodeUnitTest.cpp
   )

ENDIF (BUILD_TEST_TOOLS)

IF (WIN32)
   LIST(APPEND SOURCES SocketService_win32.cpp)
ELSE (WIN32)
   LIST(APPEND SOURCES SocketService_unix.cpp)
ENDIF (WIN32)

SET (HEADERS
   Accounts.h
   Addresses.h
   Assets.h
   AssetEncryption.h
   AsyncClient.h
   AuthorizedPeers.h
   BDVCodec.h
   BinaryData.h
   BIP150_151.h
   BIP32_Node.h
   BlockDataManagerConfig.h
   BtcUtils.h
   ClientClasses.h
   CoinSelection.h
   DBUtils.h
   DecryptedDataContainer.h
   DerivationScheme.h
   EncryptionUtils.h
   hkdf.h
   HttpMessage.h
   JSON_codec.h
   lmdb_wrapper.h
   NetworkConfig.h
   ReentrantLock.h
   SecureBinaryData.h
   Signer.h
   Script.h
   SocketObject.h
   SocketService.h
   StringSockets.h
   SwigClient.h
   Transactions.h
   TxClasses.h
   TxEvalState.h
   Wallets.h
   WalletManager.h
   WebSocketClient.h
   WebSocketMessage.h
   UniversalTimer.h
   ${MDB_DIR}/lmdb.h
   ${MDB_DIR}/midl.h
   ${MDB_DIR}/lmdbpp.h
   bech32/ref/c++/bech32.h
   bech32/ref/c++/segwit_addr.h
   )

IF (BUILD_TEST_TOOLS)
   LIST (APPEND HEADERS
      BDM_mainthread.h
      BDM_Server.h
      BitcoinP2p.h
      Blockchain.h
      BlockchainScanner.h
      BlockchainScanner_Super.h
      BlockDataMap.h
      BlockDataViewer.h
      BlockObj.h
      BlockUtils.h
      BtcWallet.h
      DatabaseBuilder.h
      HistoryPager.h
      LedgerEntry.h
      nodeRPC.h
      Progress.h
      ScrAddrFilter.h
      ScrAddrObj.h
      Server.h
      SshParser.h
      StoredBlockObj.h
      txio.h
      ZeroConf.h
      gtest/NodeUnitTest.h
   )
ENDIF (BUILD_TEST_TOOLS)

SET(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/protobuf)
FILE(GLOB PROTO_FILES ${PROTO_DIR}/*.proto)

FOREACH(PROTO_FILE ${PROTO_FILES})
   GET_FILENAME_COMPONENT(PROTO_NAME "${PROTO_FILE}" NAME_WE)

   SET(PROTO_SOURCE_FILE "${PROTO_DIR}/${PROTO_NAME}.pb.cc")
   SET(PROTO_HEADER_FILE "${PROTO_DIR}/${PROTO_NAME}.pb.h")

   SET(PROTO_SOURCE_FILES ${PROTO_SOURCE_FILES} ${PROTO_SOURCE_FILE})
   SET(PROTO_HEADER_FILES ${PROTO_HEADER_FILES} ${PROTO_HEADER_FILE})

   ADD_CUSTOM_COMMAND(OUTPUT ${PROTO_SOURCE_FILE}
                      OUTPUT ${PROTO_HEADER_FILE}
                      DEPENDS ${PROTO_FILE}
                      COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ${PROTO_FILE} --cpp_out=${PROTO_DIR} --proto_path=${PROTO_DIR}
   )
ENDFOREACH(PROTO_FILE)

SET(PROTO_SOURCES ${PROTO_SOURCE_FILES} ${PROTO_HEADER_FILES})

# leveldb_windows_port
IF(WIN32)
   SET( LEVELDB_WINDOWS_PORT_SOURCES
      ${LEVELDB_WINDOWS_PORT_DIR}/Win_TranslatePath.cpp
      ${LEVELDB_WINDOWS_PORT_DIR}/dirent_win32.cpp
      ${LEVELDB_WINDOWS_PORT_DIR}/mman.cpp
      ${LEVELDB_WINDOWS_PORT_DIR}/pthread_win32port.cpp
      ${LEVELDB_WINDOWS_PORT_DIR}/win32_posix.cc
      )


   SET(LEVELDB_WINDOWS_PORT_HEADERS
      ${LEVELDB_WINDOWS_PORT_DIR}/Win_TranslatePath.h
      ${LEVELDB_WINDOWS_PORT_DIR}/dirent_win32.h
      ${LEVELDB_WINDOWS_PORT_DIR}/file.h
      ${LEVELDB_WINDOWS_PORT_DIR}/mman.h
      ${LEVELDB_WINDOWS_PORT_DIR}/pthread.h
      ${LEVELDB_WINDOWS_PORT_DIR}/unistd.h
      ${LEVELDB_WINDOWS_PORT_DIR}/win32_posix.h
      )

   SET(OS_SPECIFIC_SOURCES ${LEVELDB_WINDOWS_PORT_SOURCES})
   SET(OS_SPECIFIC_HEADERS ${LEVELDB_WINDOWS_PORT_HEADERS})
ENDIF(WIN32)

ADD_LIBRARY(${CRYPTO_LIB_NAME}
   ${SOURCES} ${HEADERS} ${PROTO_SOURCES}
   ${CRYPTOPP_EXTRA_SOURCES} ${CRYPTOPP_EXTRA_HEADERS}
   ${OS_SPECIFIC_SOURCES} ${OS_SPECIFIC_HEADERS})

IF (BUILD_TEST_TOOLS)
   TARGET_COMPILE_DEFINITIONS( ${CRYPTO_LIB_NAME} PRIVATE
      COINBASE_MATURITY_TESTS)
ENDIF (BUILD_TEST_TOOLS)

TARGET_LINK_LIBRARIES(${CRYPTO_LIB_NAME}
   ${WS_LIB}
   ${CRYPTOPP_LIB}
   )

IF(NOT WIN32)
   SET(ARMORYDB_SOURCES)
   SET(ARMORYDB_HEADERS)
ENDIF(NOT WIN32)
