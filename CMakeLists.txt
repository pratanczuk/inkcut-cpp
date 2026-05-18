cmake_minimum_required(VERSION 3.16)
project(inkcut-cpp VERSION 0.4.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Static libraries (inkcut_core, fetched dxfrw) end up linked into shared
# modules (device plugins) and the GUI. On amd64 short-PIC relocations
# happen to work, but on arm64/armhf the linker refuses to put non-PIC
# objects into a shared object. Force position-independent code globally.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CTest)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Xml SerialPort Widgets LinguistTools Network)

option(INKCUT_BUILD_PLUGINS "Build optional device plugin modules (.so)" ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/OptionalLibraries.cmake")

set(INKCUT_CORE_SOURCES
    src/device_plugin.cpp
    src/device_presets.cpp
    src/job_layout.cpp
    src/job_stack.cpp
    src/job_history.cpp
    src/svg_layers.cpp
    src/path_utils.cpp
    src/svg_path.cpp
    src/svg_document.cpp
    src/dxf_document.cpp
    src/bitmap_trace.cpp
    src/dxf_to_svg.cpp
    src/protocols.cpp
    src/grbl_comm.cpp
    src/filters.cpp
    src/ordering.cpp
    src/job_pipeline.cpp
    src/job_export.cpp
)

add_library(inkcut_core STATIC ${INKCUT_CORE_SOURCES})
target_include_directories(inkcut_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(inkcut_core PUBLIC Qt6::Core Qt6::Gui Qt6::Xml)
if(INKCUT_HAVE_POTRACE)
    target_compile_definitions(inkcut_core PUBLIC INKCUT_HAVE_POTRACE=1)
    target_include_directories(inkcut_core PRIVATE ${POTRACE_INCLUDE_DIR})
    target_link_libraries(inkcut_core PRIVATE ${POTRACE_LIBRARY})
endif()
if(INKCUT_HAVE_DXFRW)
    target_compile_definitions(inkcut_core PUBLIC INKCUT_HAVE_DXFRW=1)
    target_link_libraries(inkcut_core PUBLIC dxfrw)
endif()

if(INKCUT_BUILD_PLUGINS)
    add_subdirectory(plugins)
endif()

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/data/io.github.inkcut.InkcutCpp.desktop"
    DESTINATION share/applications
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/data/io.github.inkcut.InkcutCpp.metainfo.xml"
    DESTINATION share/metainfo
)

qt_add_executable(inkcut-cpp
    src/main.cpp
    src/serial_sender.cpp
)
target_link_libraries(inkcut-cpp PRIVATE inkcut_core Qt6::SerialPort)

qt_add_executable(inkcut-gui
    src/gui_main.cpp
    src/mainwindow.cpp
    src/live_plot_view.cpp
    src/preview_plot_view.cpp
    src/device_setup_dialog.cpp
    src/app_settings.cpp
    src/app_units.cpp
    src/app_theme.cpp
    src/i18n.cpp
    src/ui_profile.cpp
    src/settings_dialog.cpp
    src/bitmap_trace_dialog.cpp
    src/ui_icons.cpp
    src/plugin_loader.cpp
    src/plot_sender.cpp
    src/plot_live_position.cpp
    src/plot_transport.cpp
    src/serial_sender.cpp
)
target_link_libraries(inkcut-gui PRIVATE inkcut_core Qt6::Widgets Qt6::SerialPort Qt6::Network)

set(INKCUT_TS_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/inkcut_en.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/inkcut_pl.ts"
)
qt_add_translations(inkcut-gui
    TS_FILES ${INKCUT_TS_FILES}
    # trInk() wraps QCoreApplication::translate("Inkcut", …) — teach lupdate.
    LUPDATE_OPTIONS -tr-function-alias translate+=trInk
)

install(TARGETS inkcut-cpp inkcut-gui RUNTIME DESTINATION bin)
install(FILES ${INKCUT_TS_FILES} DESTINATION share/inkcut/translations OPTIONAL)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/InkcutPackaging.cmake")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
