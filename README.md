# Inkcut C++

[![Build .deb packages](https://github.com/pratanczuk/inkcut-cpp-/actions/workflows/release.yml/badge.svg)](https://github.com/pratanczuk/inkcut-cpp-/actions/workflows/release.yml)
[![License: GPL v3+](https://img.shields.io/badge/License-GPLv3%2B-blue.svg)](LICENSE)

> Native C++/Qt6 reimplementation of [Inkcut](https://github.com/codelv/inkcut) — software for
> controlling 2D plotters, vinyl cutters, engravers and CNC machines.

**English** · [Polski](#polski)

---

## Why a C++ port?

Upstream Inkcut is a Python/PyQt5 application. This project is a from-scratch C++17 + Qt 6
reimplementation focused on:

- **Lightweight footprint** — runs comfortably on ARM SBCs (Rockchip RK3128, RPi, etc.)
- **Static binaries** for easier packaging (`.deb`, AppImage, Flatpak)
- **DXF import** via [libdxfrw](https://github.com/LibreCAD/libdxfrw)
- **Bitmap → vector** import via [potrace](https://potrace.sourceforge.net/) with
  `mkbitmap`-style preprocessing (highpass, threshold, despeckle, gap closing, hole filling)
- **GUI tuned for touch / tablet** (large hit targets via UI profile)
- **EN / PL** translations via Qt Linguist

## Features

| Area | Status |
|------|--------|
| SVG import (paths, shapes, gradients, `<use>`, `<text>`, `<image>`) | ✓ |
| DXF import (libdxfrw) | ✓ |
| Bitmap raster import (potrace + `mkbitmap`-like prep) | ✓ |
| HPGL / DMPL / GPGL / G-code / CAMM output | ✓ |
| Job layout (copies, rotation, mirror, weedlines, padding) | ✓ |
| Material roll, force / speed override (HPGL FS/VS) | ✓ |
| Job history, live plot view, monitor & console | ✓ |
| Plugin system for device drivers (`.so`) | ✓ |
| Inkscape integration | ✗ (planned) |

## Install

### From `.deb` (recommended)

Pre-built packages for **Ubuntu 22.04 / 24.04** on **amd64 / arm64 / armhf** are published
under [Releases](https://github.com/pratanczuk/inkcut-cpp-/releases).

```bash
# Ubuntu 22.04 amd64
sudo apt install ./inkcut-cpp_<VERSION>_jammy_amd64.deb

# Ubuntu 22.04 armhf (RK3128, RPi 2/3 with 32-bit OS, etc.)
sudo apt install ./inkcut-cpp_<VERSION>_jammy_armhf.deb

# Ubuntu 24.04 arm64 (RPi 4/5, Jetson, etc.)
sudo apt install ./inkcut-cpp_<VERSION>_noble_arm64.deb
```

Dependencies are resolved automatically by `apt`.

### Build from source

```bash
sudo apt install cmake ninja-build g++ \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools \
    libqt6serialport6-dev libqt6svg6-dev \
    libpotrace-dev

git clone https://github.com/pratanczuk/inkcut-cpp-.git
cd inkcut-cpp-
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

To build a local `.deb`:

```bash
cd build && cpack -G DEB
```

## Run

```bash
inkcut-gui        # graphical application
inkcut-cpp --help # CLI batch converter
```

## Project layout

```
src/                  # C++ source
plugins/              # device driver .so modules (generic, GRBL, no-op)
translations/         # Qt Linguist .ts files (en, pl)
data/                 # .desktop, AppStream metainfo, icons
cmake/                # CMake helpers (deps, CPack)
debian/               # Debian source packaging (for PPA / native .deb)
tests/                # ctest-driven smoke tests
.github/workflows/    # CI + multi-arch release build
```

## Contributing

Pull requests welcome. Please respect the SPDX license headers
(`// SPDX-License-Identifier: GPL-3.0-or-later`) on new files.

For coding style: 4-space indent, C++17, no exceptions, no RTTI dependency, Qt naming conventions
(`PascalCase` types, `camelCase` members with `_` suffix on private fields).

## License

GPL-3.0-or-later. See [LICENSE](LICENSE) and [THIRD_PARTY.md](THIRD_PARTY.md).

## Relationship to upstream Inkcut

This project is an independent C++ reimplementation derived from the architecture and feature
set of the [original Inkcut](https://github.com/codelv/inkcut) by **Jairus Martin** and
contributors. Inkcut is licensed under GPL-3.0-or-later; this work continues under the same
license.

This is **not** an official Inkcut release. Issues with this port should be reported here,
not upstream. Issues that also affect upstream Python Inkcut should be reported to
[codelv/inkcut](https://github.com/codelv/inkcut/issues).

---

<a id="polski"></a>
## Polski

Natywny port C++/Qt6 [Inkcuta](https://github.com/codelv/inkcut) — programu do sterowania
ploterami tnącymi, grawerkami i prostymi maszynami CNC.

### Dlaczego port?

Oryginalny Inkcut to aplikacja Python/PyQt5. Ten projekt to przepisana od podstaw
implementacja C++17 + Qt 6:

- **lekka** — działa na słabych SBC (RK3128, Raspberry Pi)
- **statyczne pakiety** `.deb` (oraz AppImage / Flatpak w planach)
- **import DXF** przez libdxfrw
- **import bitmap** przez potrace z preprocessingiem w stylu `mkbitmap`
- **interfejs dotykowy** (tryb Tablet w ustawieniach)
- tłumaczenia **PL / EN**

### Instalacja

Gotowe pakiety dla **Ubuntu 22.04** (amd64 / arm64 / armhf) i **24.04** są w
[Releases](https://github.com/pratanczuk/inkcut-cpp-/releases).

```bash
sudo apt install ./inkcut-cpp_<WERSJA>_jammy_armhf.deb   # RK3128, RPi 32-bit
```

### Licencja

GPL-3.0-or-later — zobacz [LICENSE](LICENSE) i [THIRD_PARTY.md](THIRD_PARTY.md).
