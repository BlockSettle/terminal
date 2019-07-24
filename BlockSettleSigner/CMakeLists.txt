PROJECT( ${SIGNER_APP_NAME} )

configure_file(SignerVersion.h.in SignerVersion.h)

SET(BSNetwork ${BLOCK_SETTLE_ROOT}/common/BlocksettleNetworkingLib)

SET(SOURCES
   main.cpp
   HeadlessApp.cpp
   HeadlessSettings.cpp
   SignerAdapterListener.cpp
   ${BSNetwork}/ActiveStreamClient.cpp
   ${BSNetwork}/IdStringGenerator.cpp
   ${BSNetwork}/HeadlessContainerListener.cpp
   ${BSNetwork}/MessageHolder.cpp
   ${BSNetwork}/ZmqContext.cpp
   ${BSNetwork}/ZmqHelperFunctions.cpp
   ${BSNetwork}/ZmqServerConnection.cpp
   ${BSNetwork}/ZmqStreamServerConnection.cpp
   ${BSNetwork}/ZMQ_BIP15X_ServerConnection.cpp
   ${BSNetwork}/ZMQ_BIP15X_Msg.cpp
   ${BSNetwork}/ZMQ_BIP15X_Helpers.cpp
   ${BSNetwork}/PasswordDialogDataWrapper.cpp
)

SET(HEADERS
   HeadlessApp.h
   HeadlessSettings.h
   SignerAdapterListener.h
   ${BSNetwork}/PasswordDialogDataWrapper.h
)

INCLUDE_DIRECTORIES( ${COMMON_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_NETWORK_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_COMMON_ENUMS_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_COMMUNICATION_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_STORAGE_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${WALLET_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${CRYPTO_LIB_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${PATH_TO_GENERATED} )
INCLUDE_DIRECTORIES( ${NETTY_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BS_TERMINAL_API_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${MARKET_ENUMS_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BOTAN_INCLUDE_DIR} )

IF(WIN32)
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
ADD_EXECUTABLE(${SIGNER_APP_NAME} WIN32 ${SOURCES} ${HEADERS} blocksettle.rc)
ELSE(WIN32)
ADD_EXECUTABLE(${SIGNER_APP_NAME} ${SOURCES} ${HEADERS})
ENDIF(WIN32)

TARGET_LINK_LIBRARIES(
   ${SIGNER_APP_NAME}
   ${CPP_WALLET_LIB_NAME}
   ${CRYPTO_LIB_NAME}
   ${BOTAN_LIB}
   ${ZMQ_LIB}
   ${OS_SPECIFIC_LIBS}
   ${OPENSSL_LIBS}
)

ADD_SUBDIRECTORY(interfaces)
