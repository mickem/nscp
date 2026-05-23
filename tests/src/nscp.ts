import * as fs from "fs";
import * as os from "os";
import * as net from "net";
import * as path from "path";
import execa from "execa";

import { registerFailureDumper } from "./log-on-fail";

type ExecaChildProcess = execa.ExecaChildProcess;
type ExecaReturnValue = execa.ExecaReturnValue;

function nscpBin(): string {
  if (!process.env.NSCP_BIN) {
    throw new Error("NSCP_BIN not set; global-setup did not run");
  }
  return process.env.NSCP_BIN;
}

/**
 * Resolve a bundled asset (lua script, security file, …) against the well-
 * known layouts nscp actually ships with. Search order:
 *
 *   1. `$NSCP_SHARED_DIR/<rel>` — explicit override for unusual installs.
 *   2. `<bindir>/<rel>`         — build tree (scripts/ + security/ sit next
 *                                 to nscp in CMake's output dir).
 *   3. `<bindir>/../lib/nsclient/<rel>` — Debian/RPM install: nscp lives at
 *                                 /usr/sbin/nscp and shared files at
 *                                 /usr/lib/nsclient/ (UNIX_SHARED_PATH_FOLDER
 *                                 in CMakeLists.txt:811). This is the path
 *                                 that fails the bin-relative lookup.
 *   4. `/usr/lib/nsclient/<rel>` — absolute fallback in case the bin dir
 *                                 ever moves but the shared dir doesn't.
 */
function findShared(rel: string): string {
  const binDir = path.dirname(nscpBin());
  const candidates: string[] = [];
  if (process.env.NSCP_SHARED_DIR) {
    candidates.push(path.join(process.env.NSCP_SHARED_DIR, rel));
  }
  candidates.push(path.join(binDir, rel));
  candidates.push(path.join(binDir, "..", "lib", "nsclient", rel));
  if (process.platform !== "win32") {
    candidates.push(path.join("/usr/lib/nsclient", rel));
  }
  for (const c of candidates) {
    if (fs.existsSync(c)) return c;
  }
  throw new Error(
    `Bundled file ${rel} not found. Searched:\n  - ${candidates.join("\n  - ")}`,
  );
}

/**
 * Absolute path to a Lua script bundled next to the nscp binary
 * (`<build>/scripts/lua/<name>.lua` in a build tree;
 * `/usr/lib/nsclient/scripts/lua/<name>.lua` on Debian/RPM). Tests pass
 * this to `lua add --script` so the lookup doesn't depend on `${scripts}`
 * expanding to the right place inside a per-test workDir.
 */
export function bundledLuaScript(name: string): string {
  const withExt = name.endsWith(".lua") ? name : `${name}.lua`;
  return findShared(path.join("scripts", "lua", withExt));
}

/**
 * Absolute path to a security file bundled next to the nscp binary
 * (`<build>/security/<name>` in a build tree;
 * `/usr/lib/nsclient/security/<name>` on Debian/RPM) — e.g.
 * `nrpe_dh_2048.pem`. Use to pin NRPEServer's `dh` setting at the file
 * that actually exists in this build, since the per-test
 * `certificate-path` override points at an empty scratch dir where the
 * DH params aren't generated.
 */
export function bundledSecurityFile(name: string): string {
  return findShared(path.join("security", name));
}

export interface NscpInstanceOptions {
  /** Per-test working directory. Defaults to an os.tmpdir() entry. */
  workDir?: string;
  /** Extra args to append to `nscp test`. */
  extraTestArgs?: string[];
  /** Override the settings file path. Defaults to `<workDir>/nsclient.ini`. */
  settingsFile?: string;
  /** When `true`, log the spawned nscp's stdout/stderr to the test runner. */
  logOutput?: boolean;
  /**
   * Extra / override `--path-override KEY=VALUE` pairs applied to every
   * nscp invocation (both one-shot `run()` and the background `start()`).
   *
   * By default `certificate-path` is pinned to `<workDir>/security` so
   * `nrpe install` / `web install` write generated certs into a writable
   * scratch dir instead of /usr/lib/nsclient/security/ (which needs root).
   * Pass `{ "certificate-path": "/custom/dir" }` to point somewhere else,
   * or add more keys (`module-path`, `scripts-path`, …) on top.
   */
  pathOverrides?: Record<string, string>;
}

export type NscpRunOptions = {
  /** Maximum runtime; defaults to 30s. */
  timeout?: number;
  /** Don't throw on non-zero exit. The caller inspects `.exitCode`. */
  allowFailure?: boolean;
};

export type NscpRunResult = ExecaReturnValue;

/**
 * Per-test NSClient++ harness.
 *
 *   const nscp = new NscpInstance({ workDir });
 *   await nscp.configure({ "/modules": { CheckSystem: "enabled" } });
 *   await nscp.run(["nrpe", "install", "--insecure"]);
 *   await nscp.start();          // spawn `nscp test`
 *   await nscp.waitForPort(5666);
 *   const r = await nscp.run(["nrpe", "--host", "127.0.0.1", "--command", "x"]);
 *   await nscp.stop();
 */
export class NscpInstance {
  readonly workDir: string;
  readonly settingsFile: string;
  /** Resolved `--path-override` map (caller overrides merged on top of defaults). */
  readonly pathOverrides: Record<string, string>;
  private readonly extraTestArgs: string[];
  private readonly logOutput: boolean;
  private proc?: ExecaChildProcess;
  private stdoutBuf = "";
  private stderrBuf = "";

  constructor(opts: NscpInstanceOptions = {}) {
    this.workDir =
      opts.workDir ?? fs.mkdtempSync(path.join(os.tmpdir(), "nscp-it-"));
    fs.mkdirSync(this.workDir, { recursive: true });
    this.settingsFile = opts.settingsFile ?? path.join(this.workDir, "nsclient.ini");
    this.extraTestArgs = opts.extraTestArgs ?? [];
    this.logOutput = opts.logOutput ?? false;
    if (!fs.existsSync(this.settingsFile)) {
      fs.writeFileSync(this.settingsFile, "");
    }
    // Pre-create the default certificate-path scratch dir so nscp can
    // immediately drop generated CA / server / DH files into it.
    const defaultSecurityDir = path.join(this.workDir, "security");
    fs.mkdirSync(defaultSecurityDir, { recursive: true });
    // Point `${scripts}` at the build's scripts dir next to the nscp
    // binary. Without this it defaults to /usr/lib/nsclient/scripts
    // (the install location) which doesn't exist in dev — modules that
    // resolve scripts through `${scripts}` (LUAScript, CheckMKServer's
    // auto-loaded default_check_mk.lua, …) then silently fail to find
    // their files.
    this.pathOverrides = {
      "certificate-path": defaultSecurityDir,
      scripts: path.join(path.dirname(nscpBin()), "scripts"),
      ...(opts.pathOverrides ?? {}),
    };
    // Register once. The dumper closes over `this` and reads the latest
    // buffer at dump time, so each `start()` reset still surfaces the
    // current run's output without re-registering.
    registerFailureDumper(() => {
      if (!this.stdoutBuf && !this.stderrBuf) return "";
      let out = "--- nscp test stdout ---\n" + this.stdoutBuf;
      if (!out.endsWith("\n")) out += "\n";
      if (this.stderrBuf) {
        out += "--- nscp test stderr ---\n" + this.stderrBuf;
        if (!out.endsWith("\n")) out += "\n";
      }
      return out;
    });
  }

  /** Flatten `pathOverrides` into the CLI form `--path-override K=V …`. */
  private pathOverrideArgs(): string[] {
    const out: string[] = [];
    for (const [k, v] of Object.entries(this.pathOverrides)) {
      out.push("--path-override", `${k}=${v}`);
    }
    return out;
  }

  /** Path to a per-instance scratch sub-directory; created on first call. */
  scratch(name: string): string {
    const p = path.join(this.workDir, name);
    fs.mkdirSync(p, { recursive: true });
    return p;
  }

  /**
   * Run a one-shot nscp subcommand against this instance's settings file.
   * Throws on non-zero exit unless `allowFailure: true`.
   */
  async run(args: string[], opts: NscpRunOptions = {}): Promise<NscpRunResult> {
    if (args.length === 0) throw new Error("nscp.run: args must include a subcommand");
    // nscp requires the subcommand as the first arg; --settings is a
    // common option that must come after it.
    const [sub, ...rest] = args;
    return execa(
      nscpBin(),
      [sub, "--settings", this.settingsFile, ...this.pathOverrideArgs(), ...rest],
      {
        cwd: this.workDir,
        timeout: opts.timeout ?? 30_000,
        reject: !opts.allowFailure,
        all: true,
        env: process.env,
      },
    );
  }

  /**
   * Bulk-apply a settings tree via `nscp settings --path ... --key ... --set ...`.
   * Keys whose values are objects are recursed into.
   */
  async configure(tree: SettingsTree): Promise<void> {
    for (const [pathStr, kvs] of flattenSettings(tree)) {
      for (const [key, value] of Object.entries(kvs)) {
        await this.run([
          "settings",
          "--path",
          pathStr,
          "--key",
          key,
          "--set",
          String(value),
        ]);
      }
    }
  }

  /** Spawn `nscp test` in the background. Resolves immediately. */
  start(): void {
    if (this.proc) throw new Error("nscp already started");
    this.stdoutBuf = "";
    this.stderrBuf = "";
    this.proc = execa(
      nscpBin(),
      ["test", "--settings", this.settingsFile, ...this.pathOverrideArgs(), ...this.extraTestArgs],
      {
        cwd: this.workDir,
        reject: false,
        all: false,
        buffer: false,
        // Closing stdin keeps nscp's CommandClient REPL from holding
        // back the stdout pipe — without this we never see any output
        // until shutdown.
        stdin: "ignore",
        env: process.env,
      },
    );
    this.proc.stdout?.on("data", (b: Buffer) => {
      this.stdoutBuf += b.toString();
      if (this.logOutput) process.stdout.write(b);
    });
    this.proc.stderr?.on("data", (b: Buffer) => {
      this.stderrBuf += b.toString();
      if (this.logOutput) process.stderr.write(b);
    });
  }

  /** Stop the background nscp test process. Idempotent. */
  async stop(opts: { signal?: NodeJS.Signals; timeout?: number } = {}): Promise<void> {
    if (!this.proc) return;
    const proc = this.proc;
    const signal = opts.signal ?? (process.platform === "win32" ? "SIGKILL" : "SIGTERM");
    const timeout = opts.timeout ?? 5_000;
    const waitExit = (ms: number) => {
      let timer: NodeJS.Timeout | undefined;
      const sleep = new Promise<void>((r) => { timer = setTimeout(r, ms); });
      return Promise.race([
        proc.catch(() => undefined).then(() => { if (timer) clearTimeout(timer); }),
        sleep,
      ]);
    };
    try {
      try { proc.kill(signal); } catch { /* already exited */ }
      await waitExit(timeout);
      // `proc.killed` only reports that *a* signal was sent, not that the
      // child actually died — escalate if it's still alive.
      if (proc.exitCode === null && proc.signalCode === null) {
        try { proc.kill("SIGKILL"); } catch { /* already exited */ }
        await waitExit(2_000);
      }
    } finally {
      this.proc = undefined;
    }
  }

  /** Collected stdout from the running `nscp test` process so far. */
  capturedStdout(): string { return this.stdoutBuf; }
  capturedStderr(): string { return this.stderrBuf; }

  /** Wait for a TCP port on localhost to start accepting connections. */
  async waitForPort(port: number, opts: { host?: string; timeoutMs?: number } = {}): Promise<void> {
    const host = opts.host ?? "127.0.0.1";
    const deadline = Date.now() + (opts.timeoutMs ?? 30_000);
    let lastErr: unknown;
    while (Date.now() < deadline) {
      try {
        await new Promise<void>((resolve, reject) => {
          const s = new net.Socket();
          const fail = (e: Error) => { s.destroy(); reject(e); };
          s.setTimeout(1000);
          s.once("error", fail);
          s.once("timeout", () => fail(new Error("timeout")));
          s.connect(port, host, () => { s.end(); resolve(); });
        });
        return;
      } catch (e) {
        lastErr = e;
        await new Promise((r) => setTimeout(r, 250));
      }
    }
    throw new Error(`Port ${host}:${port} did not start accepting connections within ${opts.timeoutMs ?? 30_000}ms: ${(lastErr as Error)?.message}`);
  }
}

export type SettingsTree = {
  [path: string]: Record<string, string | number | boolean> | SettingsTree;
};

/** Resolve a SettingsTree to a flat list of `[path, {key: value}]` pairs. */
function flattenSettings(
  tree: SettingsTree,
  prefix = "",
): Array<[string, Record<string, string | number | boolean>]> {
  const out: Array<[string, Record<string, string | number | boolean>]> = [];
  for (const [k, v] of Object.entries(tree)) {
    const fullPath = k.startsWith("/") ? k : `${prefix}/${k}`.replace(/\/+/g, "/");
    if (v && typeof v === "object" && !isLeaf(v)) {
      out.push(...flattenSettings(v as SettingsTree, fullPath));
    } else {
      out.push([fullPath, v as Record<string, string | number | boolean>]);
    }
  }
  return out;
}

function isLeaf(obj: Record<string, unknown>): boolean {
  return Object.values(obj).every(
    (v) => typeof v === "string" || typeof v === "number" || typeof v === "boolean",
  );
}

/** Convenience: spawn nscp with explicit args, not bound to an instance. */
export async function nscp(args: string[], opts: NscpRunOptions = {}): Promise<NscpRunResult> {
  return execa(nscpBin(), args, {
    timeout: opts.timeout ?? 30_000,
    reject: !opts.allowFailure,
    all: true,
    env: process.env,
  });
}
