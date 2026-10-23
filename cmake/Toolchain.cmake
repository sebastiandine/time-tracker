# Preferred compiler is Clang. On Windows fall back to MinGW-w64 g++.
# MSVC is not supported. Run BEFORE project().

if(DEFINED CMAKE_CXX_COMPILER)
    return()
endif()

find_program(_TC_CLANG NAMES clang)
find_program(_TC_CLANGXX NAMES clang++)

if(_TC_CLANG AND _TC_CLANGXX)
    set(CMAKE_C_COMPILER   "${_TC_CLANG}"   CACHE FILEPATH "C compiler"   FORCE)
    set(CMAKE_CXX_COMPILER "${_TC_CLANGXX}" CACHE FILEPATH "C++ compiler" FORCE)
    message(STATUS "[time_tracker] Toolchain: Clang at ${_TC_CLANGXX}")
    return()
endif()

if(WIN32)
    find_program(_TC_GCC NAMES x86_64-w64-mingw32-gcc gcc)
    find_program(_TC_GXX NAMES x86_64-w64-mingw32-g++ g++)
    if(_TC_GCC AND _TC_GXX)
        set(CMAKE_C_COMPILER   "${_TC_GCC}" CACHE FILEPATH "C compiler"   FORCE)
        set(CMAKE_CXX_COMPILER "${_TC_GXX}" CACHE FILEPATH "C++ compiler" FORCE)
        message(STATUS "[time_tracker] Toolchain: MinGW-w64 GCC at ${_TC_GXX}")
        return()
    endif()
    message(WARNING
        "[time_tracker] Neither Clang nor MinGW-w64 GCC was found in PATH. "
        "Install LLVM (preferred) or MSYS2 mingw-w64-ucrt-x86_64-gcc and re-run."
    )
else()
    find_program(_TC_GCC NAMES gcc cc)
    find_program(_TC_GXX NAMES g++ c++)
    if(_TC_GCC AND _TC_GXX)
        set(CMAKE_C_COMPILER   "${_TC_GCC}" CACHE FILEPATH "C compiler"   FORCE)
        set(CMAKE_CXX_COMPILER "${_TC_GXX}" CACHE FILEPATH "C++ compiler" FORCE)
        message(STATUS "[time_tracker] Toolchain: GCC at ${_TC_GXX}")
        return()
    endif()
    message(WARNING "[time_tracker] Neither Clang nor GCC was found in PATH.")
endif()
