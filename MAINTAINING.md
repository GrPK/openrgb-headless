# Maintaining the openrgb-headless fork

This fork tracks `gitlab.com/CalcProgrammer1/OpenRGB` upstream and applies a small
set of deletions / edits to strip Qt and the GUI. The goal is to make upstream
syncs cheap so we keep getting new device controllers, hardware detector
improvements, and SDK protocol fixes for free.

## Branch layout

- `main` — mirrors `upstream/master`. Should never have headless-specific
  changes. Merging upstream goes into this branch first.
- `headless` — the headless build. Branches off `main` and contains all the
  deletions, the rewritten `OpenRGB.pro`, the rewritten `startup/startup.cpp`,
  and the headless `README.md` / `MAINTAINING.md`. CI publishes the binary from
  this branch.

## Initial fork setup (already done)

```bash
git remote add upstream https://gitlab.com/CalcProgrammer1/OpenRGB.git
git fetch upstream
git checkout -b main upstream/master
git push -u origin main
git checkout -b headless main
# ... apply patches, delete GUI files ...
git push -u origin headless
```

## Routine upstream sync

Run this every few weeks (or whenever upstream tags a release).

```bash
cd /path/to/openrgb-headless
git fetch upstream

# 1. Update main to upstream's master
git checkout main
git merge upstream/master --ff-only         # should always fast-forward
git push origin main

# 2. Merge main into headless and resolve
git checkout headless
git merge main
# Resolve conflicts:
#  - OpenRGB.pro almost always conflicts (we restructured it).
#    Take both sides intelligently: keep our headless edits, integrate any
#    new SOURCES/HEADERS lines for new controllers from upstream.
#  - startup/startup.cpp may conflict if upstream changed it.
#    Take our version unless upstream changed something fundamental.
#  - README.md / MAINTAINING.md — always take ours.
#  - Any new files under qt/ that upstream added — `git rm` them and re-commit.

# 3. Build and verify locally before pushing
qmake OpenRGB.pro CONFIG+=release
make -j$(nproc)
./openrgb --server --server-port 6742 &
nc -z 127.0.0.1 6742 && echo "server ok"
kill %1

# 4. Push and let CI verify Windows + Linux + macOS
git push origin headless
```

## Files we own (always keep ours on conflict)

- `README.md`
- `MAINTAINING.md`
- `.github/workflows/headless.yml`
- `OpenRGB.pro` (merge intelligently — see below)
- `startup/startup.cpp` (we deleted the GUI branches)
- `startup/main_Windows.cpp` (we removed the QApplication include)
- `startup/main_FreeBSD_Linux_MacOS.cpp` (we removed macutils.h include)

## Files we deleted (re-delete on conflict)

If upstream re-adds any of these in a merge, `git rm` them again:

- `qt/` — entire directory
- `dependencies/ColorWheel/` — entire directory
- `SuspendResume/` — entire directory
- `PluginManager.cpp`, `PluginManager.h`
- `OpenRGBPluginInterface.h`
- `Documentation/Images/` — GUI screenshots
- `README-HEADLESS.md` — superseded by `README.md`

## Files we relocated

- `qt/hsv.{cpp,h}` → `hsv.{cpp,h}` (top level). Several controllers
  `#include "hsv.h"`. Upstream merges that touch `qt/hsv.cpp` will conflict —
  apply the change to the top-level `hsv.cpp` instead.

## OpenRGB.pro conflict-resolution playbook

When upstream merges touch `OpenRGB.pro`:

1. **Upstream added a controller**: take their new `Controllers/...cpp/.h`
   lines into our SOURCES/HEADERS. Our deletions don't affect controller files.
2. **Upstream added a new GUI file**: drop their addition. Our `OpenRGB.pro`
   doesn't reference `qt/`, FORMS, RESOURCES, TRANSLATIONS, ColorWheel, or
   PluginManager — keep it that way.
3. **Upstream changed Qt version requirements** (`QT += widgets-foo`): drop
   their addition. Our `QT =` line clears all Qt modules.
4. **Upstream changed `lrelease` / `embed_translations` / icon / desktop
   install rules**: drop their addition. We don't ship a desktop entry.
5. **Upstream changed dependencies** (`hidapi`, `libusb`, `mbedtls`): take
   their changes. We use the same hardware backends.
6. **Upstream changed CLI flags** in `cli.cpp`: take their changes. We support
   the same CLI surface; our `startup/startup.cpp` ignores GUI-only flags.
7. **Upstream's precompiled header** (`CONFIG += precompile_header`,
   `PRECOMPILED_HEADER`): leave it out. It only speeds up builds; the headless
   `.pro` builds without it (CI: Windows and Linux).

## Fork-specific patches (not upstream-identical)

### DetectionManager.cpp - detector name tagging

**What we changed:**

1. Added `DetectionManager::TagDetector(result, name)`, which sets
   `detector_name` on every controller a detector returned. `name` is taken by
   value.
2. Wrapped the 7 detector-invocation sites (I2C device, I2C DIMM, I2C PCI, HID
   safe-mode, `RunHIDDetector`, `RunHIDWrappedDetector`, miscellaneous device)
   as `TagDetector(<upstream's detector call>, <that site's own name>)`.
3. `RegisterRGBController` falls back to the shared `detection_string` when a
   controller has no `detector_name` (detectors that register from inside their
   own body).

**Why:** `detector-map.json` maps device names to the detector key a client
must denylist. Detectors themselves are called directly, as upstream; there is
deliberately NO timeout, worker thread, try/catch or `<future>` wrapper (a
detached timed-out worker leaked controllers and held pointers into a freed
HID device list).

**Rules when merging upstream:**

- Pass the site's loop-local immutable name (`i2c_device_detector_strings[idx]`,
  `<detectors>[idx].name`, `detector.name`, `detector->name`), never the shared
  `detection_string` member. HID detectors also run on hidapi's hotplug thread,
  which rewrites `detection_string` concurrently. The fallback in
  `RegisterRGBController` still reads it and can race the same way; it only
  applies to self-registering detectors.
- If upstream touches an invocation line, keep their call and keep the
  `TagDetector(` wrapper around it. A new detector category gets the same
  wrapper with its own name.
- If upstream ever adds a timeout or wrapper of its own, drop this tagging
  only if `detector_name` is set another way.

### OpenRGB.pro - macOS headless build

**What we changed:** inside the `macx { }` block,

1. `CONFIG -= app_bundle` so the build emits a plain Mach-O console
   executable instead of a `.app` bundle.
2. Added `-framework IOKit` and `-framework CoreFoundation` to `LIBS`.

**Why:** the headless fork ships a console binary on every platform, not a
macOS `.app`. `IOKit` + `CoreFoundation` are required by
`dependencies/macUSPCIO/macUSPCIOAccess.h` (uses `IOServiceMatching`,
`kIOMasterPortDefault`, etc.); upstream used to get them transitively via
Qt's `QtGui` framework, which the headless build no longer links.

**Conflict resolution:** if upstream edits the `macx` block, keep both
changes on top. They are additive - neither line affects upstream's
macOS GUI build.

### ENESMBusController.cpp - first-generation fallback for unknown device strings

**What we changed:** in the constructor's final `else` (device string matches
no known variant), dropped upstream's `led_count = 0` (added in upstream
05983e2d) and restored the "assume first generation controller" comment.

**Why:** upstream skips a zero-LED controller only in the DRAM detector. The
motherboard, GPU and XPG S40G detectors still register it, with zones from the
config table, and `SetAllColorsDirect` then sends `led_count * 3 = 0` bytes: an
ASUS board or GPU with an unlisted string shows up and ignores every Direct
frame. Before 05983e2d those chips fell back to V1 registers and worked.

**Conflict resolution:** if upstream edits this `else` branch, keep their
change but leave `led_count` unset there. Drop this patch once upstream gates
every ENE detector on `GetLEDCount() > 0` or adds the missing strings.

### RGBController.h - reentrant AccessMutex

**What we changed:** `AccessMutex` is a `ReentrantSharedMutex`
(`RGBController/ReentrantSharedMutex.h`) instead of a `std::shared_mutex`. It
has the same four operations; a thread that already holds the mutex (shared or
exclusive) re-acquires it as a counter bump.

**Why:** `DeviceCallThreadFunction()` holds `AccessMutex` shared around
`DeviceUpdateLEDs()`, and controllers such as ENE call `GetActiveMode()` from
there, which takes it shared again. When a network `UpdateLEDs` is already
queued for the exclusive lock, `std::shared_mutex` (Windows SRW, macOS)
blocks the nested shared acquire behind the writer, and that controller never
updates again. Measured on an ENE DDR4 kit streamed at 30 fps: one stick
deadlocked within minutes while its twin on the same SMBus kept working.

**Conflict resolution:** keep our type on the `AccessMutex` line in
`RGBController.h` and the header in `OpenRGB.pro`. Call sites need nothing:
they only use `lock`, `unlock`, `lock_shared` and `unlock_shared`.

**Upstream PR candidate:** Yes. Drop this patch once upstream stops taking
`AccessMutex` recursively.

### NetworkServer.cpp - listen-thread wakeups

**What we changed:** the per-controller and ProfileManager listen threads wait
on `start_cv` with a predicate (`ListenThreadHasWork`: queue not empty, or
told to stop), release `start_mutex` as soon as they wake, and every notify
goes through `WakeListenThread`, which takes `start_mutex` before notifying.

**Why:** the threads waited with no predicate and producers notified without
`start_mutex`, so a notify between a thread's queue check and its wait was
lost. A queued packet then waited for the next packet to the same controller,
and a lost shutdown notify left `SetControllers()` hanging in `join()` with
`controller_threads_mutex` held, which stalls every client.

**Conflict resolution:** keep the two helpers above
`RGBController_UpdateCallback`, the predicate waits, and `WakeListenThread`
at every notify site; a new listen thread or enqueue site follows the same
pattern.

**Upstream PR candidate:** Yes.

## Verifying after a merge

The CI workflow at `.github/workflows/headless.yml` builds Windows, Linux,
and macOS (arm64) on every push. Wait for all three green before declaring
the merge done.

Local smoke test:

```bash
qmake OpenRGB.pro CONFIG+=release
make -j$(nproc) 2>&1 | tail -20
./openrgb --server --noautoconnect &
sleep 2
nc -z 127.0.0.1 6742 && echo "ok" || echo "FAIL"
kill %1
```

If the local smoke test passes and CI is green, the merge is good.

## Cadence

Sync upstream every **2-3 months** (matches upstream's release cadence). More
frequent syncs aren't worth the conflict-resolution time. Less frequent syncs
let conflicts pile up and become harder.
