#!/usr/bin/env python3
"""Minimal X input driver (an xdotool stand-in) for headless GUI checks under Xvfb.

Uses libX11 + libXtst through ctypes. Commands run in order:
    xdrive.py move X Y | down [BUTTON] | up [BUTTON] | click [BUTTON] | drag X1 Y1 X2 Y2 [STEPS]
              key KEYSYM | type TEXT | sleep SECONDS | shot FILE.png
Several commands can be chained:  xdrive.py move 10 10 click sleep 0.5 shot out.png
DISPLAY must name the target server (for example :99 from Xvfb).
"""
import ctypes
import ctypes.util
import os
import subprocess
import sys
import time

_x11 = ctypes.CDLL(ctypes.util.find_library("X11") or "libX11.so.6")
_xtst = ctypes.CDLL(ctypes.util.find_library("Xtst") or "libXtst.so.6")
_x11.XOpenDisplay.restype = ctypes.c_void_p
_x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
_x11.XFlush.argtypes = [ctypes.c_void_p]
_x11.XStringToKeysym.restype = ctypes.c_ulong
_x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
_x11.XKeysymToKeycode.restype = ctypes.c_ubyte
_x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
_xtst.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ulong]
_xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
_xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]

_display = _x11.XOpenDisplay(None)
if not _display:
    sys.exit("cannot open display " + os.environ.get("DISPLAY", ""))


def move(x, y):
    _xtst.XTestFakeMotionEvent(_display, -1, int(x), int(y), 0)
    _x11.XFlush(_display)
    time.sleep(0.03)


def button(number, pressed):
    _xtst.XTestFakeButtonEvent(_display, int(number), 1 if pressed else 0, 0)
    _x11.XFlush(_display)
    time.sleep(0.05)


def key(keysym):
    code = _x11.XKeysymToKeycode(_display, _x11.XStringToKeysym(keysym.encode()))
    _xtst.XTestFakeKeyEvent(_display, code, 1, 0)
    _xtst.XTestFakeKeyEvent(_display, code, 0, 0)
    _x11.XFlush(_display)
    time.sleep(0.05)


def main(argv):
    i = 0
    while i < len(argv):
        cmd = argv[i]
        i += 1
        def arg(default=None):
            nonlocal i
            if i < len(argv) and (default is None or argv[i].lstrip("-").replace(".", "").isdigit()):
                i += 1
                return argv[i - 1]
            return default
        if cmd == "move":
            move(arg(), arg())
        elif cmd == "down":
            button(arg("1"), True)
        elif cmd == "up":
            button(arg("1"), False)
        elif cmd == "click":
            b = arg("1")
            button(b, True)
            button(b, False)
        elif cmd == "drag":
            x1, y1, x2, y2 = (int(arg()) for _ in range(4))
            steps = int(arg("12"))
            move(x1, y1)
            button(1, True)
            for s in range(1, steps + 1):
                move(x1 + (x2 - x1) * s / steps, y1 + (y2 - y1) * s / steps)
                time.sleep(0.03)
            button(1, False)
        elif cmd == "key":
            key(arg())
        elif cmd == "type":
            for ch in arg():
                key(ch)
        elif cmd == "sleep":
            time.sleep(float(arg()))
        elif cmd == "shot":
            subprocess.run(["import", "-window", "root", arg()], check=True)
        else:
            sys.exit("unknown command " + cmd)


if __name__ == "__main__":
    main(sys.argv[1:])
