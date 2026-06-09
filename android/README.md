# Android Build Flow

This repository now includes an Android Gradle app template under `android/` and a custom SDL activity in:

- `android/app/src/main/java/org/libsdl/app/GeraNESActivity.java`

`build.sh android` generates a Gradle project inside your Android build directory, wires it to the top-level `CMakeLists.txt`, bundles runtime assets, and builds either:

- a signed or unsigned `.apk`
- a signed or unsigned `.aab`

## JSONC config file

Android builds now require:

- `android/build-config.jsonc`

Start from:

- `android/build-config.example.jsonc`

This uses JSONC rather than strict JSON so each field can have inline comments.

Resolution order is:

- `android/build-config.jsonc`
- script defaults

## Native Android picker support

`GeraNESActivity` also provides the Android document picker bridge used by the native app:

- ROM, replay, symbol, and ZIP mod file pickers use Android's native file picker
- mod folders use Android's native folder picker
- replay saves and PNG exports use Android's native create-document picker

Selected documents are copied into app-internal cache when the native code still expects normal filesystem paths.

## Release signing

The JSONC file supports:

- `signing.keystorePath`
- `signing.keystorePassword`
- `signing.keyAlias`
- `signing.keyPassword`

If you omit them, `build.sh android` can still build release artifacts, but they may be unsigned and not publishable to Google Play.

## Package selection

- `"packageFormat": "apk"`
- `"packageFormat": "aab"`

## Optional metadata fields

JSONC fields:

- `appName`
- `applicationId`
- `namespace`
- `versionCode`
- `versionName`
- `compileSdk`
- `targetSdk`
- `abis`
- `api`
- `stl`
- `iconPath`
- `iconForegroundPath`
- `iconBackgroundPath`
- `iconMonochromePath`

`iconPath` configures a legacy single-image launcher icon.

For modern adaptive icons, set:

- `iconForegroundPath`
- `iconBackgroundPath`
- optionally `iconMonochromePath`

When adaptive icon fields are present, they take precedence over `iconPath`.

## Runtime assets

The Android activity copies bundled `data/` assets, and bundled `docs/` if present, into:

- `<internal-storage>/runtime_data`

The native Android startup path switches the current working directory there so the existing runtime file loading logic can keep using normal paths.
