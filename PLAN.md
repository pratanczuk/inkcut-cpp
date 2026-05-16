# Port Inkcut na C++ — plan pełnej implementacji

Inkcut upstream (`github.com/inkcut/inkcut`): Python + Enaml/Qt, pluginy, historia jobów itd. Ten projekt realizuje **łańcuch cięcia na Linuxie** w C++/Qt z możliwością stopniowego doganiania funkcji.

---

## Legenda statusów

- **Gotowe** — zaimplementowane i używane w buildzie.
- **W toku** — częściowo lub założenia uproszczone (opis przy funkcji).
- **Backlog** — zaplanowane, niezrobione lub tylko szkielet.

---

## Faza 0 — materiał referencyjny (gotowe)

- Klon upstream w `upstream-ref/` (`.gitignore`).

## Faza 1 — rdzeń „cutter pipe” (gotowe)

- Protokoły: `PlotStreamEncoder` (`protocols.cpp`), szeregowy Qt SerialPort.
- CLI `inkcut-cpp`: `send`, `rect`, `svg-convert`, `dxf-convert`, **`job-export`**, **`job-run`** (JSON → geometria → program → port).

## Faza 2 — geometria i SVG (gotowe / uproszczenia jawne)

**Gotowe:**

- `svg_path.cpp`: `M L H V C S Q T A Z` + względne, łańcuchy `A`.
- `svg_document.cpp`: zagnieżdżone `<svg>`, `<use>` / `symbol`, `viewBox`, `preserveAspectRatio` (meet / slice / none), jednostki CSS/SVG.
- `clip-path` + `mask` (`url(#id)`): `userSpaceOnUse` i **`objectBoundingBox`** (bbox z subtree); przy masce **ostrzeżenie**, że luminancja/alfa nie są rasterowane — przecięcie geometryczne.
- Prezentacja: `fill` / `stroke` / `stroke-width`; **`filter`** i **`url()`** na fill/stroke → ostrzeżenia i uproszczona geometria.
- Tekst → kontur: **`text-anchor`** (start / middle / end), **`tspan`** z listami **`x`/`y`/`dx`/`dy`**, **`xml:space`**, **`textPath`** (`startOffset`, `%`, `side`).
- **`<image>`**: prostokąt obramowania + ostrzeżenia (brak pliku, `http`, `data:`); rozwiązywanie ścieżek względem katalogu dokumentu.
- **`<foreignObject>`**: ostrzeżenie + opcjonalny prostokąt z `x/y/width/height`.

**Backlog długoterminowy (SVG):**

| Priorytet | Temat |
|-----------|--------|
| P1 | Pełna zgodność SVG tekstu (mixed tspans na ścieżce, dziedziczenie czcionek, shaping kompletny) |
| P2 | Gradienty „naprawdę” na konturze (sample stroked outline), filtry SVG |
| P3 | `<image>` z faktycznym rastrem na kontur |

## Faza 3 — Job i urządzenia (gotowe)

- HPGL, DMPL, GPGL, G-code, CAMM GL-1.
- `job_pipeline.cpp`: kolejność → repeat → min-line → blade-offset → flatten → overcut → kod.
- **`job_export.cpp`**: eksport/import dokumentu zadania JSON (`format: inkcut-job`, ustawienia + bbox).

## Plan domknięcia funkcji (100%)

| Obszar | Elementy | Status |
|--------|----------|--------|
| Materiał / weedlines / grafika | Panele jak `MaterialDockItem`, `WeedlinesDockItem`, `GraphicDockItem` | ✅ |
| Warstwy / filtry kolorów | Warstwy + fill/stroke osobno (`LayersDockItem`) | ✅ |
| Dolne docki | Zadania (tabela), Na żywo, Monitor, Konsola*, Sterowanie | ✅ |
| Urządzenie | Menu **Urządzenie → Konfiguracja**, presety, transporty, CUPS `lp` | ✅ |
| Ustawienia | Menu **Ustawienia → Ustawienia zadania**, velocity, repeat, min-line | ✅ |
| Wysyłka | Pauza/anulowanie, live trail, zatwierdzenie + status Approved | ✅ |
| Historia | v2 JSON, przywracanie ustawień z listy | ✅ |
| Pluginy | noop + generic, transform w pipeline | ✅ |

## Faza 4 — GUI (gotowe z funkcjami planowanymi wcześniej jako backlog)

**Gotowe:** Qt Widgets — docki (Materiał / Linie tnące / Warstwy | Grafika | Konsola / Sterowanie / Monitor / Zadania / Na żywo). Materiał, weedlines, skala/obrót/lustro/kopie; warstwy Inkscape + filtry kolorów; presety urządzeń; transporty serial/plik/drukarka; zatwierdzenie przed cięciem; pauza/anulowanie; live plot; historia jobów v2; eksport programu/JSON; monitor TX/RX; sterowanie ręczne z ikonami.

**Wtyczki:** rozszerzone `DevicePlugin` (`transformPath`, `providedPresetIds`, …), loader, `plugins/noop`, `plugins/generic`; CMake **`INKCUT_BUILD_PLUGINS`** (domyślnie ON).

## Faza 5 — dystrybucja i środowisko (gotowe; wyjątek: AppImage)

| Zadanie | Status |
|---------|--------|
| Szablon **CPack** `.deb` | `cmake/InkcutPackaging.cmake` |
| Reguły **udev** | `udev/99-inkcut-serial.rules` |
| Dokumentacja build/install | `docs/BUILD_INSTALL.md` |
| **Ikona `.desktop`, AppStream** | `data/io.github.inkcut.InkcutCpp.desktop`, `data/io.github.inkcut.InkcutCpp.metainfo.xml` |
| **Flatpak** | Szablon `packaging/flatpak/io.github.inkcut.InkcutCpp.yml` |
| **AppImage** | Celowo odłożony — nie w zakresie |

## Faza 6 — Testy i jakość (gotowe / rozszerzalne)

| Zadanie | Status |
|---------|--------|
| Smoke loadera SVG | `tests/svg_loader_smoke.cpp` |
| Smoke DXF | `tests/dxf_smoke.cpp` |
| CI GitHub Actions | `.github/workflows/ci.yml` |
| **`svg_path` smoke** | `tests/svg_path_parse.cpp` |
| **HPGL encoder smoke** | `tests/protocol_hpgl_smoke.cpp` |
| **JSON job round-trip** | `tests/job_json_roundtrip.cpp` |
| **Powtórzenie zadania (JSON → geometria → program)** | `tests/job_replay_smoke.cpp` |

## Faza 7 — Formaty dodatkowe (gotowe)

- **DXF ASCII i binarny** → `QPainterPath` (`dxf_document.cpp`): LINE, ARC, CIRCLE, LWPOLYLINE, POLYLINE/VERTEX z bulge, SPLINE (fit/control), ELLIPSE, **INSERT / bloki**.
- CLI: **`dxf-convert`**, **`job-export`**, **`job-run`**; GUI: otwieranie `.dxf`.

## Faza 8 — Parity z upstream (backlog długoterminowy)

- Rozbudowa ekosystemu pluginów (konfiguracja jak upstream, dodatkowe backendy).
- Rozszerzenia Inkscape jako osobny proces/skrypt — nie w rdzeniu.

---

## Kolejność realizacji (podsumowanie)

1. **Faza 5 + 6 (minimalny zestaw):** CPack, udev, `docs/BUILD_INSTALL.md`, testy smoke — *wykonane / rozszerzone*.
2. **SVG:** lista współrzędnych tekstu, `xml:space`, **textPath**, **text-anchor**, **image**/ostrzeżenia — *wykonane z założonymi uproszczeniami*.
3. **Faza 7:** DXF bloki/INSERT, CLI/GUI, JSON job — *wykonane*.
4. **`send`** z automatyczną konwersją `.svg`/`.dxf` — *wykonane*.
5. **`job-run`**, GUI historia/postęp/RX, pluginy `.so`, dokumentacja — *wykonane*. **AppImage** — celowo nie realizowane teraz.

**Licencja:** GPL v3 (jak ekosystem Inkcut).
