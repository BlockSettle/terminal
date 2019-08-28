if(CMAKE_C_COMPILER_ID STREQUAL GNU OR CMAKE_C_COMPILER_ID STREQUAL Clang)
   if(CMAKE_BUILD_TYPE STREQUAL Debug)
      add_compile_options(-Wall -Wextra)

      # extra for clang
      if(CMAKE_C_COMPILER_ID STREQUAL Clang)
	 add_compile_options(-Weverything)
      endif()
   else()
      add_compile_options(-w) # turn off all warnings
   endif()
elseif(MSVC)
   if(NOT CMAKE_BUILD_TYPE STREQUAL Debug)
      add_compile_options(/W0) # turn off all warnings
   endif()
endif()
