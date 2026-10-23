#documentation #windows #installer

# Windows Installer

The Windows installer is built with NSIS (Nullsoft Scriptable Install System) using the script at `scripts/installer.nsi`. The portable zip is the same `build/bin` tree without the test executable.

Workflow wiring: [CI/CD Guide](ci-cd-guide.md).

**Quick Setup:** a Release build with the app enabled fills `build/bin`. CI then runs `windeployqt`, copies remaining UCRT64 DLLs, and invokes `makensis`.

## How It Works

- The app target runs `windeployqt` (or `windeployqt6`) after link. That copies Qt DLLs and `platforms/qwindows.dll` next to `time-tracker.exe`.
- `scripts/collect_windows_runtime.sh` copies any remaining DLLs that `ldd` still resolves under `/ucrt64/bin/`.
- `scripts/verify_windows_runtime.sh` fails the job when `time-tracker.exe`, the platform plugin, or a top-level DLL is missing a dependency.
- The test binary `time_tracker_core_tests.exe` is deleted before DLL collection, so the zip and the installer ship the app only.
- CI invokes `makensis -DAPP_VERSION="${VERSION}" scripts/installer.nsi`.
- `APP_VERSION` is embedded in the installer window title, branding, and registry entries.
- If `APP_VERSION` is not provided (a local manual run), it defaults to `localbuild`.

## Artifacts

| Artifact | Where |
|---|---|
| `time-tracker-windows-<version>.zip` | Portable exe, Qt DLLs, and MinGW runtime. Uploaded on feature runs. Attached to the master GitHub release. |
| `time-tracker-windows-installer-<version>` | NSIS installer uploaded on feature runs. |
| `time-tracker-windows-installer-<version>.exe` | Same installer, attached to the master GitHub release. |

Feature `<version>` is `<branch>-<sha>`. Master `<version>` is the semver from [versioning.md](versioning.md).

## Installed Components

- **Core files (required)**: everything left in `build/bin/`, including `time-tracker.exe`, Qt plugins, and runtime DLLs
- **Start Menu shortcuts**: optional
- **Desktop shortcut**: optional

## Registry

The installer writes `HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\Time Tracker` for Add/Remove Programs, and `HKLM\...\App Paths\time-tracker.exe` so the exe is findable via `PATH`.

## Uninstaller

The uninstaller removes the install directory, Start Menu entries, desktop shortcut, and those registry keys.

## Local Installer

From an MSYS2 UCRT64 shell, after a Release build that produced `build/bin/time-tracker.exe` and `build/bin/platforms/qwindows.dll`:

```bash
makensis -DAPP_VERSION=0.1.0 scripts/installer.nsi
```

The installer is written to `time-tracker-windows-installer.exe` in the repository root. NSIS comes from the MSYS2 package `mingw-w64-ucrt-x86_64-nsis`.
