# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

WinMerge is an open-source differencing and merging tool for Windows. It compares files and folders, presenting differences visually. It supports 2-way and 3-way file comparison, folder comparison, image comparison, hex comparison, and web page comparison. Written in C++ using MFC (Microsoft Foundation Classes), targeting Windows with Unicode support.

## Build Commands

### Full Build (compile + test + installer + archive)
```
DownloadDeps.cmd                          # First time only: downloads prebuilt dependencies
BuildAll.vs2022.cmd [x86|x64|ARM64]       # VS2022
BuildAll.cmd [x86|x64|ARM64]              # Auto-detect VS version
```

### Compile Only
```
BuildBin.cmd [x86|x64|ARM64]
```
This runs `MSBuild WinMerge.sln /t:Rebuild /p:Configuration="Release"`.

### Run Unit Tests
```
Testing\GoogleTest\UnitTests\UnitTests.exe
```
Tests use Google Test 1.14.0. The test executable is built as part of `WinMerge.sln`. To run from VS, set UnitTests as startup project.

### Static Analysis
```
runcppcheck.cmd                           # cppcheck with C++17, MFC/Windows/ATL libraries
```

### Format Code
```
runastyle.bat                             # Artistic Style formatter
```

### Output Binary Location
```
Build\{Platform}\{Configuration}\WinMergeU.exe
# e.g., Build\X64\Release\WinMergeU.exe
```

## Solution Files

- **`WinMerge.sln`** - Main development solution (Merge app, UnitTests, GUITests)
- **`ALL.sln`** - Full solution including plugins, ShellExtension, all subprojects
- VS2017 variants: `WinMerge.vs2017.sln`, `ALL.vs2017.sln`

Build configurations: Debug, Release, Test. Platforms: Win32, x64, ARM, ARM64.

## Architecture

### MFC Document/View Pattern

The application uses MFC's MDI (Multiple Document Interface) with Doc/View architecture. Each comparison type has its own Document, View, and Frame triplet:

| Type | Document | View | Frame |
|------|----------|------|-------|
| File compare | `MergeDoc` | `MergeEditView` | `MergeEditFrm` |
| Folder compare | `DirDoc` | `DirView` | `DirFrame` |
| Hex compare | `HexMergeDoc` | `HexMergeView` | `HexMergeFrm` |
| Image compare | - | - | `ImgMergeFrm` |
| Web page compare | - | - | `WebPageDiffFrm` |

- **`Merge.cpp/h`** (`CMergeApp`) - Application entry point, manages document templates, options, filters
- **`MainFrm.cpp/h`** (`CMainFrame`) - Main MDI frame window, defines frame types (FRAME_FOLDER, FRAME_FILE, FRAME_HEXFILE, FRAME_IMGFILE, FRAME_WEBPAGE)

### Diff/Compare Engine Layer (`Src/CompareEngines/`)

Pluggable comparison backends orchestrated by `DiffWrapper`:
- `Wrap_DiffUtils` - GNU diffutils (primary text diff engine)
- `BinaryCompare` - Binary file comparison
- `ByteCompare` / `ByteComparator` - Byte-level comparison
- `ImageCompare` - Image comparison (via WinIMerge)
- `TimeSizeCompare` - Quick comparison by timestamp/size
- `ExistenceCompare` - File existence check only

### Diff Infrastructure

- `DiffWrapper` - Main orchestrator for comparisons
- `DiffContext` - Context/state for diff operations
- `DiffThread` - Background threading for diffs
- `DiffList` / `DiffItem` - Difference data structures
- `stringdiffs` - Inline word/character-level differences within lines

### Folder Comparison

- `DirScan` - Directory scanning and traversal
- `FolderCmp` - Folder comparison logic
- `DirWatcher` - File system change monitoring
- `RenameMoveDetection` - Detecting renamed/moved files across folders

### Filter System

- `FileFilter` / `FileFilterMgr` / `FileFilterHelper` - File-level filters
- `LineFiltersList` - Line-level regex filters (ignore matching lines)
- `SubstitutionFiltersList` - Regex substitution filters
- `Src/FilterEngine/` - New expression-based filter engine (uses re2c lexer + Lemon parser)

### Plugin System

Plugins use COM/OLE Automation. Written in C++, VBScript, or as Windows Script Components (`.sct`).
- `Plugins.cpp/h` - Plugin loading and management
- `InternalPlugins.cpp/h` - Built-in plugins
- `FileTransform.cpp/h` - File transformation pipeline via plugins

### Editor Component

The text editor is Crystal Edit (`Externals/crystaledit/`), a customized syntax-highlighting editor control with its own parsers for 40+ languages in `Src/editlib/parsers/`.

### Options System (`Src/Common/`)

- `OptionsMgr` - Abstract options interface
- `RegOptionsMgr` - Registry-backed storage
- `IniOptionsMgr` - INI file-backed storage (portable mode)

## Code Style

- **Main source** (`Src/`, `Src/Common/`, `Src/CompareEngines/`, `ShellExtension/`): **Allman style, tab indentation**
- **Editor code** (`Src/editlib/`): **GNU style, 2-space indentation**
- Both use: `--pad-oper --unpad-paren`
- C++17 standard

## Key External Dependencies

| Library | Purpose |
|---------|---------|
| POCO (Foundation + XML) | Utilities and XML processing |
| Boost 1.88.0 | Header-only C++ utilities |
| Google Test 1.14.0 | Unit testing |
| Crystal Edit | Text editor component (in `Externals/crystaledit/`) |
| WinIMerge | Image comparison (submodule) |
| WinWebDiff | Web page comparison via WebView2 (submodule) |
| frhed | Hex editor (submodule) |
| 7-Zip / Merge7z | Archive support |
| darkmodelib | Windows dark mode |

## Testing

Unit tests are in `Testing/GoogleTest/UnitTests/` with test files organized by component (e.g., `BinaryCompare/`, `DiffWrapper/`, `FileFilter/`, `StringDiffs/`, `Plugins/`). GUI tests are in `Testing/GoogleTest/GUITests/`. The `BuildAll` scripts gate installer creation on unit test success.
