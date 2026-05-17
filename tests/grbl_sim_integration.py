#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

import socket
import subprocess
import sys
import time
from pathlib import Path


def read_lines(sock, buf, duration=1.0):
    end = time.time() + duration
    out = []
    while time.time() < end:
        try:
            d = sock.recv(4096)
            if not d:
                break
            buf += d
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                t = line.strip().decode("latin1", "replace")
                if t:
                    out.append(t)
        except socket.timeout:
            pass
    return out, buf


def main():
    if len(sys.argv) < 3:
        print("usage: grbl_sim_integration.py <inkcut_cpp_bin> <port>", file=sys.stderr)
        return 2
    inkcut_bin = Path(sys.argv[1])
    port = int(sys.argv[2])
    fixture = Path(__file__).resolve().parent / "fixtures" / "sample_cut.svg"
    if not inkcut_bin.exists():
        print(f"missing inkcut-cpp binary: {inkcut_bin}", file=sys.stderr)
        return 2
    if not fixture.exists():
        print(f"missing fixture: {fixture}", file=sys.stderr)
        return 2
    s = socket.create_connection(("127.0.0.1", port), timeout=3)
    s.settimeout(0.5)
    buf = b""

    def send(cmd):
        if isinstance(cmd, str):
            cmd = cmd.encode("latin1")
        s.sendall(cmd)

    # handshake
    send(b"\x18\n")
    lines, buf = read_lines(s, buf, 3.0)
    if not any(x.lower().startswith("grblhal ") or x.lower().startswith("grbl ") for x in lines):
        send("\n")
        lines2, buf = read_lines(s, buf, 2.0)
        lines.extend(lines2)
    if not any(x.lower().startswith("grblhal ") or x.lower().startswith("grbl ") for x in lines):
        print("no welcome", file=sys.stderr)
        return 3

    # status
    send("?\n")
    lines, buf = read_lines(s, buf, 1.0)
    if not any(x.startswith("<") and x.endswith(">") for x in lines):
        print("no status", file=sys.stderr)
        return 4

    # settings
    send("$$\n")
    lines, buf = read_lines(s, buf, 2.5)
    has_setting = any(x.startswith("$") and "=" in x for x in lines)
    has_ok = any(x.strip().lower() == "ok" for x in lines)
    if not has_setting or not has_ok:
        print("settings parse failed", file=sys.stderr)
        return 5

    # write and ack
    for cmd in ("$10=1\n", "$32=0\n", "G21\n", "G90\n", "G0 X0 Y0\n", "G1 X10 Y10 F1000\n"):
        send(cmd)
        lines, buf = read_lines(s, buf, 2.0)
        if not any(x.strip().lower() == "ok" for x in lines):
            send("?\n")
            lines2, buf = read_lines(s, buf, 0.6)
            lines.extend(lines2)
        if not any(x.strip().lower() == "ok" for x in lines):
            print(f"no ack for {cmd.strip()}", file=sys.stderr)
            return 6

    # Real cut path from SVG through inkcut-cpp over TCP.
    cp = subprocess.run(
        [
            str(inkcut_bin),
            "send",
            "--port",
            f"127.0.0.1:{port}",
            "--protocol",
            "gcode",
            "--dry-run",
            str(fixture),
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    if cp.returncode != 0:
        print(cp.stdout, file=sys.stderr)
        print(cp.stderr, file=sys.stderr)
        return 7
    gcode = cp.stdout
    if "G0" not in gcode and "G1" not in gcode:
        print("generated gcode has no moves", file=sys.stderr)
        return 8
    send(gcode if gcode.endswith("\n") else gcode + "\n")
    lines, buf = read_lines(s, buf, 8.0)
    if not any(x.strip().lower() == "ok" for x in lines):
        send("?\n")
        lines2, buf = read_lines(s, buf, 0.8)
        lines.extend(lines2)
    if not any(x.strip().lower() == "ok" for x in lines):
        print("no ack after streaming generated SVG gcode", file=sys.stderr)
        return 9

    s.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
