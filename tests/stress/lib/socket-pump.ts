import { ChildProcess, spawn } from "child_process";
import * as path from "path";
import * as readline from "readline";

/**
 * The node side of stress/lib/socket_pump.py: a Python process that makes the
 * connections a flood needs, so node does not have to.
 *
 * Why: every libuv to date (fixed in 1.53.0, which no node release ships yet)
 * passes an uninitialised OSVERSIONINFOW to RtlGetVersion on every loopback
 * connect on Windows (libuv issue #5106). Now and then the stack garbage reads
 * as the larger OSVERSIONINFOEXW's size, RtlGetVersion writes 8 bytes too far
 * and node dies on its stack cookie - exit 0xC0000409, no output, no report,
 * jest's RUNS line just stops. A connection per request at a few thousand a
 * second (NRPE, NSCA) hits it within minutes. The undici keep-alive pool the
 * REST client uses, and the odd port probe, open few enough connections not
 * to matter.
 *
 * The pump knows no protocol: the NRPE and NSCA clients still build and check
 * every packet; the pump connects, writes, and reads until the server closes.
 *
 * Which transport the clients use is `NSCP_STRESS_SOCKETS`: `python` (what
 * the stress global setup defaults it to) or `node` (the default elsewhere,
 * so the regular suites that borrow the NRPE client need no Python).
 * `NSCP_STRESS_PYTHON` names the interpreter; default `python` on Windows,
 * `python3` elsewhere.
 *
 * One Python process tops out below what the agent can take - about 3900
 * NSCA submissions a second against node's 6700 - so the pump is
 * `NSCP_STRESS_PUMPS` processes (default 4), handed requests in turn.
 */

export type SocketTransport = "node" | "python";

export function socketTransport(): SocketTransport {
  const v = process.env.NSCP_STRESS_SOCKETS ?? "node";
  if (v !== "node" && v !== "python") {
    throw new Error(`NSCP_STRESS_SOCKETS=${v} is neither "node" nor "python"`);
  }
  return v;
}

/** TLS for a pumped connection, as files: Python's ssl loads certificates from paths. */
export interface PumpTls {
  /** The CA the server's certificate must chain to. */
  ca: string;
  /** The client certificate and its key, for a server that wants one. */
  cert?: string;
  key?: string;
  /** The name the server's certificate must carry; default the host. */
  servername?: string;
}

export interface PumpTarget {
  host: string;
  port: number;
  timeoutMs: number;
}

export interface PumpResult {
  /** Everything the server sent, up to its close. */
  data: Buffer;
  /** From the connection being up to the server's close: the time the agent had it. */
  exchangeMs: number;
  /** Connect attempts that found no local port free, retried before this one. */
  retries: number;
}

interface Pending {
  resolve: (r: PumpResult) => void;
  reject: (e: Error) => void;
  /** For a handshake: build the bytes to send from the bytes read. */
  respond?: (got: Buffer) => Buffer;
}

interface Reply {
  id?: number;
  ready?: boolean;
  phase?: string;
  ok?: boolean;
  data?: string;
  error?: string;
  ms?: number;
  retries?: number;
}

/** One pump process. */
class PumpProcess {
  private readonly child: ChildProcess;
  private readonly pending = new Map<number, Pending>();
  private nextId = 1;
  private readonly ready: Promise<void>;
  private exited: Error | undefined;
  private stderr = "";

  constructor() {
    const python =
      process.env.NSCP_STRESS_PYTHON ?? (process.platform === "win32" ? "python" : "python3");
    // -u: unbuffered, so a reply is not held back behind a block buffer.
    this.child = spawn(python, ["-u", path.join(__dirname, "socket_pump.py")], {
      stdio: ["pipe", "pipe", "pipe"],
      windowsHide: true,
    });
    let onReady: () => void = () => undefined;
    let onFail: (e: Error) => void = () => undefined;
    this.ready = new Promise<void>((resolve, reject) => {
      onReady = resolve;
      onFail = reject;
    });
    this.child.stderr?.on("data", (b: Buffer) => {
      this.stderr = (this.stderr + b.toString()).slice(-4000);
    });
    const fail = (e: Error) => {
      if (this.exited) return;
      this.exited = e;
      onFail(e);
      for (const p of this.pending.values()) p.reject(e);
      this.pending.clear();
    };
    this.child.on("error", (e) =>
      fail(new Error(`socket pump (${python}) did not start: ${e.message}`)),
    );
    this.child.on("exit", (code, signal) =>
      fail(
        new Error(
          `socket pump exited (code ${code}, signal ${signal ?? "none"}): ${this.stderr.trim()}`,
        ),
      ),
    );
    readline.createInterface({ input: this.child.stdout! }).on("line", (line) => {
      const m = JSON.parse(line) as Reply;
      if (m.ready) {
        onReady();
        return;
      }
      const p = this.pending.get(m.id!);
      if (!p) return;
      if (m.phase === "read") {
        let send: Buffer;
        try {
          send = p.respond!(Buffer.from(m.data!, "base64"));
        } catch (e) {
          // The pump is waiting for an answer; an empty write lets the
          // exchange end, and the failure is the caller's own.
          this.write({ id: m.id, send: "" });
          this.pending.delete(m.id!);
          p.reject(e as Error);
          return;
        }
        this.write({ id: m.id, send: send.toString("base64") });
        return;
      }
      this.pending.delete(m.id!);
      if (m.ok) {
        p.resolve({
          data: Buffer.from(m.data ?? "", "base64"),
          exchangeMs: m.ms ?? 0,
          retries: m.retries ?? 0,
        });
      } else {
        const e = new Error(m.error ?? "failed") as Error & { retries?: number };
        e.retries = m.retries;
        p.reject(e);
      }
    });
    // A pump nobody closed must not keep the test runner alive.
    this.child.unref();
    for (const s of [this.child.stdin, this.child.stdout, this.child.stderr]) {
      (s as unknown as { unref?: () => void } | null)?.unref?.();
    }
  }

  /** Connect, write `send`, and read until the server closes. */
  request(target: PumpTarget, send: Buffer, tls?: PumpTls): Promise<PumpResult> {
    return this.call({
      op: "request",
      ...this.where(target),
      send: send.toString("base64"),
      tls: tls ?? null,
    });
  }

  /**
   * Connect, read exactly `read` bytes, write what `respond` makes of them,
   * and read until the server closes.
   */
  handshake(
    target: PumpTarget,
    read: number,
    respond: (got: Buffer) => Buffer,
  ): Promise<PumpResult> {
    return this.call({ op: "handshake", ...this.where(target), read }, respond);
  }

  /** End the pump: it finishes what is in flight with nobody listening, and exits. */
  close(): void {
    this.child.stdin?.end();
  }

  private where(t: PumpTarget) {
    return { host: t.host, port: t.port, timeout: t.timeoutMs };
  }

  private async call(
    msg: Record<string, unknown>,
    respond?: (got: Buffer) => Buffer,
  ): Promise<PumpResult> {
    await this.ready;
    if (this.exited) throw this.exited;
    const id = this.nextId++;
    return new Promise<PumpResult>((resolve, reject) => {
      this.pending.set(id, { resolve, reject, respond });
      this.write({ ...msg, id });
    });
  }

  private write(msg: Record<string, unknown>): void {
    this.child.stdin!.write(JSON.stringify(msg) + "\n");
  }
}

/** The pump: `NSCP_STRESS_PUMPS` processes, each request to the next in turn. */
export class SocketPump {
  private readonly procs: PumpProcess[];
  private next = 0;

  constructor(processes = Number(process.env.NSCP_STRESS_PUMPS ?? 4)) {
    if (!Number.isInteger(processes) || processes < 1) {
      throw new Error(`NSCP_STRESS_PUMPS=${processes} is not a whole number >= 1`);
    }
    this.procs = Array.from({ length: processes }, () => new PumpProcess());
  }

  /** Connect, write `send`, and read until the server closes. */
  request(target: PumpTarget, send: Buffer, tls?: PumpTls): Promise<PumpResult> {
    return this.pick().request(target, send, tls);
  }

  /**
   * Connect, read exactly `read` bytes, write what `respond` makes of them,
   * and read until the server closes.
   */
  handshake(
    target: PumpTarget,
    read: number,
    respond: (got: Buffer) => Buffer,
  ): Promise<PumpResult> {
    return this.pick().handshake(target, read, respond);
  }

  /** End every pump process: each finishes what is in flight and exits. */
  close(): void {
    for (const p of this.procs) p.close();
  }

  private pick(): PumpProcess {
    const p = this.procs[this.next];
    this.next = (this.next + 1) % this.procs.length;
    return p;
  }
}
