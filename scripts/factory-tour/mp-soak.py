#!/usr/bin/env python3
"""Two-player desync soak: a headless host plus a GUI client under Xvfb that keeps building.

    scripts/factory-tour/mp-soak.py PARK SCRATCH_DIR [--minutes 10] [--display :77] [--port 11760]

The host runs `openrct2-cli host` with its own user data (not advertised, joining players get the User group); the
client runs `openrct2 join` on DISPLAY with desync debugging on. The client then builds and removes belts, chests and
inserters at random over the park's free ground (a fixed seed, so runs repeat). Every few seconds the client log
and its desyncs/ folder are checked; any desync ends the run with a non-zero exit. Expects the 1280x720 view of
the FactoryTopologyTests slice park (see wiki/TESTING.md); SCRATCH_DIR must not be on the NTFS D drive.
"""
import argparse
import os
import random
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BUILD = os.path.join(REPO, "build")


def user_dir(path, overrides):
    os.makedirs(path, exist_ok=True)
    src = os.path.expanduser("~/.config/OpenRCT2/config.ini")
    text = open(src).read()
    for key, value in overrides.items():
        text, n = re.subn(rf"^{key} = .*$", f"{key} = {value}", text, flags=re.M)
        if n == 0:
            sys.exit(f"config key {key} not found")
    open(os.path.join(path, "config.ini"), "w").write(text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("park")
    parser.add_argument("scratch")
    parser.add_argument("--minutes", type=float, default=10)
    parser.add_argument("--display", default=":77")
    parser.add_argument("--port", type=int, default=11760)
    args = parser.parse_args()

    scratch = os.path.abspath(args.scratch)
    host_dir = os.path.join(scratch, "soak-host")
    client_dir = os.path.join(scratch, "soak-client")
    for d in (host_dir, client_dir):
        shutil.rmtree(d, ignore_errors=True)
    common = {"advertise": "false", "play_intro": "false", "default_port": str(args.port)}
    user_dir(host_dir, {**common, "player_name": '"host"', "log_server_actions": "true"})
    user_dir(client_dir, {**common, "player_name": '"soaker"', "desync_debugging": "true"})
    env = dict(os.environ, SDL_AUDIODRIVER="dummy")
    host_log = open(os.path.join(scratch, "soak-host.log"), "w")
    host_cmd = [os.path.join(BUILD, "openrct2-cli"), "host", args.park, f"--port={args.port}",
                f"--user-data-path={host_dir}"]
    # Joining players start in group 2 (User, upstream's default rights plus factory building) instead of Spectator.
    user_permissions = ["CHAT", "TERRAFORM", "SET_WATER_LEVEL", "TOGGLE_PAUSE", "CREATE_RIDE", "REMOVE_RIDE",
                        "BUILD_RIDE", "RIDE_PROPERTIES", "SCENERY", "PATH", "CLEAR_LANDSCAPE", "GUEST", "STAFF",
                        "PARK_PROPERTIES", "PARK_FUNDING", "TOGGLE_SCENERY_CLUSTER", "DRAG_PATH_AREA", "FACTORY"]
    groups = {
        "default_group": 2,
        "groups": [
            {"id": 0, "name": "Admin", "permissions": ["PERMISSION_" + p for p in user_permissions + [
                "KICK_PLAYER", "MODIFY_GROUPS", "SET_PLAYER_GROUP", "CHEAT", "PASSWORDLESS_LOGIN", "MODIFY_TILE",
                "EDIT_SCENARIO_OPTIONS"]]},
            {"id": 1, "name": "Spectator", "permissions": ["PERMISSION_CHAT"]},
            {"id": 2, "name": "User", "permissions": ["PERMISSION_" + p for p in user_permissions]},
        ],
    }
    import json
    json.dump(groups, open(os.path.join(host_dir, "groups.json"), "w"), indent=4)
    host = subprocess.Popen(host_cmd, stdout=host_log, stderr=subprocess.STDOUT, env=env)
    time.sleep(8)
    env.update(DISPLAY=args.display, XAUTHORITY="/dev/null")
    os.environ.update(DISPLAY=args.display, XAUTHORITY="/dev/null")
    client_log_path = os.path.join(scratch, "soak-client.log")
    client_log = open(client_log_path, "w")
    client = subprocess.Popen([os.path.join(BUILD, "openrct2"), "join", "127.0.0.1", f"--port={args.port}",
                               f"--user-data-path={client_dir}"], stdout=client_log, stderr=subprocess.STDOUT, env=env)
    time.sleep(15)

    import xdrive  # needs DISPLAY set

    def shot(name):
        subprocess.run(["import", "-window", "root", os.path.join(scratch, name)], check=False)

    def desynced():
        text = open(client_log_path, errors="replace").read()
        reports = os.listdir(os.path.join(client_dir, "desyncs")) if os.path.isdir(os.path.join(client_dir, "desyncs")) else []
        return ("desync" in text.lower()) or bool(reports)

    rng = random.Random(1234)
    # Free ground in the slice park view at 1280x720, and the palette slots of the first row.
    area = (330, 260, 720, 450)
    palette = {"belt": (36, 80), "inserter": (102, 80), "chest": (168, 80)}
    xdrive.move(1075, 12)
    xdrive.button(1, True)
    xdrive.button(1, False)
    time.sleep(1)
    shot("soak-start.png")
    deadline = time.time() + args.minutes * 60
    actions = 0
    result = 0
    while time.time() < deadline:
        if host.poll() is not None or client.poll() is not None:
            print("a game process exited early")
            result = 2
            break
        kind = rng.choice(["belt", "belt", "chest", "inserter", "remove", "rotate"])
        x = rng.randint(area[0], area[2])
        y = rng.randint(area[1], area[3])
        if kind in palette:
            xdrive.move(*palette[kind])
            xdrive.button(1, True)
            xdrive.button(1, False)
            time.sleep(0.2)
        if kind == "belt":
            x2 = min(area[2], max(area[0], x + rng.randint(-160, 160)))
            y2 = min(area[3], max(area[1], y + rng.randint(-80, 80)))
            xdrive.move(x, y)
            xdrive.button(1, True)
            for s in range(1, 9):
                xdrive.move(x + (x2 - x) * s / 8, y + (y2 - y) * s / 8)
            xdrive.button(1, False)
        elif kind in ("chest", "inserter"):
            xdrive.move(x, y)
            xdrive.button(1, True)
            xdrive.button(1, False)
        elif kind == "remove":
            xdrive.move(x, y)
            xdrive.button(3, True)
            xdrive.button(3, False)
        else:
            xdrive.key("z")
        actions += 1
        time.sleep(0.4)
        if actions % 25 == 0:
            if desynced():
                print(f"DESYNC after {actions} actions")
                result = 1
                break
            print(f"{actions} actions, {int(deadline - time.time())} s left, in sync", flush=True)
    time.sleep(5)
    shot("soak-end.png")
    if result == 0 and desynced():
        print("DESYNC at the end")
        result = 1
    client.terminate()
    host.terminate()
    time.sleep(2)
    for p in (client, host):
        if p.poll() is None:
            p.kill()
    print(f"soak finished: {actions} actions, {'desync' if result == 1 else 'no desync' if result == 0 else 'aborted'}")
    sys.exit(result)


if __name__ == "__main__":
    main()
