/**
 * Which platform a suite is running on, decided once.
 *
 * Every suite used to carry its own `process.platform === "win32"` line, and
 * they had drifted: some held a boolean, some a `describe`-or-skip, a few
 * tested for `"linux"` where they meant "not Windows" - which, now that the
 * harness also runs on macOS, is exactly the distinction that matters. Import
 * from here instead, so a suite says which of the three it is about:
 *
 *   - `onWindows` / `onLinux` / `onDarwin` name one OS;
 *   - `onUnix` is Linux and macOS together, for anything POSIX (a `/bin/sh`
 *     script, a `.so` module name, a SIGTERM shutdown).
 *
 * A case that reads procfs or sysfs, talks to systemd, or expects `dpkg` is
 * `onLinux`, not `onUnix`: on a Mac it would fail on the first line rather
 * than skip, which is the drift this file removes.
 *
 * The `describe*` / `it*` variants are the block-level gates. They evaluate
 * `describe` at import time, so this module is for suites and the helpers
 * they import - not for `global-setup.ts` or the jest environment, which run
 * before those globals exist.
 */

export const onWindows = process.platform === "win32";
export const onLinux = process.platform === "linux";
export const onDarwin = process.platform === "darwin";
/** Linux or macOS: anything POSIX. */
export const onUnix = !onWindows;

/** `describe` when `condition` holds, `describe.skip` otherwise. */
export function describeIf(condition: boolean): jest.Describe {
  return condition ? describe : describe.skip;
}

/** `it` when `condition` holds, `it.skip` otherwise. */
export function itIf(condition: boolean): jest.It {
  return condition ? it : it.skip;
}

export const describeOnWindows = describeIf(onWindows);
export const describeOnLinux = describeIf(onLinux);
export const describeOnDarwin = describeIf(onDarwin);
export const describeOnUnix = describeIf(onUnix);

export const itOnWindows = itIf(onWindows);
export const itOnLinux = itIf(onLinux);
export const itOnDarwin = itIf(onDarwin);
export const itOnUnix = itIf(onUnix);

/**
 * The check modules the macOS build does not carry yet. Their data sources
 * read the Linux kernel (procfs, mntent, inotify) and each module's
 * `module.cmake` skips it on Darwin until a Darwin data source exists.
 *
 * A suite that loads one of them gates on `describeWithModules(...)` below,
 * which skips the block on macOS and runs it everywhere else. The port that
 * adds a module to the macOS build removes it from this set, and that one
 * edit turns every gate on at once - the suites themselves do not change.
 *
 * Deliberately a platform gate rather than a probe of the install: on Linux
 * and Windows these modules are always built, and a suite that quietly
 * skipped because one was missing there would be hiding a broken package.
 */
const NOT_BUILT_ON_DARWIN: ReadonlySet<string> = new Set([
  "CheckSystem",
  "CheckDisk",
  "CheckLogFile",
]);

/** True when this platform's build carries `module`. */
export function moduleBuiltHere(module: string): boolean {
  return !(onDarwin && NOT_BUILT_ON_DARWIN.has(module));
}

/** `describe` when every named module is built on this platform. */
export function describeWithModules(...modules: string[]): jest.Describe {
  return describeIf(modules.every(moduleBuiltHere));
}

/** `it` when every named module is built on this platform. */
export function itWithModules(...modules: string[]): jest.It {
  return itIf(modules.every(moduleBuiltHere));
}
