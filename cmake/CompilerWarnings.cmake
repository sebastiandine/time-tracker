add_library(time_tracker_warnings INTERFACE)

set(_TIME_TRACKER_GCC_CLANG_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    # -Wconversion / -Wsign-conversion are intentionally NOT enabled.
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(time_tracker_warnings INTERFACE ${_TIME_TRACKER_GCC_CLANG_FLAGS})
endif()
