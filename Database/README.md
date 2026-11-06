# Database usage

This module provides a lightweight SQLite-backed store for tuning results. Typical usage is guarded by the KTT_DATABASE define.

For hands-on, code-level usage see [Example.md](Example.md), which walks through the end-to-end example in [/Examples/CoulombSum3dDatabase/CoulombSum3dDatabase.cpp](Examples/CoulombSum3dDatabase/CoulombSum3dDatabase.cpp). The schema behind these operations is documented in [ERD.md](ERD.md).

## Enable database usage

Enable the database build option in premake5 (use the option `--database`), which defines `KTT_DATABASE` and includes the database header in the build.

The remote client (`RemoteDatabase`, see [Remote database](#remote-database)) is built with `--remote-database`, which
implies `--database`, defines `KTT_REMOTE_DATABASE` and links libcurl. A build with `--database` only does not need
libcurl.

## One interface for local and remote

`Database` (SQLite) implements the interface `ktt::db::IDatabase` (`Database/IDatabase.h`), and so does
`RemoteDatabase`. Code that only saves and reads results can take a `const ktt::db::IDatabase&` and work with either:

```cpp
void SaveRun(const ktt::db::IDatabase& db, const ktt::db::TuningInfo& info, std::vector<ktt::KernelResult> results)
{
    db.SaveResults(info, std::move(results));
}
```

The interface has `SaveResults`, `SimpleGetBestResults`, `GetBestResults` and `GetStatsForSource`, with the same
default arguments on every implementation. `SyncFrom` is on `Database` only, because it copies from one SQLite file to
another.

## Load results and stats

Open a `Database` (default location `~/.local/share/ktt/ktt.db` on Linux, or `$XDG_DATA_HOME/ktt/ktt.db` when set; `%LOCALAPPDATA%\ktt\ktt.db` on Windows) and use `GetDatabaseTuningInfo` to key queries against a tuning problem:

- `SimpleGetBestResults` returns the fastest stored results for that tuning problem on a matching device (see below).
- `GetBestResults` takes a `GetBestResultsQuery` with optional device and input-data predicates for fine-grained filtering.
- `GetStatsForSource` returns aggregate counts (spaces, devices, runs, results) and the name of a source, for a
  source fingerprint.

`SimpleGetBestResults` and `GetBestResults` return the results fastest first, up to the given limit. Both order by the
kernel duration stored when the result was saved (`tuning_result.duration`: `KernelResult::GetKernelDuration` at save
time, in nanoseconds, which leaves out the extra duration of the launcher), so the two methods agree. For results saved
in `JSON_T4`, the loaded `GetKernelDuration` can differ from that key, because the T4 format keeps only the total
duration and loads it as the kernel duration.

`SimpleGetBestResults` uses a stored result when its run's device matches the queried device:

- the same model, that is the same device name, vendor and type, on any compute API; or
- for a CUDA device, a device of another model on CUDA with the same compute capability major version and a minor
  version at most one apart (8.6 matches 8.5, 8.6 and 8.7, but not 8.4, 8.8 or 7.6).

Other device properties (extensions, driver version, device identifier) are not compared. Use `GetBestResults` with a
device predicate for any other rule.

## Time unit

KTT writes the durations inside a serialized result in the time unit active when the result is saved
(`Tuner::SetTimeUnit`, milliseconds by default). `SaveResults` stores that unit with the run (`tuning_run.time_unit`),
and loading a result reads its durations in that stored unit, so the loaded `KernelResult` has the same durations
whatever unit is active when you load it. Runs saved under different units can be mixed in one database, and
`SyncFrom` copies the unit of each run.

To read the stored durations, the database switches KTT's process-wide time unit to the run's unit while it
deserializes a result and restores the previous unit afterwards. This is not thread-safe: do not load results while
another thread uses or changes the time unit (for example while it tunes or prints results).

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

The schema is not migrated. A database file written before the names existed (without the `name` columns) or before
the time unit was stored (without `tuning_run.time_unit`) is not supported and must be recreated.

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

## Remote database

`ktt::db::RemoteDatabase` (`Database/Remote/RemoteDatabase.h`) stores and reads tuning results on a ktt-db server
instead of a local file. Build KTT with `--remote-database` (see below) and include the header yourself; `Ktt.h` only
includes the local `Database`.

```cpp
#include <Database/Remote/RemoteDatabase.h>

const ktt::db::RemoteDatabase db("https://ktt.example.org", apiKey);
```

- **Server URL:** the server root with `http://` or `https://`; the client appends `/ktt-db/app/...`.
- **API key:** create one in the web app's settings. A `read` key may only read, a `write` key may also save. **The
  key decides the realms:** reads see what is linked to any of the key's realms, and a save links the new data to all
  of them. There is no realm parameter. Keep the key out of source code, e.g. read it from an environment variable;
  KTT never writes it into exception messages or logs.
- **Construction** does not contact the server, so a wrong URL or key shows up on the first call.
- `RemoteDatabase` is not copyable, like `Database`. Its methods are `const` and can be called from several threads
  at once.

The `IDatabase` methods of `RemoteDatabase` are not implemented yet: they throw `KttException`. They follow in the next
step of the remote client.

### Connection options

`RemoteDatabaseOptions` is the optional third constructor argument:

| Field | Default | Meaning |
| --- | --- | --- |
| `connectTimeout` | 5 s | Time limit for establishing the connection (0 = none). |
| `requestTimeout` | 60 s | Time limit for one attempt of a request (0 = none). |
| `maxRetries` | 3 | Additional attempts after a connection error, a timeout or `502` / `503` / `504`. |
| `verifyTls` | `true` | Verify the server certificate and host name. Turn it off only for testing. |
| `caBundle` | none | A CA bundle (PEM file) for a server with a certificate from an internal CA. |

- **Retries** wait 250 ms before the first retry and double the wait each time (up to 8 s). Other statuses (`400`,
  `401`, `403`, `404`, `409`, `500`, ...) are never retried, and neither is a `503` that reports a query timeout,
  because the same query would time out again. Every request the client sends can be repeated safely; a save is
  repeated with the same run id, and the server treats it as the same run.
- **Proxy:** libcurl takes it from the environment. `http_proxy` (for `http://` URLs) is read in lower case only;
  libcurl ignores `HTTP_PROXY` on purpose. `https_proxy`, `all_proxy` and `no_proxy` are read in lower or upper case.
- **Redirects** are not followed, so the key is never sent to another host.

### Building with libcurl

- **Linux:** install the libcurl development package (e.g. `libcurl4-openssl-dev` on Debian and Ubuntu), then
  `premake5 gmake --remote-database`.
- **macOS:** the system libcurl from the SDK is used.
- **Windows (Visual Studio):** set `CURL_ROOT` to a **dynamic** libcurl installation with `include/curl/curl.h` and
  the import library `lib/libcurl.lib` before running premake, e.g. vcpkg's `x64-windows` triplet
  (`vcpkg install curl:x64-windows`, then `CURL_ROOT=<vcpkg>\installed\x64-windows`). The release import library is
  linked in every configuration. Put `libcurl.dll` (and the DLLs it depends on, from the same `bin` folder) next to
  the application. Static libcurl builds (e.g. `x64-windows-static`) and the MinGW packages from curl.se are not
  supported by the build script.

`CURL_ROOT` can also point to a custom libcurl on Linux and macOS.

Only the Linux build has been tested so far; the Windows and macOS setups above have not been built yet.
