#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Add English translations for all Inkcut UI strings to inkcut_en.ts."""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EN_TS = ROOT / "translations" / "inkcut_en.ts"

# Polish source -> English translation (new strings not yet in .ts)
NEW_TRANSLATIONS: dict[str, str] = {
    # settings_dialog.cpp
    "Limit optymalizatora": "Optimizer timeout",
    "Siatka": "Grid",
    "Motyw": "Theme",
    "Profil interfejsu": "UI profile",
    "Ustawienia — Inkcut": "Settings — Inkcut",
    "Ustawienia": "Settings",
    "Zadanie": "Job",
    "System": "System",
    "Sterowanie": "Control",
    "np. PG; lub G-code podawania": "e.g. PG; or feed G-code",
    "np. PG; lub G-code cofania": "e.g. PG; or retract G-code",
    "Komendy wysyłane na port po kliknięciu Załaduj / Wyładuj w zakładce Sterowanie. "
    "Użyj \\n na końcu linii, jeśli ploter tego wymaga (np. PG;\\n). "
    "Wymagane połączenie z ploterem.": (
        "Commands sent to the port when you click Load / Unload in the Control tab. "
        "Use \\n at the end of a line if the plotter requires it (e.g. PG;\\n). "
        "Plotter connection required."
    ),
    # device_setup_dialog.cpp
    "+ Dodaj": "+ Add",
    "Nazwa": "Name",
    "Sterownik": "Driver",
    "Producent": "Manufacturer",
    "Model": "Model",
    "Lustro X": "Mirror X",
    "Lustro Y": "Mirror Y",
    "Typ": "Type",
    "Port": "Port",
    "Prędkość (baud)": "Baud rate",
    "Bity danych": "Data bits",
    "Parzystość": "Parity",
    "Bity stopu": "Stop bits",
    "Kontrola przepływu": "Flow control",
    "Wtyczka": "Plugin",
    "Brak": "None",
    "Parzysta": "Even",
    "Nieparzysta": "Odd",
    "Przed połączeniem": "Before connect",
    "Przed połączeniem (np. reset\\n)": "Before connect (e.g. reset\\n)",
    "Po połączeniu": "After connect",
    "Po połączeniu (np. G21\\n)": "After connect (e.g. G21\\n)",
    "Komendy": "Commands",
    "Przed zadaniem": "Before job",
    "Przed cięciem": "Before cutting",
    "Po zadaniu": "After job",
    "Wbudowane komendy start/stop": "Built-in start/stop commands",
    "Tryb DMPL": "DMPL mode",
    "Skala plotera": "Plotter scale",
    "G-code / GRBL": "G-code / GRBL",
    "Dialekt": "Dialect",
    "Precyzja": "Precision",
    "Offset ostrza": "Blade offset",
    "Offset": "Offset",
    "Min. linia": "Min. line",
    "Min. skok (PU)": "Min. jump (PU)",
    " (kopia)": " (copy)",
    "Kopiuj": "Copy",
    "Program (*.hpgl *.plt *.prn);;Wszystkie (*)": (
        "Plotter program (*.hpgl *.plt *.prn);;All (*)"
    ),
    "Protokół jest ustawiany przez wybrany sterownik. "
    "Zaznacz „Własne”, aby zmienić ręcznie.": (
        "Protocol is forced by the selected driver. "
        "Check “Custom” to change it manually."
    ),
    "Solenoid PWM (M3 S…)": "Solenoid PWM (M3 S…)",
    "Prędkość (cm/s)": "Speed (cm/s)",
    "Prędkość ruchu narzędzia. Dla HPGL/DMPL/GPGL/CAMM Inkcut wysyła "
    "komendę VS<n>; (typowo cm/s, zakres ~1–110 zależnie od plotera). "
    "Dla G-code parametr jest ignorowany — użyj pola Feed lub $110/$111 w GRBL.": (
        "Tool travel speed. For HPGL/DMPL/GPGL/CAMM Inkcut sends VS<n>; "
        "(typically cm/s, range ~1–110 depending on plotter). "
        "For G-code this is ignored — use the Feed field or GRBL’s $110/$111."
    ),
    "Feed cięcia (G1)": "Cutting feed (G1)",
    "Feed przejazdu (G0)": "Travel feed (G0)",
    "Posuw cięcia. Dodawany do każdego G1 jako F<n>. "
    "0 = nie wysyłaj — wtedy obowiązują $110/$111 w GRBL lub wcześniej "
    "ustawione F. Typowo 600–1500 mm/min dla cięcia folii.": (
        "Cutting feedrate. Appended to every G1 as F<n>. "
        "0 = don’t send — GRBL’s $110/$111 or any previously set F applies. "
        "Typically 600–1500 mm/min for vinyl."
    ),
    "Posuw przejazdów (G0). 0 = nie dodawaj F — GRBL używa wtedy "
    "$110/$111. Zwykle G0 i tak ignoruje F w GRBL.": (
        "Rapid-move feedrate (G0). 0 = don’t append F — GRBL uses "
        "$110/$111. GRBL ignores F on G0 anyway."
    ),
    "PWM pióro w górze (M5 jeśli 0)": "PWM pen up (M5 if 0)",
    "PWM nacisk (pióro w dole)": "PWM pressure (pen down)",
    "PWM maks. ($30 w GRBL)": "PWM max ($30 in GRBL)",
    "Lift G-code": "Lift G-code",
    "Lower G-code": "Lower G-code",
    "Powtórzenia pojedynczej warstwy (np. twardy materiał) ustaw w głównym "
    "oknie: lewy panel → zakładka Warstwy.": (
        "Per-layer repeats (e.g. hard material) are set in the main window: "
        "left panel → Layers tab."
    ),
    "Filtry": "Filters",
    "(brak)": "(none)",
    "Port szeregowy": "Serial port",
    "Sterowanie ręczne działa tylko dla portu szeregowego.": (
        "Manual controls are available only for serial transport."
    ),
    "Nie można połączyć TCP %1:%2": "Cannot connect TCP %1:%2",
    "Błąd zapisu na TCP.": "TCP write error.",
    "Zapis TCP nie powiódł się.": "TCP write failed.",
    "Timeout zapisu TCP.": "TCP write timeout.",
    "Zapis do pliku": "Save to file",
    "Drukarka (CUPS)": "Printer (CUPS)",
    "Drukarka CUPS": "CUPS printer",
    # mainwindow.cpp — menus
    "Ostatnie pliki": "Recent files",
    "Zapisz program plotera…": "Save plotter program…",
    "Eksport zadania (JSON)…": "Export job (JSON)…",
    "Ustawienia…": "Settings…",
    "O programie…": "About…",
    "Port C++/Qt aplikacji Inkcut.\nWersja 0.2": "C++/Qt port of Inkcut.\nVersion 0.2",
    # mainwindow — material tab
    "Czerwona przerywana — płaszczyzna urządzenia (x-y).\n"
    "Czarna ciągła — materiał.\n"
    "Czarna przerywana — dostępny obszar (po marginesach).\n"
    "Niebieski — ruch jałowy (move); szary — cięcie (cut).": (
        "Red dashed — device plane (x-y).\n"
        "Black solid — material.\n"
        "Black dashed — usable area (after margins).\n"
        "Blue — travel (move); gray — cut."
    ),
    "Obszar plotowania": "Plot area",
    "Szer.": "W",
    "Wys.": "H",
    "Marginesy plotowania": "Plot margins",
    "Lewy": "Left",
    "Prawy": "Right",
    "Dolny": "Bottom",
    "Podaj po": "Feed after",
    "Własna siła / prędkość cięcia": "Custom cut force / speed",
    "Siła (FS)": "Force (FS)",
    "Prędkość cięcia (VS). Typowo cm/s, zakres ~1–110 zależnie od plotera.": (
        "Cutting speed (VS). Usually cm/s; range ~1–110 depends on plotter."
    ),
    "Posuw cięcia (G1). 0 = nie wysyłaj F — obowiązują $110/$111 w GRBL.": (
        "Cutting feed (G1). 0 = don’t send F — GRBL’s $110/$111 apply."
    ),
    "Posuw przejazdów (G0). 0 = nie dodawaj F.": "Travel feed (G0). 0 = don’t append F.",
    # graphic tab
    "Brak wczytanego pliku.": "No file loaded.",
    "Rozmiar grafiki": "Graphic size",
    "Skala X": "Scale X",
    "Skala Y": "Scale Y",
    "Zablokuj proporcje": "Lock aspect ratio",
    "Kopie grafiki": "Graphic copies",
    "Dopasuj do obszaru (auto scale)": "Fit to area (auto scale)",
    "Lustrzane odbicie": "Mirror",
    "Pozycja na materiale": "Position on material",
    "Zaznacz warstwę/kolor i ustaw × (przyciski −/+). "
    "Bez warstw Inkscape: „Cały dokument”.": (
        "Select a layer/color and set × ( −/+ buttons). "
        "Without Inkscape layers: “Whole document”."
    ),
    "Kolory obrysu": "Stroke colors",
    "Kolumna": "Column",
    "Margines weedline (plot)": "Weedline margin (plot)",
    "Margines weedline (kopia)": "Weedline margin (copy)",
    # job history
    "Data": "Date",
    "Dokument": "Document",
    "Liczba": "Count",
    "Czas": "Time",
    "Status": "Status",
    "Kopie": "Copies",
    "Rozmiar": "Size",
    # monitor / control
    "Przerwij": "Abort",
    "Loguj TX/RX": "Log TX/RX",
    "Hex": "Hex",
    "np. PG; lub G0 X10 Y10": "e.g. PG; or G0 X10 Y10",
    "Sterowanie": "Control",
    "Status": "Status",
    "Zapisz": "Save",
    "Zapisano.": "Saved.",
    # bitmap_trace / pipeline errors
    "Nie można przygotować obrazu do śledzenia.": "Cannot prepare image for tracing.",
    "Brak pamięci na bitmapę potrace.": "Out of memory for potrace bitmap.",
    "potrace_param_default nie powiódł się.": "potrace_param_default failed.",
    "Nie można przygotować obrazu.": "Cannot prepare image.",
    "potrace_trace nie powiódł się.": "potrace_trace failed.",
    "Potrace nie znalazł kształtów do wycięcia.": "Potrace found no shapes to cut.",
    "Brak geometrii w włączonych warstwach/kolorach.": "No geometry in enabled layers/colors.",
    "Nie można utworzyć pliku tymczasowego dla DXF.": "Cannot create temporary file for DXF.",
    "Zapis DXF tymczasowego nie powiódł się.": "Failed to write temporary DXF.",
    "libdxfrw: odczyt DXF nie powiódł się.": "libdxfrw: failed to read DXF.",
    # svg warnings
    "<image>: brak lub zerowy width/height — pominięto.": "<image>: missing or zero width/height — skipped.",
    "<image>: URI sieciowe nie są pobierane (prostokąt obramowania).": (
        "<image>: network URIs are not fetched (bounding rectangle used)."
    ),
    "<image>: brak libpotrace — użyto prostokąta obramowania.": (
        "<image>: libpotrace missing — bounding rectangle used."
    ),
    "<foreignObject>: zawartość niestandardowa pominięta (opcjonalne obramowanie).": (
        "<foreignObject>: custom content skipped (optional bounding box)."
    ),
    "Filtry SVG (filter=%1) — geometria bez rozmycia/efektów.": (
        "SVG filters (filter=%1) — geometry without blur/effects."
    ),
    "Filtry SVG są ignorowane — geometria bez efektów.": "SVG filters ignored — geometry without effects.",
    "Wypełnienie url(#…) — nieznany odniesienie; użyto obrysu.": "Fill url(#…) — unknown reference; stroke used.",
    "Obrys url(#…) — nieznane odniesienie; użyto obrysu.": "Stroke url(#…) — unknown reference; stroke used.",
    # plot / transport
    "Brak danych lub urządzenia.": "No data or device.",
    "Błąd zapisu na port.": "Error writing to port.",
    "JSON: błąd składni ustawień.": "JSON: settings syntax error.",
    "JSON: błąd składni.": "JSON: syntax error.",
    "%1: nie jest wtyczką DevicePlugin": "%1: is not a DevicePlugin",
    "Podaj nazwę drukarki CUPS (pole „Drukarka”).": "Enter CUPS printer name (“Printer” field).",
    "Nie można uruchomić „lp” — zainstaluj CUPS lub zapisz plik ręcznie.": (
        "Cannot run “lp” — install CUPS or save the file manually."
    ),
    "lp zakończył się kodem %1": "lp exited with code %1",
    "Brak ścieżki pliku wyjściowego.": "No output file path.",
    "Nie można zapisać: %1": "Cannot save: %1",
    "Nie można zapisać pliku dla drukarki: %1": "Cannot save file for printer: %1",
    "Zapisano %1 — lp nie powiódł się: %2": "Saved %1 — lp failed: %2",
    "Zapis na port nie powiódł się.": "Failed to write to port.",
    "Zapis niekompletny.": "Incomplete write.",
    # dxf
    "DXF: zbyt głębokie INSERT/bloki.": "DXF: INSERT/blocks nesting too deep.",
    "DXF: pusty plik.": "DXF: empty file.",
    "DXF: brak geometrii w ENTITIES (obsługa: LINE, ARC, CIRCLE, "
    "LWPOLYLINE, POLYLINE, SPLINE, ELLIPSE, INSERT z blokami).": (
        "DXF: no geometry in ENTITIES (supported: LINE, ARC, CIRCLE, "
        "LWPOLYLINE, POLYLINE, SPLINE, ELLIPSE, INSERT with blocks)."
    ),
    "DXF: nie można zdekodować par grup (ASCII lub binarny). Zapisz jako "
    "ASCII DXF lub sprawdź integrę pliku.": (
        "DXF: cannot decode group pairs (ASCII or binary). Save as ASCII DXF "
        "or check file integrity."
    ),
    # CLI (main.cpp) — optional but included
    "Brak wartości dla --out": "Missing value for --out",
    "Podano więcej niż jeden plik wejściowy.": "More than one input file specified.",
    "Brak pliku wejściowego.": "No input file.",
    "Brak wartości dla --port": "Missing value for --port",
    "Brak wartości dla --baud": "Missing value for --baud",
    "Podano więcej niż jeden plik.": "More than one file specified.",
    "send: wymagane --port oraz ścieżka pliku.": "send: --port and file path required.",
    "Podano więcej niż jeden plik JSON.": "More than one JSON file specified.",
    "Filtry kolorów dostępne tylko dla SVG.": "Color filters available for SVG only.",
    "[control] połączono %1\n": "[control] connected %1\n",
    "Brak danych — wyślij zadanie G-code, aby odczytać $$": (
        "No data — send a G-code job to read $$"
    ),
    "Brak wybranych kluczy w odpowiedzi $$.": "No selected keys in $$ response.",
}


def xml_escape(text: str) -> str:
    return (
        text.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


def update_en_ts(extra: dict[str, str]) -> tuple[int, int]:
    tree = ET.parse(EN_TS)
    root = tree.getroot()
    ctx = root.find("context")
    if ctx is None:
        raise RuntimeError("No context in TS file")

    existing: dict[str, ET.Element] = {}
    for msg in ctx.findall("message"):
        src_el = msg.find("source")
        if src_el is not None and src_el.text is not None:
            existing[src_el.text] = msg

    added = updated = 0
    for src, en in sorted(extra.items()):
        if src in existing:
            trans_el = existing[src].find("translation")
            if trans_el is not None:
                cur = (trans_el.text or "").strip()
                typ = trans_el.get("type", "")
                if not cur or typ == "unfinished" or (cur == src and any(ord(c) > 127 for c in src)):
                    trans_el.text = en
                    trans_el.attrib.pop("type", None)
                    updated += 1
        else:
            msg = ET.SubElement(ctx, "message")
            s = ET.SubElement(msg, "source")
            s.text = src
            t = ET.SubElement(msg, "translation")
            t.text = en
            added += 1

    # Sort messages by source text
    messages = list(ctx.findall("message"))
    for m in messages:
        ctx.remove(m)
    messages.sort(key=lambda m: (m.find("source").text or "") if m.find("source") is not None else "")
    for m in messages:
        ctx.append(m)

    tree.write(EN_TS, encoding="utf-8", xml_declaration=True)
    return added, updated


def main() -> None:
    added, updated = update_en_ts(NEW_TRANSLATIONS)
    print(f"inkcut_en.ts: +{added} new, ~{updated} updated")


if __name__ == "__main__":
    main()
