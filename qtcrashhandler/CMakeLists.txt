if(NOT WIN32)
   return()
endif()

set(SOURCES
    detaildialog.cpp
    dumpsender.cpp
    main.cpp
    mainwidget.cpp
    mainwidget.ui
)

include_directories(${CMAKE_BINARY_DIR}/common/BlockSettleUILib)

add_executable(QtCrashHandler ${SOURCES})

target_link_libraries(QtCrashHandler
   ${CRYPTO_LIB_NAME}
   ${QT_LINUX_LIBS}
   Qt5::Widgets
   Qt5::Network
   ${QT_LIBS}
   ${OS_SPECIFIC_LIBS}
   ${OPENSSL_LIBS}
)

if(ENABLE_CRASH_HANDLER)
   target_compile_definitions(QtCrashHandler PRIVATE -DENABLE_CRASH_HANDLER)
endif()

set_target_properties(QtCrashHandler PROPERTIES AUTOUIC ON)

add_executable(CleanUpSystray WIN32 CleanUpSystray.cpp)

target_link_libraries(CleanUpSystray
   ${OS_SPECIFIC_LIBS}
   bufferoverflowU.lib
   libcmt.lib
)
