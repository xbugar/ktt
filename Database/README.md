# Database usage

This module provides a lightweight SQLite-backed store for tuning results. Typical usage is guarded by the KTT_DATABASE define.

## Enable database usage

Enable the database build option in premake5 (use the option `--database`), which defines `KTT_DATABASE` and includes the database header in the build.

An end-to-end usage example is already present in [/Examples/CoulombSum3dDatabase/CoulombSum3dDatabase.cpp](Examples/CoulombSum3dDatabase/CoulombSum3dDatabase.cpp).

## Load results and stats

Use Database with the default location (~/.local/share/ktt/ktt.db):

```cpp
const auto db = ktt::db::Database();
const auto tuningInfo = tuner.GetDatabaseTuningInfo(kernel);
const auto stats = db.GetStatsForSource(tuningInfo.spaceInfo.sourceFingerprint);
if (stats)
{
    const nlohmann::json statsJson = *stats;
    std::cout << statsJson.dump(2) << std::endl;
}
const auto results = db.SimpleGetBestResults(tuningInfo);
```

For more control, use a query with predicates:

```cpp
const ktt::db::GetBestResultsQuery query{
    tuningInfo.spaceInfo,
    std::function<bool(const ktt::db::DeviceInfo &)>([](const ktt::db::DeviceInfo &device) {
        return device.computeApi == ktt::ComputeApi::CUDA &&
            device.cudaComputeCapabilityMajor.has_value() &&
            device.cudaComputeCapabilityMinor.has_value() &&
            (device.cudaComputeCapabilityMajor.value() > 10 ||
                (device.cudaComputeCapabilityMajor.value() == 7 &&
                    device.cudaComputeCapabilityMinor.value() >= 5));
    }),
    std::nullopt, // no input filter
    50
};
const auto results = db.GetBestResults(query);
```

## Save results

Capture results and persist them for later runs. You can add input metadata to the run:

```cpp
#ifdef KTT_DATABASE
const auto db = ktt::db::Database();
auto tuningInfo = tuner.GetDatabaseTuningInfo(kernel);
tuningInfo.inputData = "atoms=" + std::to_string(atoms) + ";gridSize=" + std::to_string(gridSize);
db.SaveResults(tuningInfo, results, {ktt::OutputFormat::JSON, 2}); // format and JSON indent size for stored results
#endif
```

## Sync from another database

Merge all tuning data from another database file into the current one. Runs are matched by their GUID, so
runs that already exist are skipped — the operation is idempotent and safe to repeat. Each newly copied run
brings along its tuning source, tuning space, device and results, and the copy runs inside a single
transaction (a failure leaves the current database unchanged). The other database is opened read-only.

```cpp
#ifdef KTT_DATABASE
const auto db = ktt::db::Database();
const size_t added = db.SyncFromFile("/path/to/other-ktt.db");
std::cout << "Added " << added << " new run(s)" << std::endl;
#endif
```

## Output format and JSON indentation

Database stores tuning results as text in the database. The serialization format and, for JSON, its
indentation are controlled per call through the `SaveOptions` argument to `SaveResults` (not the
constructor). Indentation is useful for SQL reader apps (e.g., DBeaver) that display JSON text more
clearly. `SaveOptions` defaults to JSON with an indent of 2:

```cpp
db.SaveResults(tuningInfo, results);                                // default: JSON, indent = 2
db.SaveResults(tuningInfo, results, {ktt::OutputFormat::JSON, 4});  // JSON, indent with 4 spaces
db.SaveResults(tuningInfo, results, {ktt::OutputFormat::JSON, 0});  // JSON, compact (no indentation)
db.SaveResults(tuningInfo, results, {ktt::OutputFormat::XML, 2});   // XML (indent is ignored)
```

If you need a custom database path, use the path/connection constructor:

```cpp
ktt::db::Database("/tmp/ktt.db");
ktt::db::Database(sqlite3Connection);
```
