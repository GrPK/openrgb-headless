# OpenRGB Headless

A headless fork of [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) that strips
the entire Qt dependency (Qt5Widgets, Qt5Gui, Qt5Core, Qt5DBus) and ships only the
SDK server. The result is a small, GUI-less binary that exposes the OpenRGB SDK over
its standard TCP port (default `6742`) and nothing else: no GUI, no plugin loader,
no system tray, no installer-bundled `.dll`s for desktop frameworks.

The device controllers and the SDK protocol are unchanged from upstream.

| Build | Size |
|---|---|
| Upstream OpenRGB Windows portable | ~13 MiB download / ~25 MiB extracted |
| This fork (Windows x64, with hidapi/libusb/PawnIO DLLs + MSVC runtime) | **~9 MiB** |
| This fork (Linux x64, dynamic) | **~11 MiB** |
| This fork (macOS arm64, dynamic, Homebrew dylibs) | **~8.9 MiB** |

## Why

Upstream OpenRGB has [an open feature request for a headless mode](https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/2012)
that has been pending for years. This fork implements it as the smallest possible
patch on top of upstream. It exists for embedding OpenRGB inside other services as
a child process - the host speaks the SDK over loopback TCP and gets full
device-control access without bundling 12+ MiB of Qt runtime DLLs the SDK server
never actually uses.

The fork stays close to upstream so it can keep merging upstream device-controller
work, hardware detector improvements, and SDK protocol fixes. See
[`MAINTAINING.md`](MAINTAINING.md) for the upstream-sync workflow.

## What was removed vs. upstream

Everything that exists only to drive a graphical interface:

- `qt/` - every dialog, page, model, dialog .ui form, theme, font, icon, translation
- `dependencies/ColorWheel/` - pure `QWidget` colour-picker, only used by the GUI
- `PluginManager.cpp/h` and `OpenRGBPluginInterface.h` - the GUI plugin loader (uses
  `QPluginLoader` and the `QWidget` plugin ABI)
- `SuspendResume/` - every per-platform listener, all of which depended on
  `QAbstractNativeEventFilter` (Windows) or `QDBusConnection` (Linux/FreeBSD).
  The host that embeds the headless server is expected to detect OS power events
  itself and bounce the subprocess on resume.
- `Documentation/Images/` - GUI screenshots
- All Linux desktop / icon / AppStream metainfo / systemd / tmpfiles install rules
- The Windows `.exe` icon (`RC_ICONS`) - headless tools don't need one
- The macOS `.app` bundle config (Info.plist, .icns) - headless tools aren't bundles
- All translation files (`qt/i18n/*.ts`) and the `lrelease` / `embed_translations`
  qmake configs

## What stayed

Everything that's not GUI-bound:

- Every device controller under `Controllers/`
- The full OpenRGB SDK TCP protocol (`NetworkServer.cpp`, `NetworkProtocol.cpp`)
- All hardware detectors (`i2c_smbus/`, `hidapi_wrapper/`, `serial_port/`,
  `interop/`, `SPDAccessor/`)
- `ResourceManager`, `SettingsManager`, `ProfileManager`, `LogManager` - all
  Qt-free in upstream
- The `cli.cpp` CLI parser - flags that don't make sense headless
  (`--gui`, `--start-minimized`, `--client`) are accepted but ignored
- Cross-platform: CI builds and ships Windows and Linux binaries; macOS (arm64) builds from source (see Building)
- The qmake build system - kept as-is so upstream merges remain straightforward

## Fork additions

- **Reentrant controller lock**: a controller's state mutex may be re-acquired
  by a thread that already holds it. Upstream's `std::shared_mutex` deadlocks a
  controller whose LED update calls a locking getter while a network update
  waits for the lock (seen on ENE DRAM under a 30 fps stream).
- **No lost listen-thread wakeups**: the SDK server's per-controller threads
  can no longer miss a wakeup, which left a packet waiting for the next one to
  that controller, or hung a rescan in `join()` and stalled every client.
- **Detection flow**: every detector is called directly, exactly as upstream
  does, with no timeout, worker thread or exception wrapper. The only addition
  is tagging each detected controller with its detector's name for the
  detector map.
- **Detector map**: at the end of every detection pass, and whenever a
  controller is added or removed (hotplug), the server writes
  `detector-map.json` into the configuration directory, mapping each detected
  device name to the `REGISTER_*_DETECTOR` name that produced it. The SDK
  device description carries only the device name, and the two differ wherever
  a table-driven detector registers one generic string and emits per-model
  names (`Corsair DRAM` produces `Corsair Vengeance RGB DDR5`), so without the
  map an SDK client cannot work out which key to write into `detectors`. First
  writer wins on a duplicate device name.

  ```json
  { "version": 1, "devices": { "Corsair Vengeance RGB DDR5": "Corsair DRAM" } }
  ```

- **Commander Core cooling over the SDK**: two packets outside the upstream ID
  range, `NET_PACKET_ID_NEXUS_COOLING_GET_STATUS` and
  `NET_PACKET_ID_NEXUS_COOLING_SET_DUTIES`, addressed by controller ID from
  protocol 6. A controller that implements `NexusCoolingDevice` (`NexusCooling.h`)
  answers them; every other controller replies with an unsupported ACK. The
  Commander Core controller owns the device, so cooling transactions (fan and
  pump RPM, temperatures, a software duty table) run on its worker thread under
  the same controller lock as lighting, and the SDK calls only read or replace
  cached state. The payload layouts are documented in
  `Controllers/CorsairCommanderCoreController/CorsairCommanderCoreCooling.h`;
  `tests/CorsairCommanderCoreCoolingTest.cpp` checks them.

## License

OpenRGB is licensed under the **GNU General Public License, version 2 or later**
(GPL-2.0-or-later). This fork inherits the same license. The
[`LICENSE`](LICENSE) file in the repo root applies to the entire codebase.

The original upstream copyright belongs to Adam Honse (CalcProgrammer1) and
contributors. The headless patches are visible in [the diff against upstream
master](https://github.com/hello-nexus/openrgb-headless/compare/main...headless).

## Building

### Windows (MSVC + qmake/jom)

```cmd
qmake OpenRGB.pro CONFIG+=release
jom -j %NUMBER_OF_PROCESSORS%
```

You still need a Qt5 install for `qmake` itself (it's the build tool), but the
resulting `OpenRGB.exe` links zero Qt libraries.

The exe (and the hidapi/libusb DLLs) link the MSVC runtime dynamically, so the
machine needs a Visual C++ redistributable at least as new as the toolset that
built them; an older `msvcp140.dll` crashes the daemon before `main`. The CI
artifact sidesteps this by shipping `msvcp140.dll`, `msvcp140_codecvt_ids.dll`,
`vcruntime140.dll` and `vcruntime140_1.dll` next to the exe (app-local). For a
local build, copy them from `%VCToolsRedistDir%\x64\Microsoft.VC*.CRT\`.

### Linux

```bash
sudo apt install qtbase5-dev libusb-1.0-0-dev libhidapi-dev libmbedtls-dev pkg-config build-essential
qmake OpenRGB.pro CONFIG+=release
make -j$(nproc)
```

Verify the binary is Qt-free:

```bash
ldd ./openrgb | grep -i qt
# (should produce no output)
```

### macOS (Homebrew + qmake)

```bash
brew install qt@5 libusb hidapi mbedtls@3 pkg-config
export PATH="$(brew --prefix qt@5)/bin:$PATH"
qmake OpenRGB.pro CONFIG+=release
make -j$(sysctl -n hw.ncpu)
```

The macOS build produces a plain Mach-O console binary (not a `.app`
bundle). Verify it links no Qt frameworks:

```bash
otool -L ./openrgb | grep -i qt
# (should produce no output)
```

CI covers Windows and Linux only; the macOS (arm64) binary is built from source with the steps above. x86_64 macOS is not
currently exercised by CI but the source paths are preserved.

## Running

```
OpenRGB --server --server-port 6742 --noautoconnect
```

The TCP SDK server listens on the port and accepts clients. There is no GUI, no
window, no tray icon - it is a pure background process. `--noautoconnect`
prevents auto-connecting to a remote OpenRGB instance and is the right default
for an embedded server.

CLI flags supported:

| Flag | Effect |
|---|---|
| `--server` | Run the SDK server (always implied in this fork) |
| `--server-port <n>` | Port to listen on (default 6742) |
| `--noautoconnect` | Don't auto-connect to a remote OpenRGB instance |
| `--nodetect` | Skip device detection |
| `--config <dir>` | Configuration directory (defaults to platform-specific) |
| `--profile <name>` | Load a profile at startup |
| `--list-devices` | Print detected devices and exit |
| `--loglevel <level>` | `fatal` / `error` / `message` / `verbose` / `debug` / `trace` |

GUI-only flags (`--gui`, `--start-minimized`, `--client`) are accepted but ignored.

## Maintaining

See [`MAINTAINING.md`](MAINTAINING.md) for the upstream-sync workflow. The short
version: track `upstream/master`, merge it into `main`, then merge `main` into
`headless`. Re-deletion of any GUI files upstream may have re-added is the only
recurring manual step.

## Credits

All upstream code © Adam Honse (CalcProgrammer1) and the OpenRGB contributors.
Headless build patches by [hello-nexus](https://github.com/hello-nexus).
