# Windows KDE Craft build

## Known-good environment

- Craft root: `C:\CraftRoot`
- ABI: `windows-gcc-x86_64`
- Local source: `C:\Users\Hugo\kdenlive`
- Qt 6.11.1, KDE Frameworks 6.29.0, MLT 7.41.0
- GCC 14.2.0, CMake 4.1.4, Ninja 1.13.2

## Build from the local fork

Open PowerShell, activate Craft, and tell the Kdenlive blueprint to use the
working tree rather than downloading another source copy:

```powershell
. 'C:\CraftRoot\craft\craftenv.ps1'
craft --options 'kdenlive.srcDir=C:\Users\Hugo\kdenlive' kdenlive
```

The Kdenlive blueprint enables `BUILD_TESTING=ON` in this environment. The
verified build directory for this checkout is `C:\_\3377f5a\build`; Craft may
choose a different hashed directory after a clean rebuild.

## Run the AI plan contract test

```powershell
. 'C:\CraftRoot\craft\craftenv.ps1'
ctest --test-dir 'C:\_\3377f5a\build' -R '^aieditorplannertest$' --output-on-failure
```

Verified result on 2026-09-10: 1/1 passed, 0 failed.

## Runtime

The installed development build is `C:\CraftRoot\bin\firawynix-kdenlive.exe`. Activate
the Craft environment before launching it so all DLL and data paths resolve.

Craft can warn that Windows Developer Mode is disabled. That warning only means
archive extraction cannot use fast symbolic links; it did not prevent the build
or test from succeeding. Enabling Developer Mode remains an optional
administrator-level optimization.

## GitHub runner short paths

Entry: `.github/workflows/build-windows.yml:Bootstrap KDE Craft`

- Ordinary Craft junction short paths are disabled because Autotools-generated
  makefiles can retain a stale junction after Craft recreates it.
- The Qt 6 recipe separately calls `CraftShortPath.createSubstShortPath()`.
  It requires `[ShortPath] DriveLetter = Z:/` in the bootstrapped
  `CraftSettings.ini` before `qtbase` unpacking; disabling junctions alone does
  not remove this requirement.
- Run 35782592871 failed at `qtbase` unpack with "Failed to find [ShortPath]
  DriveLetter". The workflow now sets that option after bootstrap, while
  leaving ordinary junctions disabled.

Updated: 2026-09-22
