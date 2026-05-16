# SPDX-License-Identifier: GPL-3.0-or-later
# CPack: pakiet .deb (cmake --install … staging && cd build && cpack -G DEB)

set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "Inkcut C++ contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Drive vinyl cutters, plotters and engravers from SVG/DXF/bitmap input")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/pratanczuk/inkcut-cpp")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
endif()

set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER
    "pratanczuk <pratanczuk@users.noreply.github.com>")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
# Wykryj architekturę z dpkg (amd64 / arm64 / armhf / i386 / ...).
find_program(DPKG_PROGRAM dpkg)
if(DPKG_PROGRAM)
    execute_process(
        COMMAND ${DPKG_PROGRAM} --print-architecture
        OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()
# Ubuntu 22.04 (libqt6*6) i 24.04 (libqt6*6t64) — pakiety alternatywne.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6, libqt6core6 | libqt6core6t64, libqt6gui6 | libqt6gui6t64, libqt6widgets6 | libqt6widgets6t64, libqt6serialport6 | libqt6serialport6t64, libqt6xml6 | libqt6xml6t64"
)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "libqt6svg6")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)
