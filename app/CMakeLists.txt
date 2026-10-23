set(CMAKE_AUTOMOC ON)

add_executable(time_tracker WIN32
    main.cpp
    ProcessEnvironment.cpp
    ProcessFilesystem.cpp
    SystemClock.cpp
)

set_target_properties(time_tracker PROPERTIES OUTPUT_NAME "time-tracker")

target_compile_definitions(time_tracker
    PRIVATE
        "TIME_TRACKER_APP_VERSION=\"${TIME_TRACKER_APP_VERSION}\""
)

target_link_libraries(time_tracker
    PRIVATE
        time_tracker_ui
        time_tracker_core
        sqlite3
        Qt6::Widgets
)

if(TARGET Qt6::EntryPoint)
    target_link_libraries(time_tracker PRIVATE Qt6::EntryPoint)
elseif(TARGET Qt6::EntryPointImplementation)
    target_link_libraries(time_tracker PRIVATE Qt6::EntryPointImplementation)
endif()

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    get_filename_component(_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_rt_dll IN ITEMS libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
        if(EXISTS "${_mingw_bin}/${_rt_dll}")
            add_custom_command(TARGET time_tracker POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_mingw_bin}/${_rt_dll}"
                        "$<TARGET_FILE_DIR:time_tracker>/${_rt_dll}"
                VERBATIM)
        endif()
    endforeach()
endif()

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt6
        HINTS "${Qt6_DIR}/../../../bin" "${Qt6_DIR}/../../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        message(STATUS "[time_tracker] windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
        add_custom_command(TARGET time_tracker POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations "$<TARGET_FILE:time_tracker>"
            VERBATIM)
    endif()
endif()
