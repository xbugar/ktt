# Database usage

This module provides a lightweight SQLite-backed store for tuning results. Typical usage is guarded by the KTT_DATABASE define.

For hands-on, code-level usage see [Example.md](Example.md), which walks through the end-to-end example in [/Examples/CoulombSum3dDatabase/CoulombSum3dDatabase.cpp](Examples/CoulombSum3dDatabase/CoulombSum3dDatabase.cpp). The schema behind these operations is documented in [ERD.md](ERD.md).

## Enable database usage

Enable the database build option in premake5 (use the option `--database`), which defines `KTT_DATABASE` and includes the database header in the build.

## Load results and stats

Open a `Database` (default location `~/.local/share/ktt/ktt.db` on Linux, or `$XDG_DATA_HOME/ktt/ktt.db` when set; `%LOCALAPPDATA%\ktt\ktt.db` on Windows) and use `GetDatabaseTuningInfo` to key queries against a tuning problem:

- `SimpleGetBestResults` returns the fastest stored results for that tuning problem.
- `GetBestResults` takes a `GetBestResultsQuery` with optional device and input-data predicates for fine-grained filtering.
- `GetStatsForSource` returns aggregate counts (spaces, devices, runs, results) and the name of a source, for a
  source fingerprint.

## Save results

Capture results and persist them for later runs with `SaveResults`. You can attach free-form run metadata via `TuningInfo::inputData`, which is later filterable through the input predicate of a `GetBestResultsQuery`.

## Source and space names

A source and a tuning space are identified by their fingerprints. You can also give them a human-readable name
through `TuningInfo::spaceInfo.sourceName` and `spaceName` (both `std::optional<std::string>`, unset by default):

```cpp
auto tuningInfo = tuner.GetDatabaseTuningInfo(kernel);
tuningInfo.spaceInfo.sourceName = "Coulomb sum 3D";
tuningInfo.spaceInfo.spaceName = "Default search space";
db.SaveResults(tuningInfo, results);
```

A name is stored only when the source or space is created, that is on the first save with its fingerprints. A later
save never changes it, even when it carries a different name or none. The source name is returned in
`SourceStats::name` by `GetStatsForSource`.

The schema is not migrated. A database file written before the names existed (without the `name` columns) is not
supported and must be recreated.

## Sync from another database

`SyncFrom` merges all tuning data from another `Database` into the current one. The other database may be
constructed from a path, an in-memory database or an existing `sqlite3*` connection. `SyncFrom` only reads runs from
it, but constructing a `Database` from a path writes to the file: it creates the file and any missing tables and
inserts the compute API rows when they are missing, which needs write access even when nothing changes. So the other
database's file must be writable, also when it is a backup. Runs are matched by their GUID, so runs that already
exist are skipped — the operation is idempotent and safe to repeat. Each newly copied run brings along its tuning
source, tuning space, device and results (the source and space names are applied only when the source or space is new
in the current database), and the copy runs inside a single transaction (a failure leaves the current database
unchanged). Syncing a database from itself (the same connection, or the same file through any path) throws.

```cpp
db.SyncFrom(ktt::db::Database(otherDatabasePath));
```

## Output format and JSON indentation

Database stores tuning results as text in the database. The serialization format and, for JSON, its
indentation are controlled per call through the `SaveOptions` argument to `SaveResults` (not the
constructor). Indentation is useful for SQL reader apps (e.g., DBeaver) that display JSON text more
clearly. `SaveOptions` defaults to JSON with an indent of 2; XML is also supported (indentation is ignored for XML).

If you need a custom database path, construct `Database` with a filesystem path; an already-open `sqlite3` connection can also be adopted.
