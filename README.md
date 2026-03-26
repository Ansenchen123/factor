# Factor Manager

`Factor Manager` is a desktop maintenance-tracking tool for factory or production-line equipment.

It is designed as a practical operations utility rather than a toy CRUD demo. The application lets operators:

- manage production lines and equipment groups
- define recurring maintenance items with day-based intervals
- mark checks as completed for the current day
- preview due and overdue items through a simulation date mode
- persist maintenance data locally in JSON format

## Portfolio Value

This project showcases:

- a modernized C++17 desktop application structure
- local JSON persistence
- raylib/raygui-based GUI development
- scheduling logic for due and overdue maintenance workflows
- a reproducible CMake build pipeline
- a clear separation between UI, domain model, and storage logic

## Feature Highlights

- Production line management
  Add, rename, duplicate, and remove lines
- Equipment management
  Group assets under each production line
- Maintenance item tracking
  Define interval-based checks and mark them complete
- Dashboard summary
  View due-today and overdue maintenance items
- Simulation mode
  Preview future maintenance status without editing persisted records
- Language packs
  Load UI text from locale JSON files with built-in English and Traditional Chinese

## Architecture

The application is intentionally split into a few small layers so the codebase reads like a real maintainable product instead of a one-file prototype:

- `src_cpp/main.cpp`
  Desktop UI flow, panel rendering, and user actions
- `app_include/i18n.hpp` and `src_cpp/i18n.cpp`
  Locale file loading and runtime language switching
- `app_include/model.hpp`
  Domain entities such as production lines, equipment, and maintenance items
- `app_include/storage.hpp` and `src_cpp/storage.cpp`
  JSON persistence, sample dataset generation, and scheduling calculations
- `src_cpp/raygui_bridge.cpp`
  Third-party GUI bridge implementation needed by raygui on this toolchain

That structure makes it much easier to extend the project with reporting, filtering, or authentication later.

## Tech Stack

- C++17
- raylib
- raygui
- cJSON
- CMake

## Build

Requirements:

- CMake 3.20+
- MinGW g++
- `raylib.dll` is copied into the build output automatically on Windows

From the project root:

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

## Run

```bash
build/factor_manager.exe
```

The app reads and writes all maintenance records from `data/maintenance/database.json`.

## Release Package

If you want a portable Windows build, package the app with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package_release.ps1 -Version v1.0.0
```

The generated package includes:

- `factor_manager.exe`
- `raylib.dll`
- the full `data/` folder
- `README.md`
- `LICENSE`

This means the app is not a single-file executable. The release package must keep `exe`, `dll`, and `data/` together.

## What Makes This Resume-Worthy

- It solves a believable real-world operations problem instead of being a generic todo app.
- The code demonstrates desktop GUI work, persistence, date logic, and project packaging.
- The repository is organized to be cloneable, buildable, and understandable by another engineer.

## Controls

- Click a line to inspect its equipment
- Click an equipment entry to inspect maintenance items
- Use `Done` to mark an item checked for the current date
- Enable simulation mode in the dashboard to preview future scheduling

## Project Layout

- `src_cpp/`: modern C++ application logic
- `app_include/`: domain model and storage interfaces
- `data/locales/`: UI language packs
- `src/cJSON.c`: bundled JSON dependency
- `include/`: third-party GUI headers
- `data/`: persisted local application data

## Notes

- Simulation mode is intentionally read-only for data editing, so it does not accidentally rewrite live records.
- The sample dataset is included so reviewers can launch the app and immediately see realistic maintenance states.
- Old build artifacts, logs, binaries, and packaged outputs are intentionally ignored by git.
