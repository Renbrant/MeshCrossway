#!/usr/bin/env python3
"""Pure Python fixture checks for one-time Android debug APK migration."""
import importlib.util
import json
from pathlib import Path
import sqlite3
import tempfile
import zipfile

script = Path(__file__).resolve().parent / "migrate_android_debug_install.py"
spec = importlib.util.spec_from_file_location("migration", script)
migration = importlib.util.module_from_spec(spec)
spec.loader.exec_module(migration)

with tempfile.TemporaryDirectory() as temp:
    base = Path(temp)
    (base / "databases").mkdir()
    (base / "shared_prefs").mkdir()
    h = sqlite3.connect(base / "databases" / "meshcrossway-history.db")
    for table in migration.HISTORY:
        h.execute("CREATE TABLE " + table + "(id TEXT PRIMARY KEY,label TEXT)")
    h.execute("INSERT INTO nodes VALUES('node-one','Visible node')")
    h.execute("INSERT INTO flags VALUES('favorite-one','1')")
    h.commit()
    h.close()
    t = sqlite3.connect(base / "databases" / "meshcrossway-tracks.db")
    t.execute("CREATE TABLE folders(id TEXT PRIMARY KEY,name TEXT)")
    t.execute("CREATE TABLE tracks(id TEXT PRIMARY KEY,name TEXT,state TEXT,color INTEGER,visible INTEGER,created INTEGER,distance REAL,count INTEGER,folder_id TEXT)")
    t.execute("CREATE TABLE fixes(track TEXT,seq INTEGER,segment INTEGER,lat REAL,lon REAL,accuracy REAL,altitude REAL,time INTEGER)")
    t.execute("INSERT INTO folders VALUES('71425100-46b1-4dd5-a65b-14ab7680dfc9','Hiking')")
    t.execute("INSERT INTO tracks VALUES('d427fb1c-bace-4187-92f7-c118e1e42a3f','Trail','saved',7,1,17200000,2.1,1,'71425100-46b1-4dd5-a65b-14ab7680dfc9')")
    t.execute("INSERT INTO fixes VALUES('d427fb1c-bace-4187-92f7-c118e1e42a3f',0,0,40.0,-105.0,5,NULL,17200000)")
    t.commit()
    t.close()
    (base / "shared_prefs" / "ui.xml").write_text(
        '<map><boolean name="gpsEnabled" value="true"/><string name="quickReplies">Hello</string></map>', encoding="utf-8"
    )
    (base / "shared_prefs" / "radios.xml").write_text(
        '<map><string name="MESHTASTIC">AA:BB:CC:DD:EE:FF</string><boolean name="autoReconnect" value="true"/></map>',
        encoding="utf-8"
    )
    archive = base / "export.zip"
    migration.write_output(base, archive)
    with zipfile.ZipFile(archive) as result:
        assert set(result.namelist()) == {
            "manifest.json", "settings.json", "history.json", "tracks.json"
        }
        metadata = json.loads(result.read("manifest.json"))
        assert metadata["format"] == "MeshCrosswayDeviceBackup"
        data = json.loads(result.read("history.json"))
        assert data["tables"]["nodes"][0]["label"] == "Visible node"
        assert data["tables"]["flags"][0]["id"] == "favorite-one"
        assert data["selection"]["MESHTASTIC"] == "AA:BB:CC:DD:EE:FF"
        gps = json.loads(result.read("tracks.json"))
        assert gps["tracks"][0]["name"] == "Trail"
        assert gps["folders"][0]["name"] == "Hiking"
        assert gps["fixes"][0]["lat"] == 40.0
        assert len(gps["fixes"]) == 1
        prefs = json.loads(result.read("settings.json"))
        assert prefs["files"]["ui"]["gpsEnabled"]["value"] is True
        assert prefs["files"]["ui"]["quickReplies"]["value"] == "Hello"
    print("Offline debug migration fixture passed: history, settings, favorites, tracks, folders, all fixes.")
