CMAKE_MINIMUM_REQUIRED( VERSION 3.3 )

PROJECT( ${CRYPTO_LIB_NAME} )

SET( SOURCES
   Accounts.cpp
   Addresses.cpp
   Assets.cpp
   AssetEncryption.cpp
   BDM_seder.cpp
   BinaryData.cpp
   BlockDataManagerConfig.cpp
   BtcUtils.cpp
   CoinSelection.cpp
   DataObject.cpp
   DBUtils.cpp
   DecryptedDataContainer.cpp
   DerivationScheme.cpp
   EncryptionUtils.cpp
   FcgiMessage.cpp
   JSON_codec.cpp
   lmdb_wrapper.cpp
   log.cpp
   ReentrantLock.cpp
   Script.cpp
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
   ${MDB_DIR}/mdb.c
   ${MDB_DIR}/midl.c
   ${MDB_DIR}/lmdbpp.cpp
   bech32/ref/c++/bech32.cpp
   bech32/ref/c++/segwit_addr.cpp
   )

SET( HEADERS
   Accounts.h
   Addresses.h
   Assets.h
   AssetEncryption.h
   BDM_seder.h
   BinaryData.h
   BlockDataManagerConfig.h
   BtcUtils.h
   CoinSelection.h
   DataObject.h
   DBUtils.h
   DecryptedDataContainer.h
   DerivationScheme.h
   EncryptionUtils.h
   FcgiMessage.h
   JSON_codec.h
   lmdb_wrapper.h
   ReentrantLock.h
   Signer.h
   Script.h
   SocketObject.h
   StringSockets.h
   SwigClient.h
   Transactions.h
   TxClasses.h
   TxEvalState.h
   Wallets.h
   WalletManager.h
   UniversalTimer.h
   ${MDB_DIR}/lmdb.h
   ${MDB_DIR}/midl.h
   ${MDB_DIR}/lmdbpp.h
   bech32/ref/c++/bech32.h
   bech32/ref/c++/segwit_addr.h
   )

# Extra files for cryptopp
SET( CRYPTOPP_EXTRA_SOURCES ${CRYPTOPP_EXTRA_DIR}/DetSign.cpp)
SET( CRYPTOPP_EXTRA_HEADERS ${CRYPTOPP_EXTRA_DIR}/DetSign.h)

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

ADD_LIBRARY( ${CRYPTO_LIB_NAME}
   ${SOURCES} ${HEADERS}
   ${CRYPTOPP_EXTRA_SOURCES} ${CRYPTOPP_EXTRA_HEADERS}
   ${OS_SPECIFIC_SOURCES} ${OS_SPECIFIC_HEADERS})

TARGET_LINK_LIBRARIES(${CRYPTO_LIB_NAME} ${CRYPTOPP_LIB} ${WS_LIB})

IF(NOT WIN32)
   SET(ARMORYDB_SOURCES)
   SET(ARMORYDB_HEADERS)
ENDIF(NOT WIN32)
