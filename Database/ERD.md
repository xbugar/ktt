# Database ERD

Entity-relationship diagram of the SQLite schema created by `Schema::CreateIfNotExists`
(see [Database/Schema/Schema.cpp](Database/Schema/Schema.cpp)).

```mermaid
erDiagram
    compute_api {
        INTEGER id PK
        TEXT name UK "NOT NULL"
    }

    output_format {
        INTEGER id PK
        TEXT name UK "NOT NULL"
    }

    device_info {
        INTEGER id PK "AUTOINCREMENT"
        TEXT name "NOT NULL"
        TEXT vendor "NOT NULL"
        TEXT type "NOT NULL"
        TIMESTAMP created_at "DEFAULT CURRENT_TIMESTAMP"
    }

    device_api {
        INTEGER id PK "AUTOINCREMENT"
        INTEGER compute_api_id FK "NOT NULL"
        INTEGER version_major "nullable"
        INTEGER version_minor "nullable"
        TEXT extensions "nullable"
    }

    tuning_source {
        INTEGER id PK "AUTOINCREMENT"
        TEXT source_fingerprint UK "NOT NULL"
        TIMESTAMP created_at "DEFAULT CURRENT_TIMESTAMP"
    }

    tuning_space {
        INTEGER id PK "AUTOINCREMENT"
        INTEGER source_id FK "NOT NULL"
        TEXT parameter_fingerprint "NOT NULL"
        TEXT space_fingerprint "NOT NULL"
        TIMESTAMP created_at "DEFAULT CURRENT_TIMESTAMP"
    }

    tuning_run {
        INTEGER id PK "AUTOINCREMENT"
        BLOB guid UK "NOT NULL"
        INTEGER space_id FK "NOT NULL"
        INTEGER device_id FK "NOT NULL"
        INTEGER device_api_id FK "NOT NULL"
        INTEGER output_format_id FK "NOT NULL"
        TIMESTAMP created_at "DEFAULT CURRENT_TIMESTAMP"
        TEXT input_data "nullable"
        TEXT device_identifier "nullable"
    }

    tuning_result {
        INTEGER id PK "AUTOINCREMENT"
        INTEGER run_id FK "NOT NULL"
        INTEGER duration "NOT NULL"
        TEXT result "NOT NULL"
    }

    compute_api   ||--o{ device_api    : "compute_api_id"
    tuning_source ||--o{ tuning_space  : "source_id"
    tuning_space  ||--o{ tuning_run    : "space_id"
    device_info   ||--o{ tuning_run    : "device_id"
    device_api    ||--o{ tuning_run    : "device_api_id"
    output_format ||--o{ tuning_run    : "output_format_id"
    tuning_run    ||--o{ tuning_result : "run_id"
```

## Notes

- `compute_api` and `output_format` are seeded lookup tables:
  - `compute_api`: `OpenCL` (1), `CUDA` (2), `Vulkan` (3), `Cpp` (4)
  - `output_format`: `JSON` (1), `JSON_T4` (2), `XML` (3)
- Unique constraints (beyond the marked `UK` columns):
  - `device_info` — unique on `(name, vendor, type)`
  - `device_api` — unique on `(compute_api_id, version_major, version_minor, extensions)`
  - `tuning_space` — unique on `(space_fingerprint, parameter_fingerprint, source_id)`
- A `tuning_run` is uniquely identified across databases by its `guid`, which is what
  `SyncFromFile` uses to skip already-imported runs.
- `tuning_run.device_identifier` is the persistent hardware identifier of the device the run executed on
  (the device UUID reported by the compute API, e.g. `GPU-<uuid>` for CUDA). It originates from
  `ktt::DeviceInfo::GetDeviceIdentifier()` and is carried through `ktt::db::DeviceInfo::deviceIdentifier`. It is
  nullable: the C++/CPU backend and devices whose driver does not expose a UUID leave it `NULL`. It is preserved
  across databases by `SyncFromFile`.
