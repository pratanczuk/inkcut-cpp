# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(inkcut_test_svg svg_loader_smoke.cpp)
target_link_libraries(inkcut_test_svg PRIVATE inkcut_core Qt6::Gui)

add_executable(inkcut_test_dxf dxf_smoke.cpp)
target_link_libraries(inkcut_test_dxf PRIVATE inkcut_core Qt6::Gui)

add_executable(inkcut_test_job_json job_json_roundtrip.cpp)
target_link_libraries(inkcut_test_job_json PRIVATE inkcut_core Qt6::Core)

add_executable(inkcut_test_job_replay job_replay_smoke.cpp)
target_link_libraries(inkcut_test_job_replay PRIVATE inkcut_core Qt6::Gui)

add_executable(inkcut_test_svg_path svg_path_parse.cpp)
target_link_libraries(inkcut_test_svg_path PRIVATE inkcut_core Qt6::Gui)

add_executable(inkcut_test_protocol protocol_hpgl_smoke.cpp)
target_link_libraries(inkcut_test_protocol PRIVATE inkcut_core Qt6::Core)

add_test(NAME svg_loader_smoke COMMAND inkcut_test_svg)
set_tests_properties(svg_loader_smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_test(NAME dxf_loader_smoke COMMAND inkcut_test_dxf)
set_tests_properties(dxf_loader_smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_test(NAME job_json_roundtrip COMMAND inkcut_test_job_json)

add_test(NAME job_replay_smoke COMMAND inkcut_test_job_replay)
set_tests_properties(job_replay_smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_test(NAME svg_path_parse COMMAND inkcut_test_svg_path)
set_tests_properties(svg_path_parse PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_test(NAME protocol_hpgl_smoke COMMAND inkcut_test_protocol)

if(INKCUT_HAVE_POTRACE)
    add_executable(inkcut_test_bitmap bitmap_trace_smoke.cpp)
    target_link_libraries(inkcut_test_bitmap PRIVATE inkcut_core Qt6::Gui)
    target_compile_definitions(inkcut_test_bitmap PRIVATE
        INKCUT_WHEEL_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/wheel.pbm")
    add_test(NAME bitmap_trace_smoke COMMAND inkcut_test_bitmap)
    set_tests_properties(bitmap_trace_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
