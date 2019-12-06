CMAKE_MINIMUM_REQUIRED( VERSION 3.3 )

PROJECT(${CRYPTO_LIB_NAME})

SET(CPP_FOR_SWIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../ArmoryDB/cppForSwig)

SET (SOURCES
   ${CPP_FOR_SWIG_ROOT}/Accounts.cpp
   ${CPP_FOR_SWIG_ROOT}/Addresses.cpp
   ${CPP_FOR_SWIG_ROOT}/Assets.cpp
   ${CPP_FOR_SWIG_ROOT}/AssetEncryption.cpp
   ${CPP_FOR_SWIG_ROOT}/AsyncClient.cpp
   ${CPP_FOR_SWIG_ROOT}/AuthorizedPeers.cpp
   ${CPP_FOR_SWIG_ROOT}/BinaryData.cpp
   ${CPP_FOR_SWIG_ROOT}/BIP150_151.cpp
   ${CPP_FOR_SWIG_ROOT}/BIP32_Node.cpp
   ${CPP_FOR_SWIG_ROOT}/BlockDataManagerConfig.cpp
   ${CPP_FOR_SWIG_ROOT}/BtcUtils.cpp
   ${CPP_FOR_SWIG_ROOT}/ClientClasses.cpp
   ${CPP_FOR_SWIG_ROOT}/CoinSelection.cpp
   ${CPP_FOR_SWIG_ROOT}/DBUtils.cpp
   ${CPP_FOR_SWIG_ROOT}/DecryptedDataContainer.cpp
   ${CPP_FOR_SWIG_ROOT}/DerivationScheme.cpp
   ${CPP_FOR_SWIG_ROOT}/EncryptionUtils_libbtc.cpp
   ${CPP_FOR_SWIG_ROOT}/hkdf.cpp
   ${CPP_FOR_SWIG_ROOT}/HttpMessage.cpp
   ${CPP_FOR_SWIG_ROOT}/JSON_codec.cpp
   ${CPP_FOR_SWIG_ROOT}/KDF.cpp
   ${CPP_FOR_SWIG_ROOT}/lmdb_wrapper.cpp
   ${CPP_FOR_SWIG_ROOT}/log.cpp
   ${CPP_FOR_SWIG_ROOT}/NetworkConfig.cpp
   ${CPP_FOR_SWIG_ROOT}/ReentrantLock.cpp
   ${CPP_FOR_SWIG_ROOT}/Script.cpp
   ${CPP_FOR_SWIG_ROOT}/ScriptRecipient.cpp
   ${CPP_FOR_SWIG_ROOT}/SecureBinaryData.cpp
   ${CPP_FOR_SWIG_ROOT}/Signer.cpp
   ${CPP_FOR_SWIG_ROOT}/SocketObject.cpp
   ${CPP_FOR_SWIG_ROOT}/SwigClient.cpp
   ${CPP_FOR_SWIG_ROOT}/StringSockets.cpp
   ${CPP_FOR_SWIG_ROOT}/TerminalPassphrasePrompt.cpp
   ${CPP_FOR_SWIG_ROOT}/Transactions.cpp
   ${CPP_FOR_SWIG_ROOT}/TxClasses.cpp
   ${CPP_FOR_SWIG_ROOT}/TxEvalState.cpp
   ${CPP_FOR_SWIG_ROOT}/UniversalTimer.cpp
   ${CPP_FOR_SWIG_ROOT}/WalletFileInterface.cpp
   ${CPP_FOR_SWIG_ROOT}/WalletHeader.cpp
   ${CPP_FOR_SWIG_ROOT}/Wallets.cpp
   ${CPP_FOR_SWIG_ROOT}/WalletManager.cpp
   ${CPP_FOR_SWIG_ROOT}/WebSocketClient.cpp
   ${CPP_FOR_SWIG_ROOT}/WebSocketMessage.cpp
   ${CPP_FOR_SWIG_ROOT}/lmdb/libraries/liblmdb/mdb.c
   ${CPP_FOR_SWIG_ROOT}/lmdb/libraries/liblmdb/midl.c
   ${CPP_FOR_SWIG_ROOT}/lmdb/libraries/liblmdb/lmdbpp.cpp
   ${CPP_FOR_SWIG_ROOT}/chacha20poly1305/chacha.c
   ${CPP_FOR_SWIG_ROOT}/chacha20poly1305/chachapoly_aead.c
   ${CPP_FOR_SWIG_ROOT}/chacha20poly1305/poly1305.c
   )

IF (BUILD_TEST_TOOLS)
   LIST (APPEND SOURCES
      ${CPP_FOR_SWIG_ROOT}/BDM_mainthread.cpp
      ${CPP_FOR_SWIG_ROOT}/BDM_Server.cpp
      ${CPP_FOR_SWIG_ROOT}/BitcoinP2P.cpp
      ${CPP_FOR_SWIG_ROOT}/Blockchain.cpp
      ${CPP_FOR_SWIG_ROOT}/BlockchainScanner.cpp
      ${CPP_FOR_SWIG_ROOT}/BlockchainScanner_Super.cpp
      ${CPP_FOR_SWIG_ROOT}/BlockDataMap.cpp
      ${CPP_FOR_SWIG_ROOT}/BlockDataViewer.cpp
      ${CPP_FOR_SWIG_ROOT}/BlockObj.cpp
      ${CPP_FOR_SWIG_ROOT}/BlockUtils.cpp
      ${CPP_FOR_SWIG_ROOT}/BtcWallet.cpp
      ${CPP_FOR_SWIG_ROOT}/DatabaseBuilder.cpp
      ${CPP_FOR_SWIG_ROOT}/HistoryPager.cpp
      ${CPP_FOR_SWIG_ROOT}/LedgerEntry.cpp
      ${CPP_FOR_SWIG_ROOT}/nodeRPC.cpp
      ${CPP_FOR_SWIG_ROOT}/Progress.cpp
      ${CPP_FOR_SWIG_ROOT}/ScrAddrFilter.cpp
      ${CPP_FOR_SWIG_ROOT}/ScrAddrObj.cpp
      ${CPP_FOR_SWIG_ROOT}/Server.cpp
      ${CPP_FOR_SWIG_ROOT}/SshParser.cpp
      ${CPP_FOR_SWIG_ROOT}/StoredBlockObj.cpp
      ${CPP_FOR_SWIG_ROOT}/txio.cpp
      ${CPP_FOR_SWIG_ROOT}/ZeroConf.cpp
      ${CPP_FOR_SWIG_ROOT}/gtest/NodeUnitTest.cpp
   )

ENDIF (BUILD_TEST_TOOLS)

IF (WIN32)
   LIST(APPEND SOURCES ${CPP_FOR_SWIG_ROOT}/SocketService_win32.cpp)
ELSE (WIN32)
   LIST(APPEND SOURCES ${CPP_FOR_SWIG_ROOT}/SocketService_unix.cpp)
ENDIF (WIN32)

SET (HEADERS
   ${CPP_FOR_SWIG_ROOT}/Accounts.h
   ${CPP_FOR_SWIG_ROOT}/Addresses.h
   ${CPP_FOR_SWIG_ROOT}/Assets.h
   ${CPP_FOR_SWIG_ROOT}/AssetEncryption.h
   ${CPP_FOR_SWIG_ROOT}/AsyncClient.h
   ${CPP_FOR_SWIG_ROOT}/AuthorizedPeers.h
   ${CPP_FOR_SWIG_ROOT}/BDVCodec.h
   ${CPP_FOR_SWIG_ROOT}/BinaryData.h
   ${CPP_FOR_SWIG_ROOT}/BIP150_151.h
   ${CPP_FOR_SWIG_ROOT}/BIP32_Node.h
   ${CPP_FOR_SWIG_ROOT}/BlockDataManagerConfig.h
   ${CPP_FOR_SWIG_ROOT}/BtcUtils.h
   ${CPP_FOR_SWIG_ROOT}/ClientClasses.h
   ${CPP_FOR_SWIG_ROOT}/CoinSelection.h
   ${CPP_FOR_SWIG_ROOT}/DBUtils.h
   ${CPP_FOR_SWIG_ROOT}/DecryptedDataContainer.h
   ${CPP_FOR_SWIG_ROOT}/DerivationScheme.h
   ${CPP_FOR_SWIG_ROOT}/EncryptionUtils.h
   ${CPP_FOR_SWIG_ROOT}/hkdf.h
   ${CPP_FOR_SWIG_ROOT}/HttpMessage.h
   ${CPP_FOR_SWIG_ROOT}/JSON_codec.h
   ${CPP_FOR_SWIG_ROOT}/lmdb_wrapper.h
   ${CPP_FOR_SWIG_ROOT}/NetworkConfig.h
   ${CPP_FOR_SWIG_ROOT}/ReentrantLock.h
   ${CPP_FOR_SWIG_ROOT}/SecureBinaryData.h
   ${CPP_FOR_SWIG_ROOT}/Signer.h
   ${CPP_FOR_SWIG_ROOT}/Script.h
   ${CPP_FOR_SWIG_ROOT}/SocketObject.h
   ${CPP_FOR_SWIG_ROOT}/SocketService.h
   ${CPP_FOR_SWIG_ROOT}/StringSockets.h
   ${CPP_FOR_SWIG_ROOT}/SwigClient.h
   ${CPP_FOR_SWIG_ROOT}/TerminalPassphrasePrompt.h
   ${CPP_FOR_SWIG_ROOT}/Transactions.h
   ${CPP_FOR_SWIG_ROOT}/TxClasses.h
   ${CPP_FOR_SWIG_ROOT}/TxEvalState.h
   ${CPP_FOR_SWIG_ROOT}/WalletFileInterface.h
   ${CPP_FOR_SWIG_ROOT}/WalletHeader.h
   ${CPP_FOR_SWIG_ROOT}/Wallets.h
   ${CPP_FOR_SWIG_ROOT}/WalletManager.h
   ${CPP_FOR_SWIG_ROOT}/WebSocketClient.h
   ${CPP_FOR_SWIG_ROOT}/WebSocketMessage.h
   ${CPP_FOR_SWIG_ROOT}/UniversalTimer.h
   ${CPP_FOR_SWIG_ROOT}/lmdb/libraries/liblmdb/lmdb.h
   ${CPP_FOR_SWIG_ROOT}/lmdb/libraries/liblmdb/midl.h
   ${CPP_FOR_SWIG_ROOT}/lmdb/libraries/liblmdb/lmdbpp.h
   )

IF (BUILD_TEST_TOOLS)
   LIST (APPEND HEADERS
      ${CPP_FOR_SWIG_ROOT}/BDM_mainthread.h
      ${CPP_FOR_SWIG_ROOT}/BDM_Server.h
      ${CPP_FOR_SWIG_ROOT}/BitcoinP2p.h
      ${CPP_FOR_SWIG_ROOT}/Blockchain.h
      ${CPP_FOR_SWIG_ROOT}/BlockchainScanner.h
      ${CPP_FOR_SWIG_ROOT}/BlockchainScanner_Super.h
      ${CPP_FOR_SWIG_ROOT}/BlockDataMap.h
      ${CPP_FOR_SWIG_ROOT}/BlockDataViewer.h
      ${CPP_FOR_SWIG_ROOT}/BlockObj.h
      ${CPP_FOR_SWIG_ROOT}/BlockUtils.h
      ${CPP_FOR_SWIG_ROOT}/BtcWallet.h
      ${CPP_FOR_SWIG_ROOT}/DatabaseBuilder.h
      ${CPP_FOR_SWIG_ROOT}/HistoryPager.h
      ${CPP_FOR_SWIG_ROOT}/LedgerEntry.h
      ${CPP_FOR_SWIG_ROOT}/nodeRPC.h
      ${CPP_FOR_SWIG_ROOT}/Progress.h
      ${CPP_FOR_SWIG_ROOT}/ScrAddrFilter.h
      ${CPP_FOR_SWIG_ROOT}/ScrAddrObj.h
      ${CPP_FOR_SWIG_ROOT}/Server.h
      ${CPP_FOR_SWIG_ROOT}/SshParser.h
      ${CPP_FOR_SWIG_ROOT}/StoredBlockObj.h
      ${CPP_FOR_SWIG_ROOT}/txio.h
      ${CPP_FOR_SWIG_ROOT}/ZeroConf.h
      ${CPP_FOR_SWIG_ROOT}/gtest/NodeUnitTest.h
   )
ENDIF (BUILD_TEST_TOOLS)

SET(PROTO_DIR ${CPP_FOR_SWIG_ROOT}/protobuf)
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
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/Win_TranslatePath.cpp
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/dirent_win32.cpp
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/dirent_win32.cpp
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/mman.cpp
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/pthread_win32port.cpp
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/win32_posix.cc
      )


   SET(LEVELDB_WINDOWS_PORT_HEADERS
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/Win_TranslatePath.h
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/dirent_win32.h
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/file.h
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/mman.h
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/pthread.h
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/unistd.h
      ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix/win32_posix.h
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

TARGET_INCLUDE_DIRECTORIES(${CRYPTO_LIB_NAME}
   PUBLIC ${CPP_FOR_SWIG_ROOT}
   PUBLIC ${CPP_FOR_SWIG_ROOT}/lmdb/libraries/liblmdb
)

IF(WIN32)
   TARGET_INCLUDE_DIRECTORIES(${CRYPTO_LIB_NAME} PUBLIC ${CPP_FOR_SWIG_ROOT}/leveldb_windows_port/win32_posix)
ENDIF(WIN32)
