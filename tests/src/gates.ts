/**
 * The block-level platform gates: `describe` or `it` when a condition holds,
 * the `.skip` variant otherwise, so a suite reads as a statement of where it
 * runs rather than a ternary over the jest globals.
 *
 * The conditions come from platform.ts. This file reads `describe` and `it`
 * at import time, so it is for suites and the helpers they import only -
 * global-setup.ts and the jest environment run before those globals exist and
 * import platform.ts instead.
 */
import { moduleBuiltHere, onDarwin, onLinux, onUnix, onWindows } from "./platform";

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
 * `describe` when every named module is built on this platform; see
 * `moduleBuiltHere` in platform.ts for the list and why it is a platform
 * gate rather than a probe of the install.
 */
export function describeWithModules(...modules: string[]): jest.Describe {
  return describeIf(modules.every(moduleBuiltHere));
}

/** `it` when every named module is built on this platform. */
export function itWithModules(...modules: string[]): jest.It {
  return itIf(modules.every(moduleBuiltHere));
}
