if(CMAKE_BUILD_TYPE STREQUAL Debug)
    option(ENABLE_LTO "enable link-time-optimizations" OFF)
else()
    option(ENABLE_LTO "enable link-time-optimizations" ON)
endif()

if(NOT ENABLE_LTO)
    return()
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    include(ProcessorCount)
    ProcessorCount(num_cpus)

    include(GCCToolchain)

    add_compile_options(-flto=${num_cpus})

    set(CMAKE_EXE_LINKER_FLAGS          "${CMAKE_EXE_LINKER_FLAGS}          -flto=${num_cpus}")
    set(CMAKE_SHARED_LINKER_FLAGS       "${CMAKE_SHARED_LINKER_FLAGS}       -flto=${num_cpus}")
    set(CMAKE_MODULE_LINKER_FLAGS       "${CMAKE_MODULE_LINKER_FLAGS}       -flto=${num_cpus}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    include(LLVMToolchain)

    add_compile_options(-flto)

    set(CMAKE_EXE_LINKER_FLAGS          "${CMAKE_EXE_LINKER_FLAGS}          -flto")
    set(CMAKE_SHARED_LINKER_FLAGS       "${CMAKE_SHARED_LINKER_FLAGS}       -flto")
    set(CMAKE_MODULE_LINKER_FLAGS       "${CMAKE_MODULE_LINKER_FLAGS}       -flto")
elseif(MSVC)
    add_compile_options(/GL)

    set(CMAKE_EXE_LINKER_FLAGS          "${CMAKE_EXE_LINKER_FLAGS}          /LTCG")
    set(CMAKE_SHARED_LINKER_FLAGS       "${CMAKE_SHARED_LINKER_FLAGS}       /LTCG")
    set(CMAKE_MODULE_LINKER_FLAGS       "${CMAKE_MODULE_LINKER_FLAGS}       /LTCG")
    set(CMAKE_STATIC_LINKER_FLAGS       "${CMAKE_STATIC_LINKER_FLAGS}       /LTCG")
endif()
