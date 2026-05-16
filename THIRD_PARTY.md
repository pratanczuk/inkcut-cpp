# Third-party software

Inkcut C++ links against and bundles the following components. Every component is compatible
with the project license (**GPL-3.0-or-later**); copies of their license texts are available in
the respective upstream repositories.

| Component | Version / source | License | Usage |
|-----------|------------------|---------|-------|
| [Qt 6](https://www.qt.io/) | system (`qt6-base-dev`, `qt6-tools-dev`, `libqt6serialport6-dev`) | LGPL-3.0 / GPL-3.0 / commercial | GUI framework, networking, serial I/O, translations |
| [libpotrace](https://potrace.sourceforge.net/) | system (`libpotrace-dev`) | GPL-2.0-or-later | Bitmap → vector tracing |
| [libdxfrw](https://github.com/LibreCAD/libdxfrw) | FetchContent tag `LC2.2.0` | GPL-2.0-or-later | DXF/DWG file reading |
| [Inkcut](https://github.com/codelv/inkcut) (upstream design) | — | GPL-3.0-or-later | Reference for architecture, filters, protocols, device presets |
| GNU C++ standard library, glibc | system | GPL with system-library exception | Runtime |

## Build-time tools

| Tool | License | Usage |
|------|---------|-------|
| CMake | BSD-3-Clause | Build system |
| Ninja | Apache-2.0 | Build driver |
| GCC / Clang | GPL-3.0 with GCC Runtime Library Exception | Compiler |
| `lrelease` (Qt LinguistTools) | LGPL-3.0 | Translation compilation |
| `dpkg-buildpackage`, `debhelper` | GPL-2.0+ | Debian source packaging |

## Bundled assets

| Asset | License |
|-------|---------|
| Icons in `data/` and `src/ui_icons.cpp` (drawn from primitives) | GPL-3.0-or-later (own work) |
| Translations in `translations/*.ts` | GPL-3.0-or-later (own work) |
| Test fixtures in `tests/fixtures/` | GPL-3.0-or-later (own work) |

## License compatibility

| Combination | Result | Notes |
|-------------|--------|-------|
| GPL-3.0+ project + GPL-2.0+ library (potrace, libdxfrw) | OK — distribute as GPL-3.0+ | Per FSF compatibility matrix |
| GPL-3.0+ project + LGPL-3.0 library (Qt) | OK — link allowed | LGPL permits use from GPL works |
| GPL-3.0+ project + commercial Qt | OK | Independent of upstream license |

If you ship a binary distribution of Inkcut C++ (e.g. `.deb`, AppImage, Flatpak), the GPL
requires that you also offer the **complete corresponding source** — including any modifications
you made. See the [`LICENSE`](LICENSE) for the precise terms (sections 4–6).
