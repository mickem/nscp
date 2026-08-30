/**
 * Exercises the CheckExternalScripts module end-to-end against the real nscp
 * binary, over the one-shot CLI / client-query path (no server/port/docker).
 *
 * These cover the security-relevant behaviour of the module: the `ext-scr
 * install` argument-lockdown tool, the argument metacharacter guard, and the
 * timeout enforcement on a runaway script. They are cross-platform except where
 * a case explicitly guards on the OS.
 */
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const onWindows = process.platform === "win32";

describe("CheckExternalScripts — ext-scr install argument lockdown (settings path)", () => {
  let nscp: NscpInstance;

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  /** Read a single settings value back from the store. */
  async function getSetting(settingsPath: string, key: string): Promise<string> {
    const r = await nscp.run(["settings", "--show", "--path", settingsPath, "--key", key], { allowFailure: true });
    return (r.all ?? `${r.stdout}\n${r.stderr}`).trim();
  }

  it("`ext-scr install --arguments=false` actually disables arguments on the path the module reads", async () => {
    // Start from a permissive config written where the module actually reads it.
    await nscp.configure({
      "/settings/external scripts": { "allow arguments": "true", "allow nasty characters": "true" },
    });

    // The lockdown tool must clear it on the same path. Before the fix it wrote
    // to `/settings/external scripts/server`, which nothing reads, so the
    // permissive value below stayed in force — a fail-dangerous no-op.
    const r = await nscp.run(["ext-scr", "install", "--arguments=false"], { allowFailure: true });
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;
    expect(out).toMatch(/Arguments are NOT allowed/i);

    expect(await getSetting("/settings/external scripts", "allow arguments")).toMatch(/false/i);
    expect(await getSetting("/settings/external scripts", "allow nasty characters")).toMatch(/false/i);
  });

  it("`ext-scr install --arguments=safe` enables arguments but not nasty characters", async () => {
    await nscp.configure({
      "/settings/external scripts": { "allow arguments": "false", "allow nasty characters": "false" },
    });

    const r = await nscp.run(["ext-scr", "install", "--arguments=safe"], { allowFailure: true });
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;
    expect(out).toMatch(/SAFE Arguments are allowed/i);

    expect(await getSetting("/settings/external scripts", "allow arguments")).toMatch(/true/i);
    expect(await getSetting("/settings/external scripts", "allow nasty characters")).toMatch(/false/i);
  });
});
