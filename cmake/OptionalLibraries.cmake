# SPDX-License-Identifier: GPL-3.0-or-later

include(FetchContent)

option(INKCUT_USE_POTRACE "Trace bitmaps with libpotrace" ON)
option(INKCUT_USE_DXFRW "Convert DXF via libdxfrw to SVG on open" ON)

set(INKCUT_HAVE_POTRACE 0)
set(INKCUT_HAVE_DXFRW 0)

if(INKCUT_USE_POTRACE)
    # find_library already searches /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
    # (multiarch) on its own; PATHS is only a hint.
    find_path(POTRACE_INCLUDE_DIR NAMES potracelib.h potrace.h
              PATHS /usr/include /usr/local/include /usr/include/potrace)
    find_library(POTRACE_LIBRARY NAMES potrace
                 PATHS /usr/lib /usr/local/lib)
    if(NOT POTRACE_LIBRARY AND CMAKE_LIBRARY_ARCHITECTURE)
        foreach(_libdir
                "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
                /usr/lib /usr/local/lib)
            if(EXISTS "${_libdir}/libpotrace.so.0")
                set(POTRACE_LIBRARY "${_libdir}/libpotrace.so.0")
                break()
            endif()
        endforeach()
    endif()

    # Runtime (libpotrace0) is often installed without -dev headers.
    if(POTRACE_LIBRARY AND NOT POTRACE_INCLUDE_DIR)
        set(_potrace_dev_root "${CMAKE_BINARY_DIR}/potrace-dev")
        set(_potrace_hdr "${_potrace_dev_root}/usr/include/potracelib.h")
        if(NOT EXISTS "${_potrace_hdr}")
            find_program(APT_GET_EXECUTABLE apt-get)
            if(APT_GET_EXECUTABLE)
                execute_process(
                    COMMAND ${APT_GET_EXECUTABLE} download libpotrace-dev
                    WORKING_DIRECTORY "${_potrace_dev_root}"
                    RESULT_VARIABLE _potrace_dl
                    OUTPUT_QUIET ERROR_QUIET)
                if(_potrace_dl EQUAL 0)
                    file(GLOB _potrace_debs "${_potrace_dev_root}/libpotrace-dev_*.deb")
                    if(_potrace_debs)
                        execute_process(
                            COMMAND dpkg-deb -x "${_potrace_debs}" "${_potrace_dev_root}"
                            RESULT_VARIABLE _potrace_x)
                    endif()
                endif()
            endif()
        endif()
        if(EXISTS "${_potrace_hdr}")
            set(POTRACE_INCLUDE_DIR "${_potrace_dev_root}/usr/include")
        endif()
    endif()

    if(POTRACE_INCLUDE_DIR AND POTRACE_LIBRARY)
        set(INKCUT_HAVE_POTRACE 1)
        message(STATUS "libpotrace: ${POTRACE_LIBRARY} (includes: ${POTRACE_INCLUDE_DIR})")
    else()
        message(STATUS "libpotrace not found — bitmap tracing disabled (install libpotrace-dev)")
    endif()
endif()

if(INKCUT_USE_DXFRW)
    FetchContent_Declare(
        libdxfrw
        GIT_REPOSITORY https://github.com/LibreCAD/libdxfrw.git
        GIT_TAG LC2.2.0
        GIT_SHALLOW TRUE
    )
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(libdxfrw)
    if(TARGET dxfrw)
        set(INKCUT_HAVE_DXFRW 1)
        target_include_directories(dxfrw PUBLIC
            $<BUILD_INTERFACE:${libdxfrw_SOURCE_DIR}/src>)
        # libdxfrw's CMakeLists.txt enables -Werror -Wall -Wextra -pedantic
        # at directory scope. New GCC/Clang releases routinely add warnings
        # that turn into hard build failures (especially on non-x86 archs).
        # Append -Wno-error at target level so it overrides the directory
        # flag while keeping all warnings visible.
        if(NOT MSVC)
            target_compile_options(dxfrw PRIVATE -Wno-error)
        endif()
        message(STATUS "libdxfrw: bundled via FetchContent")
    endif()
endif()
