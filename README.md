# Warudo RhyLive Calibrate for OBS

Recalibrate RhyLive from one button directly below **Settings** in the OBS **Controls** panel.

[한국어](README.ko.md) · [Multilingual quick start](docs/QUICKSTART.md)

No separate control window or background helper is required. Warudo must remain running on the same PC.

## Features

- A native OBS button, with the same sizing and theme as the existing controls.
- Automatic language selection from OBS: English, Korean, Japanese, Simplified Chinese, Traditional Chinese, German, French, Spanish, and Brazilian Portuguese.
- English fallback for unsupported languages; language-family fallback for regional variants.
- Local API authentication without asking you to paste a token.
- Success and error messages on the button and in the OBS status bar.
- One self-contained plugin DLL; translations are embedded at build time.

## Compatibility

| Item | Target |
| --- | --- |
| Operating system | Windows x64 |
| OBS build target | OBS Studio 33.0.0 |
| Qt build target | Qt 6.11.1, MSVC x64 |
| Warudo integration | Local HTTP/WebSocket API and RhyLive Receiver |
| Receiver selection | Exactly one active RhyLive Receiver in the current scene |

Other OBS or Qt versions have not been verified. The Warudo integration uses editor protocol details that may change between Warudo versions. Build, localization, widget placement, and isolated API scenarios are tested; calibration in a live Warudo session still needs validation before a stable release.

## Install

1. Fully close OBS.
2. Extract the Windows release ZIP into your OBS installation directory, preserving `obs-plugins/64bit/warudo-calibrate.dll`.
3. Start OBS and Warudo, then load a Warudo scene with one active RhyLive Receiver.
4. Face forward, hold your head still, and click **Recalibrate** below **Settings**.

For a standard OBS installation, the DLL destination is:

```text
C:\Program Files\obs-studio\obs-plugins\64bit\warudo-calibrate.dll
```

For a custom installation, use that installation's `obs-plugins\64bit` directory. No translation files or Qt DLLs need to be copied.

To update, close OBS and replace the DLL. To uninstall, close OBS and remove only `warudo-calibrate.dll`.

## Troubleshooting

| Message or symptom | What to check |
| --- | --- |
| Button is missing | Confirm the DLL is in the active OBS installation, restart OBS, and check the OBS log for plugin loading errors. Safe mode may disable third-party plugins. |
| Warudo unavailable | Start Warudo on the same PC and wait for its scene to load. |
| Authentication error | Restart Warudo, then try again. The plugin reads the current launch's token from the local Warudo log. |
| No RhyLive receiver | Open a scene containing an active RhyLive Receiver. |
| Select one RhyLive receiver | Leave exactly one RhyLive Receiver active in the current scene. |
| Unexpected language | Restart OBS after changing its language. Unsupported languages use English. |

## Build from source

Install Visual Studio 2022 with the Desktop development with C++ workload, a Windows SDK, CMake 3.22 or newer, Python 3.10 or newer, and Qt **6.11.1 MSVC x64**. The GitHub workflow uses the official OBS Qt development archive with a pinned checksum.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64"
cmake --build build --config Release --parallel
$env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;" + $env:PATH
ctest --test-dir build -C Release --output-on-failure
```

The plugin is `build/Release/warudo-calibrate.dll`. Alternatively, use `scripts/build.ps1 -QtRoot C:\Qt\6.11.1\msvc2022_64`.

The test program runs offscreen. Integration tests create temporary loopback servers on OS-assigned ports and use a synthetic log/token. They do not connect to a running OBS or Warudo instance.

To prepare source and Windows release ZIPs after building:

```powershell
python scripts/package.py build/Release/warudo-calibrate.dll dist
```

See [validation details](docs/VALIDATION.md) for the tested scope and current limitations.

## Translate

Copy `locale/en-US.json` to a new OBS locale filename and translate every value while preserving the keys. Rebuild to embed it. The generator rejects missing keys, duplicate keys, and blank values. Locale tests cover regional fallbacks and button placement.

## Local connection details

The plugin only connects to `localhost:19052` and `localhost:19053`. It reads the current session token from `%LOCALAPPDATA%\..\LocalLow\HakuyaLabs\Warudo\Player.log`, fetches the current scene, checks the receiver's advertised trigger metadata, and invokes the RhyLive Receiver's `TriggerCalibrate` editor trigger. It does not upload logs, store the token, save the Warudo scene, or change tracking settings.

No OBS SDK or Warudo assemblies are distributed in this repository. Runtime Qt libraries come from the user's OBS installation.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE). This is an independent project and is not affiliated with OBS Project or the Warudo developers.
