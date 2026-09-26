# Each direct child directory containing .cpp files is one standalone demo.
set(DEMO_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/test")
if(CMAKE_CONFIGURATION_TYPES)
    string(APPEND DEMO_OUTPUT_DIR "/$<CONFIG>")
endif()
file(GLOB DEMO_ENTRIES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*")
add_custom_target(test_demos)

foreach(DEMO_DIR IN LISTS DEMO_ENTRIES)
    if(NOT IS_DIRECTORY "${DEMO_DIR}")
        continue()
    endif()
    file(GLOB DEMO_SOURCES CONFIGURE_DEPENDS "${DEMO_DIR}/*.cpp" "${DEMO_DIR}/*.h" "${DEMO_DIR}/*.hpp")
    file(GLOB DEMO_CPP CONFIGURE_DEPENDS "${DEMO_DIR}/*.cpp")
    if(NOT DEMO_CPP)
        continue()
    endif()

    get_filename_component(DEMO_NAME "${DEMO_DIR}" NAME)
    set(DEMO_TARGET "test__${DEMO_NAME}")
    add_executable(${DEMO_TARGET} ${DEMO_SOURCES})
    target_link_libraries(${DEMO_TARGET} PRIVATE ${LIBS})
    set_target_properties(${DEMO_TARGET} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/test"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${DEMO_TARGET}>"
    )
    if(MSVC)
        target_compile_options(${DEMO_TARGET} PRIVATE /MP)
        target_link_options(${DEMO_TARGET} PRIVATE /ignore:4099)
    endif()

    # Keep shader/resource paths separate even when demos use identical names.
    # This target runs on every build, including when only an asset was edited.
    add_custom_target(${DEMO_TARGET}_assets
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEMO_OUTPUT_DIR}"
        VERBATIM
    )
    if(EXISTS "${DEMO_DIR}/assets")
        add_custom_command(TARGET ${DEMO_TARGET}_assets POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${DEMO_DIR}/assets"
                "${DEMO_OUTPUT_DIR}/${DEMO_NAME}"
            VERBATIM
        )
    endif()
    if(WIN32)
        file(GLOB DEMO_DLLS "${CMAKE_SOURCE_DIR}/dlls/*.dll")
        if(DEMO_DLLS)
            add_custom_command(TARGET ${DEMO_TARGET}_assets POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DEMO_DLLS} "${DEMO_OUTPUT_DIR}"
                VERBATIM
            )
        endif()
    endif()
    add_dependencies(${DEMO_TARGET} ${DEMO_TARGET}_assets)
    add_dependencies(test_demos ${DEMO_TARGET})
endforeach()
