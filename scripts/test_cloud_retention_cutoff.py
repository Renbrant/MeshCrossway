#!/usr/bin/env python3
"""Regression check for the cloud message retention cutoff.

Runs the real INSERT used by cloud/src/index.ts against an in-memory SQLite database
built from cloud/migrations, then the same pruning DELETE, and verifies that:
  - recent messages are stored,
  - messages older than seven days are dropped unless the conversation is a favorite,
  - favorite conversations keep their complete history.

The bind order of the `WHERE ? >= ?` guard is read from the source, so swapping the
arguments (cutoff first) makes this script fail.

Usage: python3 -I scripts/test_cloud_retention_cutoff.py [index.ts] [migrations-dir]
"""
import glob
import os
import re
import sqlite3
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INDEX = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "cloud", "src", "index.ts")
MIGRATIONS = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "cloud", "migrations")
SEVEN_DAYS = 7 * 86400


def fail(message):
    print("FAIL: " + message)
    sys.exit(1)


source = open(INDEX, encoding="utf-8").read()
statement = re.search(r"env\.DB\.prepare\(`(INSERT INTO messages.*?)`\)\s*\.bind\(", source, re.S)
if not statement:
    fail("could not find the messages INSERT in " + INDEX)
order = re.search(r"\n\s+(cutoff|message\.sentAt), (cutoff|message\.sentAt), userId, message\.network", source)
if not order:
    fail("could not find the cutoff/sentAt bind arguments in " + INDEX)
first, second = order.group(1), order.group(2)
if {first, second} != {"cutoff", "message.sentAt"}:
    fail("cutoff and message.sentAt must each be bound once")

db = sqlite3.connect(":memory:")
migrations = sorted(glob.glob(os.path.join(MIGRATIONS, "*.sql")))
if not migrations:
    fail("no migrations found in " + MIGRATIONS)
for path in migrations:
    db.executescript(open(path, encoding="utf-8").read())

now = int(time.time())
cutoff = now - SEVEN_DAYS
db.execute("INSERT INTO conversations(user_id,network,conversation_id,favorite,updated_at) "
           "VALUES('u','MESHTASTIC','fav',1,?)", (now,))

cases = [
    ("recent, ordinary chat", "norm", now - 60, True),
    ("8 days old, ordinary chat", "norm", now - 8 * 86400, False),
    ("recent, favorite chat", "fav", now - 60, True),
    ("8 days old, favorite chat", "fav", now - 8 * 86400, True),
]
for index, (_, conversation, sent_at, _) in enumerate(cases):
    values = {"cutoff": cutoff, "message.sentAt": sent_at}
    db.execute(statement.group(1), (
        "u", "MESHTASTIC", conversation, "m%d" % index, sent_at, "sender", "", "body", 0, 0, "", "", 0, 0,
        "received", "{}", values[first], values[second], "u", "MESHTASTIC", conversation))

db.execute("""DELETE FROM messages WHERE user_id=? AND sent_at<? AND NOT EXISTS(
    SELECT 1 FROM conversations c WHERE c.user_id=messages.user_id AND c.network=messages.network
      AND c.conversation_id=messages.conversation_id AND c.favorite=1)""", ("u", cutoff))
kept = {row[0] for row in db.execute("SELECT message_id FROM messages")}

problems = []
for index, (name, _, _, expected) in enumerate(cases):
    stored = "m%d" % index in kept
    print("%-28s expected %-9s got %s" % (name, "stored" if expected else "dropped", "stored" if stored else "dropped"))
    if stored != expected:
        problems.append(name)
if problems:
    fail("retention cutoff is wrong for: " + ", ".join(problems))
print("OK")
