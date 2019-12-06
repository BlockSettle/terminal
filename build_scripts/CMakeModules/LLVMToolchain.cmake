function(use_llvm_toolchain)
    if(CMAKE_C_COMPILER_ID STREQUAL Clang)
        set(compiler ${CMAKE_C_COMPILER})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        set(compiler ${CMAKE_CXX_COMPILER})
    else()
        return()
    endif()

    foreach(tool ar ranlib ld nm objdump as)
        execute_process(
            COMMAND "${compiler}" -print-prog-name=llvm-${tool}
            OUTPUT_VARIABLE prog_path
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        # for FreeBSD
        if(NOT prog_path MATCHES "^/")
            get_filename_component(
                abs_path ${prog_path} ABSOLUTE
                BASE_DIR /usr/local/llvm-devel/bin
            )

            if(EXISTS ${abs_path})
                set(prog_path ${abs_path})
            endif()
        endif()

        if(prog_path MATCHES "^/")
            message("-- Found llvm-${tool}: ${prog_path}")

            if(tool STREQUAL ld)
                set(tool linker)
            elseif(tool STREQUAL as)
                set(tool asm_compiler)
            endif()

            string(TOUPPER ${tool} utool)

            set(CMAKE_${utool} ${prog_path} PARENT_SCOPE)
            set(CMAKE_${utool} ${prog_path} CACHE FILEPATH ${tool} FORCE)

            set(LLVM_${utool}_FOUND TRUE)
            set(LLVM_${utool}_FOUND TRUE PARENT_SCOPE)
            set(LLVM_${utool}_FOUND TRUE CACHE BOOL "found llvm toolchain ${tool}" FORCE)

            set(LLVM_${utool} ${prog_path} PARENT_SCOPE)
            set(LLVM_${utool} ${prog_path} CACHE FILEPATH "llvm toolchain ${tool} path" FORCE)
        else()
            set(LLVM_${utool}_FOUND FALSE PARENT_SCOPE)
            set(LLVM_${utool}_FOUND FALSE CACHE BOOL "did not find llvm toolchain ${tool}" FORCE)
        endif()
    endforeach()

    if(LLVM_AR_FOUND AND LLVM_RANLIB_FOUND AND LLVM_LD_FOUND AND LLVM_NM_FOUND AND LLVM_OBJDUMP_FOUND AND LLVM_AS_FOUND)
        set(LLVM_TOOLCHAIN_FOUND TRUE PARENT_SCOPE)
        set(LLVM_TOOLCHAIN_FOUND TRUE CACHE BOOL "found llvm toolchain ar, ranlib, ld, nm, objdump and as" FORCE)
    else()
        set(LLVM_TOOLCHAIN_FOUND FALSE PARENT_SCOPE)
        set(LLVM_TOOLCHAIN_FOUND FALSE CACHE BOOL "did not find llvm toolchain ar, ranlib, ld, nm, objdump and as" FORCE)
    endif()
endfunction()

if(NOT DEFINED LLVM_TOOLCHAIN_FOUND)
    use_llvm_toolchain()
endif()
