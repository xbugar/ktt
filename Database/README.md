# Database usage

This module provides a lightweight SQLite-backed store for tuning results. Typical usage is guarded by the KTT_DATABASE define.

## Enable database usage

Enable the database build option in premake5 (use the option `--database`), which defines `KTT_DATABASE` and includes the database header in the build.

An end-to-end usage example is already present in [/Examples/CoulombSum3d/CoulombSum3d.cpp](Examples/CoulombSum3d/CoulombSum3d.cpp).

## Load results and stats

Use Database with the default location (~/.local/share/ktt/ktt.db):

```cpp
#ifdef KTT_DATABASE
const auto db = ktt::db::Database();
const auto save = tuner.GetDatabaseTuningInfo(kernel);
const auto stats = db.GetStatsForSource(save.spaceInfo.sourceFingerprint);
if (stats)
{
    const nlohmann::json statsJson = *stats;
    std::cout << statsJson.dump(2) << std::endl;
}
const auto results = db.GetBestResultsForSource(save);
#endif
```

For more control, use a query with predicates:

```cpp
#ifdef KTT_DATABASE
const ktt::db::GetResultsQuery query{
    save.spaceInfo,
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
const auto results = db.GetBestResultsQuery(query);
#endif
```

## Save results

Capture results and persist them for later runs. You can add input metadata to the run:

```cpp
#ifdef KTT_DATABASE
const auto db = ktt::db::Database(2); // JSON indent size for stored results
auto save = tuner.GetDatabaseTuningInfo(kernel);
save.inputData = "atoms=" + std::to_string(atoms) + ";gridSize=" + std::to_string(gridSize);
db.SaveResultsForSource(save, results);
#endif
```

## JSON indentation

Database stores tuning results as JSON in the database. Indentation is useful for SQL reader apps (e.g., DBeaver) that display JSON text more clearly. The Database constructor takes an indentation size (default 2):

```cpp
ktt::db::Database();     // default indent = 2
ktt::db::Database(4);    // indent with 4 spaces
ktt::db::Database(0);    // compact (no indentation)
```

If you need a custom database path, use the path constructor:

```cpp
ktt::db::Database("/tmp/ktt.db", 2);
```
