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
import hashlib
from datetime import datetime, timezone
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
EXPECTED_FILES = {"manifest.json", "settings.json", "history.json", "tracks.json"}
MAX_ENTRY_SIZE = {"manifest.json": 16384, "settings.json": 131072,
                  "history.json": 25 * 1024 * 1024,
                  "tracks.json": 160 * 1024 * 1024}


def shell(args):
    result = subprocess.run(args, capture_output=True)
    if result.returncode:
        raise RuntimeError((result.stderr or b"").decode(errors="replace")[:800])
    return result.stdout


def snapshot(adb, destination):
    # Fail BEFORE force-stop if the installed APK cannot be read using run-as.
    # This avoids disturbing a production-signed/non-debuggable installation.
    shell([adb, "exec-out", "run-as", PACKAGE, "id"])
    print("Debug app is accessible over ADB. Stopping MeshCrossway temporarily for a coherent snapshot.")
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



def validate_archive(archive_path):
    """Offline CRC, schema and size validation without displaying private contents.

    A CRC-valid ZIP is not proof that Android's importer will accept every
    row; users must retain the old app until actual restore is confirmed.
    """
    with zipfile.ZipFile(archive_path, "r") as archive:
        entries = archive.infolist()
        names = [item.filename for item in entries]
        if len(entries) != 4 or set(names) != EXPECTED_FILES:
            raise RuntimeError("Backup must contain the four expected JSON files exactly once")
        for item in entries:
            if item.is_dir() or item.file_size > MAX_ENTRY_SIZE[item.filename] or item.file_size == 0:
                raise RuntimeError("Backup entry is empty or exceeds the Android import limit")
        bad = archive.testzip()
        if bad:
            raise RuntimeError("ZIP data/CRC corruption detected in " + bad)
        with archive.open("manifest.json") as entry:
            manifest = json.load(entry)
        with archive.open("settings.json") as entry:
            prefs = json.load(entry)
        if manifest.get("format") != "MeshCrosswayDeviceBackup" or manifest.get("version") != 1:
            raise RuntimeError("Unsupported complete backup manifest")
        if prefs.get("format") != "MeshCrosswaySettings" or prefs.get("version") != 1:
            raise RuntimeError("Invalid settings backup")
        if not isinstance(prefs.get("files"), dict) or not all(
            isinstance(prefs["files"].get(name, {}), dict) for name in PREFS
        ):
            raise RuntimeError("Missing or invalid preference section")
        # Parse JSON fully for normally sized archives; for a very large track
        # file, CRC/size checks stay streaming to avoid exhausting PC RAM.
        for filename, expected_format in (
            ("history.json", "MeshCrosswayAndroid"),
            ("tracks.json", "MeshCrosswayTracks"),
        ):
            item = archive.getinfo(filename)
            if item.file_size <= 48 * 1024 * 1024:
                with archive.open(filename) as entry:
                    record = json.load(entry)
                if record.get("format") != expected_format or record.get("version") != 1:
                    raise RuntimeError("Unexpected format in " + filename)
                required = HISTORY if filename == "history.json" else TRACKS
                tables = record.get("tables") if filename == "history.json" else record
                if not all(isinstance(tables.get(table), list) for table in required):
                    raise RuntimeError("Missing data table in " + filename)
            else:
                # This is structural integrity validation, not an importer
                # simulation for very large files.
                with archive.open(filename) as entry:
                    prefix = entry.read(256).lstrip()
                if not prefix.startswith(b'{"format":"MeshCrosswayTracks","version":1,'):
                    raise RuntimeError("Unexpected large-track archive header")
    return hashlib.sha256(archive_path.read_bytes()).hexdigest()


def archive_counts(folder):
    """Counts only; never prints names, contacts, messages or coordinates."""
    result = {}
    for filename, tables in (
        ("meshcrossway-history.db", HISTORY),
        ("meshcrossway-tracks.db", TRACKS),
    ):
        db = connect(folder / "databases" / filename)
        try:
            integrity = db.execute("PRAGMA quick_check(1)").fetchone()
            if not integrity or integrity[0] != "ok":
                raise RuntimeError("Local SQLite integrity check failed")
            for table in tables:
                found = db.execute(
                    "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?", (table,)
                ).fetchone()
                result[table] = db.execute("SELECT count(*) FROM " + table).fetchone()[0] if found else 0
        finally:
            db.close()
    return result


def unique_output(path):
    # Never overwrite an existing user backup; refuse even an empty old file.
    if path.exists():
        raise FileExistsError("Backup already exists: " + str(path))
    if not path.parent.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
    return path


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("--adb", default="adb", help="Location of adb or adb.exe")
    args.add_argument("--output", help="Optional explicit output ZIP path (never overwritten)")
    args.add_argument("--output-dir", help="Folder for a new dated ZIP (default: current folder)")
    args.add_argument("--verify-only", metavar="ZIP", help="Verify an existing backup without the phone")
    options = args.parse_args()
    if options.verify_only:
        filename = pathlib.Path(options.verify_only).expanduser().resolve()
        if not filename.is_file():
            args.error("No backup ZIP at: " + str(filename))
        checksum = validate_archive(filename)
        print("PASS: backup ZIP CRC, format, entries and size limits verified.")
        print("File:", filename)
        print("Size:", round(filename.stat().st_size / (1024 * 1024), 2), "MiB")
        print("SHA-256:", checksum)
        print("The final check is restoring it inside the new app.")
        return
    if options.output and options.output_dir:
        args.error("Choose either --output or --output-dir")
    if options.output:
        target = pathlib.Path(options.output).expanduser().resolve()
    else:
        folder = pathlib.Path(options.output_dir or ".").expanduser().resolve()
        stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-UTC")
        target = folder / ("MeshCrossway-migration-" + stamp + ".zip")
    unique_output(target)
    devices = shell([options.adb, "devices"]).decode(errors="replace")
    if sum(line.endswith("\tdevice") for line in devices.splitlines()) != 1:
        args.error("Connect exactly one phone with authorized USB debugging")
    # Generate the archive under an unpredictable temporary name in the
    # destination directory and only rename it once fully verified.
    with tempfile.TemporaryDirectory(prefix="meshcrossway-migrate-") as temp:
        stage = pathlib.Path(temp)
        snapshot(options.adb, stage)
        counts = archive_counts(stage)
        temp_zip = target.parent / ("." + target.name + ".partial")
        if temp_zip.exists():
            raise FileExistsError("Temporary output already exists: " + str(temp_zip))
        try:
            write_output(stage, temp_zip)
            checksum = validate_archive(temp_zip)
            if target.exists():
                raise FileExistsError("Backup output appeared while copying: " + str(target))
            temp_zip.rename(target)
        finally:
            temp_zip.unlink(missing_ok=True)
    print("")
    print("SUCCESS: full debug-app snapshot saved and verified.")
    print("File:", target)
    print("SHA-256:", checksum)
    print("Record counts (no private content displayed):")
    for table in HISTORY + TRACKS:
        print("  {}: {}".format(table, counts[table]))
    print("")
    print("Do not uninstall the old Android app until the ZIP has been")
    print("copied to a safe location AND restored successfully in the new app.")
    print("This ZIP is NOT ENCRYPTED and contains private messages/location.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, sqlite3.Error, zipfile.BadZipFile, json.JSONDecodeError) as error:
        print("Migration failed:", error, file=sys.stderr)
        print("Do not uninstall the current app.", file=sys.stderr)
        sys.exit(1)
