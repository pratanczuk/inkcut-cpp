# Build i instalacja (Inkcut C++)

## Zależności

- CMake ≥ 3.16
- Qt 6: Core, Gui, Xml, SerialPort, Widgets
- Kompilator C++17

Na Debian/Ubuntu przykładowo:

```bash
sudo apt install cmake build-essential qt6-base-dev libqt6serialport6-dev
```

## Kompilacja

```bash
cmake -S /ścieżka/do/inkcut -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Binaria: `build/inkcut-cpp`, `build/inkcut-gui`.

## CLI: DXF → program cięcia

Te same opcje co przy `svg-convert` (`--protocol`, `--step`, `--order`, …):

```bash
./build/inkcut-cpp dxf-convert rysunek.dxf --protocol hpgl --out job.hpgl
```

## CLI: `send` z pliku SVG/DXF

Jeśli `FILE` ma przyrostek `.svg` lub `.dxf`, treść jest przetwarzana jak przy `svg-convert` / `dxf-convert` (opcje `--step`, `--protocol`, `--order`, …). Inne pliki wysyłane są bajt-w-bajt.

```bash
./build/inkcut-cpp send --port /dev/ttyUSB0 rysunek.svg --protocol hpgl --step 1
```

## CLI: eksport metadanych zadania (JSON)

```bash
./build/inkcut-cpp job-export projekt.svg --out job.json
```

Plik zawiera `format: inkcut-job`, bbox źródłowej geometrii oraz blok `settings` (re-import przez `importJobDocumentJson` oraz **`job-run`**).

## CLI: ponowne wykonanie z JSON (`job-run`)

Wymaga działającej ścieżki `source_path` z JSON oraz `--port`:

```bash
./build/inkcut-cpp job-export projekt.svg --out job.json
./build/inkcut-cpp job-run job.json --port /dev/ttyUSB0 --baud 9600
```

`--dry-run` wypisuje program na stdout; `--pad` dodaje LF na końcu jak przy `send`.

## Integracja ze środowiskiem pulpitu

Po `cmake --install …` instalowane są wpis menu (`share/applications`) oraz AppStream (`share/metainfo`). Szablon Flatpak: `packaging/flatpak/io.github.inkcut.InkcutCpp.yml`.

**GUI:** dolny dock (Monitor, Sterowanie, Na żywo, Zadania): log TX/RX (hex opcjonalnie), przybliżona pozycja z komunikatów **PU/PD** (RX), nasłuch tylko-odczyt, sterowanie ręczne z ikonami oraz lista dynamicznych wtyczek (np. `build/plugins/*.so`, `INKCUT_PLUGIN_PATH`, `/usr/lib/inkcut/plugins`) jako podpowiedź urządzenia szeregowego.

## Testy

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Test tekstu używa `QGuiApplication`; w środowisku bez X11 CTest ustawia `QT_QPA_PLATFORM=offscreen`.

## Pakiet .deb (CPack)

Po instalacji do prefiksu staging:

```bash
cmake --install build --prefix "$PWD/build/stage"
cd build && cpack -G DEB
```

Gotowy plik `.deb` pojawi się w `build/`. Listę zależności runtime dostosuj w `cmake/InkcutPackaging.cmake`, jeśli dystrybucja używa innych nazw pakietów Qt6.

## Uprawnienia do portu szeregowego

Zobacz `udev/99-inkcut-serial.rules`. Dodaj użytkownika do grupy `dialout`:

```bash
sudo usermod -aG dialout "$USER"
```

(wymaga ponownego logowania.)
