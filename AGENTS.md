# AGENTS.md

## Project at a glance
- KTT is a C++17 shared library for GPU kernel autotuning across OpenCL/CUDA/Vulkan; public API entry point is `Source/Ktt.h` and `Source/Tuner.h`.
- Runtime flow is layered: `Tuner` (public facade, exception-to-log handling) -> `TunerCore` (orchestration) -> `KernelManager`/`KernelArgumentManager` + `KernelRunner` + `TuningRunner` + backend `ComputeEngine` (`Source/Tuner.cpp`, `Source/TunerCore.cpp`).
- Backend-specific implementations live under `Source/ComputeEngine/{OpenCl,Cuda,Vulkan}` and are selected by `ComputeApi` at `TunerCore` initialization.
- Offline/online tuning behavior is implemented in `Source/TuningRunner/TuningRunner.cpp`; actual launches/validation/buffer lifecycle are in `Source/KernelRunner/KernelRunner.cpp`.

## Build and test workflows (project-specific)
- Canonical build system is **Premake** (`premake5.lua`, `Readme.md`).
- Standalone CMake builds are not an officially supported workflow for this project; use Premake for reproducible builds.
- CLion's CMake integration can still work well for development in this repository.
- Linux flow (from repo root): `./premake5 gmake --no-tutorials --no-opencl` -> `cd Build` -> `make`.
- Enable unit tests explicitly: regenerate with `--tests`, then build `Tests` target (`premake5.lua`, `Tests/Main.cpp`).
- Optional features are build-time flags in Premake (`--python`, `--tuning-loader`, `--profiling=...`, `--vulkan`, `--no-opencl`, `--no-cuda`).
- `--tuning-loader` requires `--python` (enforced in `premake5.lua`).

## Conventions and patterns to follow
- Error model in public API: methods in `Tuner` catch `KttException`, log via `TunerCore::Log()`, and return fallback values (`Invalid*Id`, empty vectors, default result).
- IDs and handles are string/integer aliases in `KttTypes`; use `InvalidKernelId`, `InvalidArgumentId`, etc. instead of magic values.
- Composite kernels must define a launcher; default launcher only exists for single-definition kernels (`KernelRunner::GetKernelLauncher`).
- Read-only argument caching is enabled by default; it affects upload/cleanup behavior in tuning loops (`KernelRunner::SetupBuffers`, `CleanupBuffers`).
- Logging/time are centralized utilities: `Tuner::SetLoggingLevel`, `SetLoggingTarget`, `SetTimeUnit` (`Source/Tuner.cpp`, `Source/Output/TimeConfiguration`).
- Formatting conventions are strict LLVM-derived with 4-space indentation, Allman-style braces, 120-column limit (`.clang-format`).

## Integration points and cross-component communication
- Python bindings are behind `KTT_PYTHON`; module entry is `Source/Python/PythonModule.cpp` (`pyktt`).
- JSON tuning-script pipeline is in `TuningLoader/`; it validates schema, deserializes commands, then executes by priority (`TuningLoader.cpp`).
- Database-backed reuse path exists: `TuneWithDbCheck()` computes fingerprints and checks SQLite record cache before tuning (`Source/TunerCore.cpp`, `Source/Database/*`).
- Result serialization/deserialization supports JSON, JSON_T4, XML (`TunerCore::CreateSerializer/CreateDeserializer`).

## High-value files to read first
- API surface: `Source/Ktt.h`, `Source/Tuner.h`, `OnboardingGuide.md`.
- Core orchestration: `Source/Tuner.cpp`, `Source/TunerCore.cpp`.
- Tuning execution path: `Source/TuningRunner/TuningRunner.cpp`, `Source/KernelRunner/KernelRunner.cpp`.
- Build matrix and feature flags: `premake5.lua`, `Readme.md`.
- Loader and scripting integration: `TuningLoader/TuningLoader.cpp`, `Source/Python/PythonModule.cpp`.
- Unit-test style/examples: `Tests/Main.cpp`, `Tests/KernelManagerTests.cpp`.
