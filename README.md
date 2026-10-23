SIXERC

A native Windows application by Anonymous-beta (Chinedu).

SIXERC is being rebuilt around a native Windows dashboard, explicit local operations, transparent diagnostics, and a dependable automated build pipeline.

«Current milestone: Native dashboard and Windows build pipeline.»

Project information

Property| Value
Project| SIXERC
Creator| Anonymous-beta (Chinedu)
Codename| Ekwensu's Grasp
Language| C++20
Platform| Windows x64
Build system| CMake
CI| GitHub Actions
Interface| Native Win32

Current features

- Native Windows desktop window.
- Dashboard and application navigation.
- Module inventory with explicit status labels.
- In-session activity log.
- Local Windows and filesystem diagnostics.
- Diagnostic report export.
- Version and creator information.
- Automated Windows x64 builds.
- Downloadable executable artifacts.

Security and transparency

The current dashboard is intentionally separated from legacy operational components.

It does not connect the interface to browser credential collection, session impersonation, covert data transfer, persistence installation, anti-analysis evasion, or destructive cleanup.

A module is not considered operational merely because its source file exists. Functional status must be established through implementation review and testing.

Build requirements

To build locally, install:

- Windows 10 or Windows 11.
- Visual Studio Build Tools with the C++ desktop workload.
- A Windows SDK.
- CMake 3.20 or newer.

From a Visual Studio Developer PowerShell:

cmake -S . -B build -A x64
cmake --build build --config Release --parallel

The executable is produced at:

build/Release/SIXERC.exe

Downloading a build

1. Open the repository's Actions tab.
2. Select the latest successful Build SIXERC run.
3. Open the run's artifacts.
4. Download "SIXERC-Windows-x64".
5. Extract the artifact and launch "SIXERC.exe".

Release builds are published when a version tag beginning with "v" is pushed.

Repository layout

sixerc/
├── .github/
│   └── workflows/
│       └── build.yml
├── src/
│   ├── main.cpp
│   ├── core/
│   ├── anti/
│   └── modules/
├── CMakeLists.txt
├── README.md
└── LICENSE

The current dashboard build compiles "src/main.cpp" as the application entry point. Legacy source files remain outside the active dashboard build and are not connected to the interface.

Development principles

- Prefer explicit, observable behavior over hidden background actions.
- Do not report a component as working without a corresponding implementation and test.
- Keep diagnostic information local unless the user explicitly exports it.
- Use secure compiler defaults.
- Keep the build reproducible.
- Document limitations honestly.
- Avoid unnecessary dependencies and decorative complexity.

Maintainer

Anonymous-beta (Chinedu)

SIXERC — Ekwensu's Grasp.