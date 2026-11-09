#!/usr/bin/env python3
"""One-time extraction from an installed DEBUG APK via adb run-as.

Use BEFORE uninstalling your current Android 0.2.x debug build. Requires
Python >=3.9, Android Platform Tools/adb, USB debugging authorized on the
phone and exactly one connected Android device. Does not upload any data.

The output ZIP is compatible with MeshCrossway 0.2.3 full-backup import.
It contains private messages, selected radios, settings, favorites, track
folders and every GPS point. It is NOT encrypted: store it privately.
"""
import argparse
import json
import pathlib
import sqlite3
import subprocess
import sys
import tarfile
import tempfile
import time
import xml.etree.ElementTree as ET
import zipfile

PACKAGE = "com.meshcrossway.mobile"
HISTORY = ("nodes", "channels", "messages", "drafts", "flags", "pois")
TRACKS = ("folders", "tracks", "fixes")
PREFS = ("radios", "ui", "map")
MAX_BYTES = 300 * 1024 * 1024


def shell(args):
    result = subprocess.run(args, capture_output=True)
    if result.returncode:
        raise RuntimeError((result.stderr or b"").decode(errors="replace")[:800])
    return result.stdout


def snapshot(adb, destination):
    shell([adb, "shell", "am", "force-stop", PACKAGE])
    command = (
        "set -e; cd /data/user/0/" + PACKAGE + "; "
        "set --; for f in databases/meshcrossway-history.db* "
        "databases/meshcrossway-tracks.db* "
        "shared_prefs/radios.xml shared_prefs/ui.xml shared_prefs/map.xml; do "
        '[ -f "$f" ] && set -- "$@" "$f"; '
        'done; [ "$#" -gt 0 ] || exit 2; tar -cf - "$@"'
    )
    process = subprocess.Popen(
        [adb, "exec-out", "run-as", PACKAGE, "sh", "-c", command],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    output = destination / "private-snapshot.tar"
    try:
        with output.open("wb") as file:
            total = 0
            while True:
                chunk = process.stdout.read(65536)
                if not chunk:
                    break
                total += len(chunk)
                if total > MAX_BYTES:
                    process.kill()
                    raise RuntimeError("Snapshot exceeds 300 MB")
                file.write(chunk)
        errors = process.stderr.read().decode(errors="replace")
        if process.wait(timeout=20):
            raise RuntimeError(
                "Could not read installed app. This requires a debuggable "
                "APK and authorized adb USB connection. " + errors[:400]
            )
        with tarfile.open(output, "r") as archive:
            for item in archive:
                name = pathlib.PurePosixPath(item.name)
                if (item.issym() or item.islnk() or not item.isfile()
                        or name.is_absolute() or ".." in name.parts):
                    raise ValueError("Unsafe archive entry")
                if not (
                    name.parts[0] == "databases" and
                    name.name.startswith(("meshcrossway-history.db", "meshcrossway-tracks.db"))
                ) and not (
                    name.parts[0] == "shared_prefs" and
                    name.name in {"radios.xml", "ui.xml", "map.xml"}
                ):
                    raise ValueError("Unexpected archived app data")
                target = destination / name
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(item) as source, target.open("wb") as sink:
                    while True:
                        chunk = source.read(65536)
                        if not chunk:
                            break
                        sink.write(chunk)
    finally:
        if process.poll() is None:
            process.kill()
        output.unlink(missing_ok=True)


def settings(folder):
    result = {"format": "MeshCrosswaySettings", "version": 1, "files": {}}
    for filename in PREFS:
        entries = {}
        file = folder / "shared_prefs" / (filename + ".xml")
        if file.exists():
            root = ET.parse(file).getroot()
            if root.tag != "map":
                raise ValueError("Invalid Android preferences")
            for item in root:
                key = item.get("name")
                if not key or len(key) > 100:
                    continue
                if item.tag == "string":
                    kind, value = "string", item.text or ""
                elif item.tag == "boolean":
                    kind, value = "boolean", item.get("value") == "true"
                elif item.tag in ("int", "long"):
                    kind, value = ("integer" if item.tag == "int" else "long"), int(item.get("value"))
                elif item.tag == "float":
                    kind, value = "float", float(item.get("value"))
                else:
                    continue
                entries[key] = {"type": kind, "value": value}
        result["files"][filename] = entries
    return result


def connect(filename):
    if not filename.is_file():
        raise FileNotFoundError("Missing private app database: " + filename.name)
    db = sqlite3.connect("file:" + str(filename) + "?mode=ro", uri=True)
    db.row_factory = sqlite3.Row
    return db


def row_writer(handle, database, tables, converter=None):
    handle.write(b"{")
    for ix, table in enumerate(tables):
        if ix:
            handle.write(b",")
        handle.write(json.dumps(table).encode() + b":[")
        found = database.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name=?", (table,)
        ).fetchone()
        if found:
            for row_ix, item in enumerate(database.execute("SELECT * FROM " + table)):
                if row_ix:
                    handle.write(b",")
                data = dict(item)
                if converter:
                    data = converter(table, data)
                handle.write(json.dumps(data, separators=(",", ":"),
                                        ensure_ascii=False, allow_nan=False).encode("utf-8"))
        handle.write(b"]")
    handle.write(b"}")


def write_output(source, target):
    history = connect(source / "databases" / "meshcrossway-history.db")
    tracks = connect(source / "databases" / "meshcrossway-tracks.db")
    preferences = settings(source)
    try:
        with zipfile.ZipFile(target, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as output:
            output.writestr("manifest.json", json.dumps({
                "format": "MeshCrosswayDeviceBackup", "version": 1,
                "exportedAt": int(time.time() * 1000),
                "warning": "Unencrypted private conversations and GPS tracks"
            }))
            output.writestr("settings.json", json.dumps(preferences))
            with output.open("history.json", "w") as out:
                out.write(b'{"format":"MeshCrosswayAndroid","version":1,"tables":')
                row_writer(out, history, HISTORY)
                selected = {
                    name: preferences["files"]["radios"].get(name, {}).get("value", "")
                    for name in ("MESHTASTIC", "MESHCORE")
                }
                out.write(b',"selection":')
                out.write(json.dumps(selected).encode("utf-8"))
                out.write(b"}")
            with output.open("tracks.json", "w") as out:
                out.write(b'{"format":"MeshCrosswayTracks","version":1,')
                def converter(table, row):
                    if table == "tracks":
                        row.setdefault("folder_id", "")
                    return row
                for ix, table in enumerate(TRACKS):
                    if ix:
                        out.write(b",")
                    out.write(json.dumps(table).encode() + b":[")
                    found = tracks.execute(
                        "SELECT name FROM sqlite_master WHERE type='table' AND name=?", (table,)
                    ).fetchone()
                    if found:
                        for j, item in enumerate(tracks.execute("SELECT * FROM " + table)):
                            if j:
                                out.write(b",")
                            out.write(json.dumps(converter(table, dict(item)),
                                                 separators=(",", ":"), ensure_ascii=False,
                                                 allow_nan=False).encode("utf-8"))
                    out.write(b"]")
                out.write(b"}")
    finally:
        history.close()
        tracks.close()
    with zipfile.ZipFile(target) as output:
        if set(output.namelist()) != {
            "manifest.json", "settings.json", "history.json", "tracks.json"
        } or output.testzip() is not None:
            raise RuntimeError("Backup ZIP verification failed")


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("--adb", default="adb")
    args.add_argument("--output", default="MeshCrossway-complete-migration.zip")
    options = args.parse_args()
    target = pathlib.Path(options.output).expanduser().resolve()
    if target.exists():
        args.error("Refusing to overwrite an existing backup")
    devices = shell([options.adb, "devices"]).decode(errors="replace")
    if sum(line.endswith("\tdevice") for line in devices.splitlines()) != 1:
        args.error("Connect exactly one USB-debugging-authorized phone")
    try:
        with tempfile.TemporaryDirectory(prefix="meshcrossway-migrate-") as temp:
            stage = pathlib.Path(temp)
            snapshot(options.adb, stage)
            write_output(stage, target)
        print("Complete backup saved:", target)
        print("Keep this private ZIP in a safe location and import it in MeshCrossway 0.2.3.")
        print("Check the ZIP is intact BEFORE uninstalling your installed Android app.")
    except Exception:
        target.unlink(missing_ok=True)
        raise


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, sqlite3.Error) as error:
        print("Migration failed:", error, file=sys.stderr)
        print("Do not uninstall the current app.", file=sys.stderr)
        sys.exit(1)
