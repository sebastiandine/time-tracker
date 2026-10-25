# Database schema

Source of truth for the SQLite file opened by `SqliteDatabase`. Timer rules
are [Time tracking](../time-tracking.md). The runner is
`core/src/infra/SqliteDatabase.cpp`. `kSchemaVersion` is 1.

By default the file is `%LOCALAPPDATA%\TimeTracker\time_tracker.db` on Windows
and `$XDG_DATA_HOME/TimeTracker/time_tracker.db` (or
`~/.local/share/TimeTracker/time_tracker.db`) elsewhere. **Data → Select Data
Location...**, `config.json`, `.env`, `TIME_TRACKER_DATA_DIRECTORY`, or
`--data-directory` can choose another directory. The file name stays
`time_tracker.db`. A missing file is created. Migration creates the tables
below and does not insert categories, topics, or time entries. WAL is on for
files. Memory databases used by tests skip WAL. After migration an authorizer
allows only ordinary reads and writes. The configuration cascade is in the
root [README](../../README.md#configuration).

## Category

```sql
CREATE TABLE Category (
  id    INTEGER PRIMARY KEY,
  name  TEXT NOT NULL UNIQUE COLLATE NOCASE,
  notes TEXT NOT NULL DEFAULT ''
) STRICT;
```

| Column | Meaning |
|---|---|
| id | Row id. Not reused by `AUTOINCREMENT`; SQLite may reuse a deleted id. |
| name | Display name. Unique, ASCII case-insensitive. |
| notes | Explanation. Empty string when the user left it blank. |

The `UNIQUE` constraint is the name lookup. There is no second name index.

## MainTopic

```sql
CREATE TABLE MainTopic (
  id          INTEGER PRIMARY KEY,
  category_id INTEGER NOT NULL REFERENCES Category(id),
  name        TEXT NOT NULL COLLATE NOCASE,
  notes       TEXT NOT NULL DEFAULT '',
  UNIQUE (category_id, name)
) STRICT;
```

| Column | Meaning |
|---|---|
| id | Row id. |
| category_id | Parent category. No cascade. |
| name | Unique among siblings, ASCII case-insensitive. |
| notes | Explanation. |

`UNIQUE (category_id, name)` also serves `WHERE category_id = ?`.

## SubTopic

```sql
CREATE TABLE SubTopic (
  id            INTEGER PRIMARY KEY,
  main_topic_id INTEGER NOT NULL REFERENCES MainTopic(id),
  name          TEXT NOT NULL COLLATE NOCASE,
  notes         TEXT NOT NULL DEFAULT '',
  UNIQUE (main_topic_id, name)
) STRICT;
```

| Column | Meaning |
|---|---|
| id | Row id. |
| main_topic_id | Parent main topic. No cascade. |
| name | Unique among siblings, ASCII case-insensitive. |
| notes | Explanation. |

## TimeEntry

```sql
CREATE TABLE TimeEntry (
  id            INTEGER PRIMARY KEY,
  main_topic_id INTEGER REFERENCES MainTopic(id),
  sub_topic_id  INTEGER REFERENCES SubTopic(id),
  started_at    TEXT NOT NULL,
  ended_at      TEXT,
  is_running    INTEGER NOT NULL CHECK (is_running IN (0, 1)),
  notes         TEXT NOT NULL DEFAULT '',
  CHECK ((main_topic_id IS NOT NULL AND sub_topic_id IS NULL) OR
         (main_topic_id IS NULL AND sub_topic_id IS NOT NULL)),
  CHECK ((ended_at IS NULL AND is_running = 1) OR
         (ended_at IS NOT NULL AND is_running = 0))
) STRICT;
```

| Column | Meaning |
|---|---|
| id | Row id. |
| main_topic_id | Set when time was logged on the main topic. Exactly one target is set. |
| sub_topic_id | Set when time was logged on a subtopic. |
| started_at | UTC `YYYY-MM-DDTHH:MM:SSZ`. Written when Start is pressed. |
| ended_at | Same form. Null while the entry is running. |
| is_running | 1 only for the open entry. |
| notes | Note for this time record, not the topic's standing note. |

### Indexes

- `idx_time_entry_single_open` — `UNIQUE (is_running) WHERE is_running = 1`.
  Keeps a second open timer from being inserted. A plain unique index cannot
  do this, because SQLite treats null `ended_at` values as distinct.
- `idx_time_entry_started` — `(started_at, id)`. Serves
  `started_at < ?` for a statistics window.
- `idx_time_entry_main` — `(main_topic_id, started_at DESC, id DESC)`. Serves
  closed history for one main topic, newest first.
- `idx_time_entry_sub` — `(sub_topic_id, started_at DESC, id DESC)`. Same for
  a subtopic.

## Setting

```sql
CREATE TABLE Setting (
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL,
  CHECK (key IN ('theme'))
) STRICT;
```

| Column | Meaning |
|---|---|
| key | Only `theme` is allowed. |
| value | `system`, `light`, or `dark`. The service rejects any other text. |

A missing `theme` row means System. The theme is not a secret.
