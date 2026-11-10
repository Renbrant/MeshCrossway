PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS conversations (
  user_id TEXT NOT NULL,
  network TEXT NOT NULL CHECK (network IN ('MESHTASTIC', 'MESHCORE')),
  conversation_id TEXT NOT NULL,
  favorite INTEGER NOT NULL DEFAULT 0 CHECK (favorite IN (0, 1)),
  updated_at INTEGER NOT NULL,
  PRIMARY KEY (user_id, network, conversation_id)
);

CREATE TABLE IF NOT EXISTS messages (
  user_id TEXT NOT NULL,
  network TEXT NOT NULL CHECK (network IN ('MESHTASTIC', 'MESHCORE')),
  conversation_id TEXT NOT NULL,
  message_id TEXT NOT NULL,
  sent_at INTEGER NOT NULL,
  sender TEXT NOT NULL,
  body TEXT NOT NULL,
  outgoing INTEGER NOT NULL CHECK (outgoing IN (0, 1)),
  is_read INTEGER NOT NULL DEFAULT 0 CHECK (is_read IN (0, 1)),
  PRIMARY KEY (user_id, network, conversation_id, message_id)
);
CREATE INDEX IF NOT EXISTS messages_user_time
  ON messages(user_id, sent_at DESC);
CREATE INDEX IF NOT EXISTS messages_conversation_time
  ON messages(user_id, network, conversation_id, sent_at DESC);

CREATE TABLE IF NOT EXISTS tracks (
  user_id TEXT NOT NULL,
  track_id TEXT NOT NULL,
  name TEXT NOT NULL,
  object_key TEXT NOT NULL UNIQUE,
  byte_size INTEGER NOT NULL CHECK (byte_size > 0),
  updated_at INTEGER NOT NULL,
  PRIMARY KEY (user_id, track_id)
);
CREATE INDEX IF NOT EXISTS tracks_user ON tracks(user_id, updated_at DESC);

CREATE TRIGGER IF NOT EXISTS tracks_quota_before_insert
BEFORE INSERT ON tracks
BEGIN
  SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM tracks WHERE user_id=NEW.user_id AND track_id=NEW.track_id)
    AND (SELECT COUNT(*) FROM tracks WHERE user_id = NEW.user_id) >= 5
    THEN RAISE(ABORT, 'TRACK_COUNT_LIMIT') END;
  SELECT CASE WHEN NEW.byte_size > 5242880
    THEN RAISE(ABORT, 'TRACK_FILE_LIMIT') END;
  SELECT CASE WHEN (SELECT COALESCE(SUM(byte_size), 0) FROM tracks WHERE user_id = NEW.user_id AND track_id != NEW.track_id) + NEW.byte_size > 26214400
    THEN RAISE(ABORT, 'TRACK_STORAGE_LIMIT') END;
END;

CREATE TRIGGER IF NOT EXISTS tracks_quota_before_update
BEFORE UPDATE OF byte_size, user_id ON tracks
BEGIN
  SELECT CASE WHEN NEW.byte_size > 5242880
    THEN RAISE(ABORT, 'TRACK_FILE_LIMIT') END;
  SELECT CASE WHEN (SELECT COUNT(*) FROM tracks WHERE user_id = NEW.user_id AND track_id != OLD.track_id) >= 5
    THEN RAISE(ABORT, 'TRACK_COUNT_LIMIT') END;
  SELECT CASE WHEN (SELECT COALESCE(SUM(byte_size), 0) FROM tracks WHERE user_id = NEW.user_id AND track_id != OLD.track_id) + NEW.byte_size > 26214400
    THEN RAISE(ABORT, 'TRACK_STORAGE_LIMIT') END;
END;
