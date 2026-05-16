# SPDX-License-Identifier: GPL-3.0-or-later

set(_inkcut_plugin_out "${CMAKE_BINARY_DIR}/plugins")

function(inkcut_add_device_plugin target_name)
    qt_add_library(${target_name} MODULE ${ARGN})
    target_include_directories(${target_name} PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(${target_name} PRIVATE inkcut_core Qt6::Core)
    set_target_properties(${target_name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${_inkcut_plugin_out}"
        RUNTIME_OUTPUT_DIRECTORY "${_inkcut_plugin_out}"
    )
    install(TARGETS ${target_name}
        LIBRARY DESTINATION lib/inkcut/plugins
        RUNTIME DESTINATION lib/inkcut/plugins
    )
endfunction()

add_subdirectory(noop)
add_subdirectory(generic)
add_subdirectory(grbl)
