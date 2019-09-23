function(use_gcc_toolchain)
    if(CMAKE_COMPILER_IS_GNUCXX)
        set(compiler ${CMAKE_CXX_COMPILER})
    elseif(CMAKE_COMPILER_IS_GNUC)
        set(compiler ${CMAKE_C_COMPILER})
    else()
        return()
    endif()

    # first try appending -util to basename of compiler
    get_filename_component(compiler_extension ${compiler} EXT)
    get_filename_component(compiler_basename  ${compiler} NAME_WE)
    get_filename_component(compiler_dirname   ${compiler} DIRECTORY)

    # and to 'gcc' as well
    find_program(gcc NAMES gcc gcc.exe GCC.EXE HINTS ${compiler_dirname})

    get_filename_component(gcc_extension ${gcc} EXT)
    get_filename_component(gcc_basename  ${gcc} NAME_WE)
    get_filename_component(gcc_dirname   ${gcc} DIRECTORY)

    foreach(tool ar nm ranlib)
        string(TOUPPER ${tool} utool)

        set(path ${compiler_dirname}/${compiler_basename}-${tool}${compiler_extension})

        if(NOT EXISTS ${path})
            set(path ${gcc_dirname}/${gcc_basename}-${tool}${gcc_extension})
        endif()

        if(NOT EXISTS ${path})
            unset(path)
            unset(path CACHE)
            find_program(path NAMES ${compiler_basename}-${tool}${compiler_extension} ${gcc_basename}-${tool}${gcc_extension} HINTS ${compiler_dirname} ${gcc_dirname})
        endif()

        if(EXISTS ${path})
            message("-- Found gcc-${tool}: ${path}")

            set(CMAKE_${utool} ${path} PARENT_SCOPE)
            set(CMAKE_${utool} ${path} CACHE FILEPATH ${tool} FORCE)

            set(GCC_${utool}_FOUND TRUE)
            set(GCC_${utool}_FOUND TRUE PARENT_SCOPE)
            set(GCC_${utool}_FOUND TRUE CACHE BOOL "found gcc toolchain ${tool}" FORCE)

            set(GCC_${utool} ${path} PARENT_SCOPE)
            set(GCC_${utool} ${path} CACHE FILEPATH "gcc toolchain ${tool} path" FORCE)
        else()
            set(GCC_${utool}_FOUND FALSE PARENT_SCOPE)
            set(GCC_${utool}_FOUND FALSE CACHE BOOL "did not find gcc toolchain ${tool}" FORCE)
        endif()

        unset(path CACHE)
    endforeach()

    if(GCC_AR_FOUND AND GCC_NM_FOUND AND GCC_RANLIB_FOUND)
        set(GCC_TOOLCHAIN_FOUND TRUE PARENT_SCOPE)
        set(GCC_TOOLCHAIN_FOUND TRUE CACHE BOOL "found gcc toolchain ar, nm and ranlib" FORCE)
    else()
        set(GCC_TOOLCHAIN_FOUND FALSE PARENT_SCOPE)
        set(GCC_TOOLCHAIN_FOUND FALSE CACHE BOOL "did not find gcc toolchain ar, nm and ranlib" FORCE)
    endif()
endfunction()

if(NOT DEFINED GCC_TOOLCHAIN_FOUND)
    use_gcc_toolchain()
endif()
