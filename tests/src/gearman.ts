/**
 * Minimal gearman client/worker plus the Mod-Gearman payload envelope, for
 * the GearmanClient integration tests.
 *
 * Three layers, each usable on its own:
 *
 *  1. The gearman binary protocol (`encodePacket` / `decodePacket`,
 *     `GearmanConnection`) and its text admin protocol (`adminStatus`,
 *     `adminWorkers`). Enough to submit jobs, register as a worker, grab
 *     jobs and complete them — the test can therefore play either the
 *     monitoring core (submit a check, read the result queue) or the worker
 *     (grab a check the real core scheduled).
 *
 *  2. The Mod-Gearman envelope (`encryptPayload` / `decryptPayload`): base64
 *     of AES-256-ECB over the text with the shared password NUL-padded to a
 *     32-byte key, OpenSSL padding off and the plaintext zero-padded by hand,
 *     exactly as `common/gm_crypt.c` does in both ConSol's mod_gearman and
 *     Nagios' nagios-mod-gearman fork. With encryption off the payload is
 *     base64 only.
 *
 *  3. The job / result text formats (`parseKeyValueText`, `formatJobText`,
 *     `formatResultText`) and a parser for the cores' status.dat
 *     (`parseStatusDat`), which is the only uniform way to observe what a
 *     Nagios Core (no REST API) or Naemon did with a result.
 *
 * Everything here is byte-exact against payloads captured from the real NEB
 * modules and send_gearman tools; those captures live in
 * modules/GearmanClient/fixtures/ and gearman-fixtures.test.ts round-trips
 * them through this code.
 */
import crypto from "crypto";
import net from "net";

// ---------------------------------------------------------------------------
// 1. Gearman binary protocol
// ---------------------------------------------------------------------------

/** Packet type codes from the gearmand PROTOCOL document. */
export enum GearmanPacket {
  CAN_DO = 1,
  CANT_DO = 2,
  RESET_ABILITIES = 3,
  PRE_SLEEP = 4,
  NOOP = 6,
  SUBMIT_JOB = 7,
  JOB_CREATED = 8,
  GRAB_JOB = 9,
  NO_JOB = 10,
  JOB_ASSIGN = 11,
  WORK_STATUS = 12,
  WORK_COMPLETE = 13,
  WORK_FAIL = 14,
  GET_STATUS = 15,
  ECHO_REQ = 16,
  ECHO_RES = 17,
  SUBMIT_JOB_BG = 18,
  ERROR = 19,
  STATUS_RES = 20,
  SUBMIT_JOB_HIGH = 21,
  SET_CLIENT_ID = 22,
  CAN_DO_TIMEOUT = 23,
  ALL_YOURS = 24,
  WORK_EXCEPTION = 25,
  OPTION_REQ = 26,
  OPTION_RES = 27,
  WORK_DATA = 28,
  WORK_WARNING = 29,
  GRAB_JOB_UNIQ = 30,
  JOB_ASSIGN_UNIQ = 31,
  SUBMIT_JOB_HIGH_BG = 32,
  SUBMIT_JOB_LOW = 33,
  SUBMIT_JOB_LOW_BG = 34,
}

/**
 * How many NUL-separated arguments each packet carries. The last argument
 * is "the rest of the packet" and may itself contain NUL bytes (a job's
 * workload), so a decoder must stop splitting after count-1 separators
 * instead of splitting on every NUL.
 */
const PACKET_ARGS: Record<number, number> = {
  [GearmanPacket.CAN_DO]: 1,
  [GearmanPacket.CANT_DO]: 1,
  [GearmanPacket.RESET_ABILITIES]: 0,
  [GearmanPacket.PRE_SLEEP]: 0,
  [GearmanPacket.NOOP]: 0,
  [GearmanPacket.SUBMIT_JOB]: 3,
  [GearmanPacket.JOB_CREATED]: 1,
  [GearmanPacket.GRAB_JOB]: 0,
  [GearmanPacket.NO_JOB]: 0,
  [GearmanPacket.JOB_ASSIGN]: 3,
  [GearmanPacket.WORK_STATUS]: 3,
  [GearmanPacket.WORK_COMPLETE]: 2,
  [GearmanPacket.WORK_FAIL]: 1,
  [GearmanPacket.GET_STATUS]: 1,
  [GearmanPacket.ECHO_REQ]: 1,
  [GearmanPacket.ECHO_RES]: 1,
  [GearmanPacket.SUBMIT_JOB_BG]: 3,
  [GearmanPacket.ERROR]: 2,
  [GearmanPacket.STATUS_RES]: 5,
  [GearmanPacket.SUBMIT_JOB_HIGH]: 3,
  [GearmanPacket.SET_CLIENT_ID]: 1,
  [GearmanPacket.CAN_DO_TIMEOUT]: 2,
  [GearmanPacket.ALL_YOURS]: 0,
  [GearmanPacket.WORK_EXCEPTION]: 2,
  [GearmanPacket.OPTION_REQ]: 1,
  [GearmanPacket.OPTION_RES]: 1,
  [GearmanPacket.WORK_DATA]: 2,
  [GearmanPacket.WORK_WARNING]: 2,
  [GearmanPacket.GRAB_JOB_UNIQ]: 0,
  [GearmanPacket.JOB_ASSIGN_UNIQ]: 4,
  [GearmanPacket.SUBMIT_JOB_HIGH_BG]: 3,
  [GearmanPacket.SUBMIT_JOB_LOW]: 3,
  [GearmanPacket.SUBMIT_JOB_LOW_BG]: 3,
};

export const GEARMAN_MAGIC_REQ = Buffer.from("\0REQ", "latin1");
export const GEARMAN_MAGIC_RES = Buffer.from("\0RES", "latin1");
export const GEARMAN_HEADER_SIZE = 12;
export const GEARMAN_DEFAULT_PORT = 4730;

export interface Packet {
  magic: "REQ" | "RES";
  type: GearmanPacket;
  /** Arguments as raw bytes; the last one may contain NULs. */
  args: Buffer[];
}

/** Twelve-byte header (magic, big-endian type, big-endian size) + NUL-joined args. */
export function encodePacket(
  type: GearmanPacket,
  args: Array<string | Buffer> = [],
  magic: "REQ" | "RES" = "REQ",
): Buffer {
  const expected = PACKET_ARGS[type];
  if (expected !== undefined && args.length !== expected) {
    throw new Error(
      `gearman packet ${GearmanPacket[type]} takes ${expected} argument(s), got ${args.length}`,
    );
  }
  const parts: Buffer[] = [];
  args.forEach((a, i) => {
    if (i > 0) parts.push(Buffer.from([0]));
    parts.push(Buffer.isBuffer(a) ? a : Buffer.from(a, "utf8"));
  });
  const body = Buffer.concat(parts);
  const header = Buffer.alloc(GEARMAN_HEADER_SIZE);
  (magic === "REQ" ? GEARMAN_MAGIC_REQ : GEARMAN_MAGIC_RES).copy(header, 0);
  header.writeUInt32BE(type, 4);
  header.writeUInt32BE(body.length, 8);
  return Buffer.concat([header, body]);
}

/**
 * Decode one packet from the front of `buf`. Returns `null` while the
 * buffer holds less than a whole packet; throws on a bad magic.
 */
export function decodePacket(buf: Buffer): { packet: Packet; consumed: number } | null {
  if (buf.length < GEARMAN_HEADER_SIZE) return null;
  const magicBytes = buf.subarray(0, 4);
  let magic: "REQ" | "RES";
  if (magicBytes.equals(GEARMAN_MAGIC_REQ)) magic = "REQ";
  else if (magicBytes.equals(GEARMAN_MAGIC_RES)) magic = "RES";
  else throw new Error(`bad gearman magic: ${magicBytes.toString("hex")}`);
  const type = buf.readUInt32BE(4) as GearmanPacket;
  const size = buf.readUInt32BE(8);
  if (buf.length < GEARMAN_HEADER_SIZE + size) return null;
  const body = buf.subarray(GEARMAN_HEADER_SIZE, GEARMAN_HEADER_SIZE + size);
  const count = PACKET_ARGS[type];
  const args: Buffer[] = [];
  if (count === undefined) {
    if (size > 0) args.push(Buffer.from(body));
  } else if (count > 0) {
    let start = 0;
    for (let i = 0; i < count - 1; i++) {
      const nul = body.indexOf(0, start);
      if (nul < 0) throw new Error(`gearman ${GearmanPacket[type]}: missing argument ${i + 1}`);
      args.push(Buffer.from(body.subarray(start, nul)));
      start = nul + 1;
    }
    args.push(Buffer.from(body.subarray(start)));
  }
  return { packet: { magic, type, args }, consumed: GEARMAN_HEADER_SIZE + size };
}

/**
 * One TCP connection to gearmand with a promise-based packet reader. A
 * gearman connection may act as client and worker at the same time (the C
 * and Go workers submit results over the connection they grab jobs on), so
 * this class does not distinguish the two roles.
 */
export class GearmanConnection {
  private buffer: Buffer = Buffer.alloc(0);
  private queue: Packet[] = [];
  private waiters: Array<{ resolve: (p: Packet) => void; reject: (e: Error) => void }> = [];
  private failure: Error | undefined;

  private constructor(private readonly socket: net.Socket) {
    socket.on("data", (chunk: Buffer) => this.onData(chunk));
    socket.on("error", (e: Error) => this.fail(e));
    socket.on("close", () => this.fail(new Error("gearman connection closed")));
  }

  static connect(host: string, port: number, timeoutMs = 10_000): Promise<GearmanConnection> {
    return new Promise((resolve, reject) => {
      const socket = net.createConnection({ host, port });
      const timer = setTimeout(() => {
        socket.destroy();
        reject(new Error(`timed out connecting to gearmand at ${host}:${port}`));
      }, timeoutMs);
      socket.once("error", (e) => {
        clearTimeout(timer);
        reject(e);
      });
      socket.once("connect", () => {
        clearTimeout(timer);
        socket.setNoDelay(true);
        resolve(new GearmanConnection(socket));
      });
    });
  }

  private onData(chunk: Buffer): void {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    for (;;) {
      let decoded;
      try {
        decoded = decodePacket(this.buffer);
      } catch (e) {
        this.fail(e as Error);
        return;
      }
      if (!decoded) return;
      this.buffer = this.buffer.subarray(decoded.consumed);
      const waiter = this.waiters.shift();
      if (waiter) waiter.resolve(decoded.packet);
      else this.queue.push(decoded.packet);
    }
  }

  private fail(e: Error): void {
    if (this.failure) return;
    this.failure = e;
    for (const w of this.waiters.splice(0)) w.reject(e);
  }

  send(type: GearmanPacket, args: Array<string | Buffer> = []): Promise<void> {
    return new Promise((resolve, reject) => {
      if (this.failure) return reject(this.failure);
      this.socket.write(encodePacket(type, args), (e) => (e ? reject(e) : resolve()));
    });
  }

  /** Next packet from the server, or a rejection after `timeoutMs`. */
  recv(timeoutMs = 10_000): Promise<Packet> {
    const queued = this.queue.shift();
    if (queued) return Promise.resolve(queued);
    if (this.failure) return Promise.reject(this.failure);
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        const i = this.waiters.findIndex((w) => w.resolve === wrapped);
        if (i >= 0) this.waiters.splice(i, 1);
        reject(new Error(`timed out after ${timeoutMs}ms waiting for a gearman packet`));
      }, timeoutMs);
      const wrapped = (p: Packet): void => {
        clearTimeout(timer);
        resolve(p);
      };
      this.waiters.push({
        resolve: wrapped,
        reject: (e) => {
          clearTimeout(timer);
          reject(e);
        },
      });
    });
  }

  /** `recv` that insists on one of the given types, turning ERROR into a rejection. */
  async expect(types: GearmanPacket | GearmanPacket[], timeoutMs = 10_000): Promise<Packet> {
    const want = Array.isArray(types) ? types : [types];
    const p = await this.recv(timeoutMs);
    if (p.type === GearmanPacket.ERROR) {
      throw new Error(
        `gearmand ERROR ${p.args[0]?.toString() ?? ""}: ${p.args[1]?.toString() ?? ""}`,
      );
    }
    if (!want.includes(p.type)) {
      throw new Error(
        `expected ${want.map((t) => GearmanPacket[t]).join("|")}, got ${GearmanPacket[p.type]}`,
      );
    }
    return p;
  }

  close(): void {
    this.socket.destroy();
  }

  /**
   * SUBMIT_JOB_BG: queue `payload` on `func` and return the job handle. This
   * is how workers push results to `check_results` and how send_gearman
   * submits passive results.
   */
  async submitBackground(func: string, payload: string | Buffer, unique = ""): Promise<string> {
    await this.send(GearmanPacket.SUBMIT_JOB_BG, [func, unique, payload]);
    const created = await this.expect(GearmanPacket.JOB_CREATED);
    return created.args[0].toString();
  }

  /**
   * SUBMIT_JOB and wait for WORK_COMPLETE; returns the worker's data. Throws
   * when the worker answers WORK_FAIL.
   */
  async submitAndWait(
    func: string,
    payload: string | Buffer,
    unique = "",
    timeoutMs = 30_000,
  ): Promise<Buffer> {
    await this.send(GearmanPacket.SUBMIT_JOB, [func, unique, payload]);
    const created = await this.expect(GearmanPacket.JOB_CREATED);
    const handle = created.args[0].toString();
    for (;;) {
      const p = await this.expect(
        [
          GearmanPacket.WORK_COMPLETE,
          GearmanPacket.WORK_FAIL,
          GearmanPacket.WORK_DATA,
          GearmanPacket.WORK_STATUS,
          GearmanPacket.WORK_WARNING,
          GearmanPacket.WORK_EXCEPTION,
        ],
        timeoutMs,
      );
      if (p.args[0].toString() !== handle) continue;
      if (p.type === GearmanPacket.WORK_COMPLETE) return p.args[1];
      if (p.type === GearmanPacket.WORK_FAIL) throw new Error(`job ${handle} failed`);
      if (p.type === GearmanPacket.WORK_EXCEPTION) {
        throw new Error(`job ${handle} raised: ${p.args[1].toString()}`);
      }
    }
  }
}

export interface GearmanJob {
  handle: string;
  func: string;
  workload: Buffer;
}

/**
 * Worker side: CAN_DO on each function, then GRAB_JOB / PRE_SLEEP / NOOP
 * until a job arrives. `grab()` returns `null` when `timeoutMs` elapses
 * with nothing queued.
 */
export class GearmanWorker {
  private constructor(readonly conn: GearmanConnection) {}

  static async connect(
    host: string,
    port: number,
    functions: string[],
    clientId = "nscp-test-fixture",
  ): Promise<GearmanWorker> {
    const conn = await GearmanConnection.connect(host, port);
    await conn.send(GearmanPacket.SET_CLIENT_ID, [clientId]);
    await conn.send(GearmanPacket.RESET_ABILITIES);
    for (const f of functions) await conn.send(GearmanPacket.CAN_DO, [f]);
    return new GearmanWorker(conn);
  }

  async grab(timeoutMs = 30_000): Promise<GearmanJob | null> {
    const deadline = Date.now() + timeoutMs;
    for (;;) {
      await this.conn.send(GearmanPacket.GRAB_JOB);
      const p = await this.conn.expect(
        [GearmanPacket.JOB_ASSIGN, GearmanPacket.NO_JOB, GearmanPacket.NOOP],
        Math.max(1, deadline - Date.now()),
      );
      if (p.type === GearmanPacket.JOB_ASSIGN) {
        return { handle: p.args[0].toString(), func: p.args[1].toString(), workload: p.args[2] };
      }
      if (p.type === GearmanPacket.NOOP) continue;
      // NO_JOB: sleep until gearmand wakes us with NOOP (or the deadline).
      await this.conn.send(GearmanPacket.PRE_SLEEP);
      const remaining = deadline - Date.now();
      if (remaining <= 0) return null;
      try {
        await this.conn.expect(GearmanPacket.NOOP, remaining);
      } catch (e) {
        if (/timed out/.test((e as Error).message)) return null;
        throw e;
      }
    }
  }

  complete(job: GearmanJob, data: string | Buffer = ""): Promise<void> {
    return this.conn.send(GearmanPacket.WORK_COMPLETE, [job.handle, data]);
  }

  fail(job: GearmanJob): Promise<void> {
    return this.conn.send(GearmanPacket.WORK_FAIL, [job.handle]);
  }

  close(): void {
    this.conn.close();
  }
}

// ---------------------------------------------------------------------------
// Text admin protocol
// ---------------------------------------------------------------------------

/** Send one admin command (`status`, `workers`, `version`) and return the reply lines. */
export function adminCommand(
  host: string,
  port: number,
  command: string,
  timeoutMs = 10_000,
): Promise<string[]> {
  return new Promise((resolve, reject) => {
    const socket = net.createConnection({ host, port });
    let data = "";
    const timer = setTimeout(() => {
      socket.destroy();
      reject(new Error(`gearmand admin '${command}' timed out`));
    }, timeoutMs);
    socket.on("error", (e) => {
      clearTimeout(timer);
      reject(e);
    });
    socket.on("connect", () => socket.write(`${command}\n`));
    socket.on("data", (chunk: Buffer) => {
      data += chunk.toString("utf8");
      // `version` answers a single "OK <version>" line; the list commands
      // end with a lone "." line.
      const lines = data.split("\n");
      if (lines.includes(".") || (command === "version" && data.includes("\n"))) {
        clearTimeout(timer);
        socket.destroy();
        resolve(lines.filter((l) => l !== "" && l !== "."));
      }
    });
  });
}

export interface QueueStatus {
  queued: number;
  running: number;
  workers: number;
}

/** `status`: one row per function name gearmand has ever seen. */
export async function adminStatus(host: string, port: number): Promise<Map<string, QueueStatus>> {
  const out = new Map<string, QueueStatus>();
  for (const line of await adminCommand(host, port, "status")) {
    const [name, queued, running, workers] = line.split("\t");
    if (!name) continue;
    out.set(name, { queued: Number(queued), running: Number(running), workers: Number(workers) });
  }
  return out;
}

export interface WorkerInfo {
  fd: number;
  address: string;
  clientId: string;
  functions: string[];
}

/** `workers`: `<fd> <ip> <client id> : <function> <function> …` per connection. */
export async function adminWorkers(host: string, port: number): Promise<WorkerInfo[]> {
  const out: WorkerInfo[] = [];
  for (const line of await adminCommand(host, port, "workers")) {
    const m = /^(\d+)\s+(\S+)\s+(\S+)\s*:\s*(.*)$/.exec(line);
    if (!m) continue;
    out.push({
      fd: Number(m[1]),
      address: m[2],
      clientId: m[3],
      functions: m[4].split(/\s+/).filter((f) => f !== ""),
    });
  }
  return out;
}

// ---------------------------------------------------------------------------
// 2. Mod-Gearman payload envelope
// ---------------------------------------------------------------------------

export const GEARMAN_KEY_BYTES = 32;
export const GEARMAN_BLOCK_SIZE = 16;

/** The password truncated or NUL-padded to exactly 32 bytes (gm_crypt.c `mod_gm_aes_init`). */
export function gearmanKey(password: string | Buffer): Buffer {
  const key = Buffer.alloc(GEARMAN_KEY_BYTES, 0);
  const src = Buffer.isBuffer(password) ? password : Buffer.from(password, "utf8");
  src.copy(key, 0, 0, Math.min(src.length, GEARMAN_KEY_BYTES));
  return key;
}

/**
 * Encrypt `text` as mod_gearman does (`mod_gm_encrypt` + `mod_gm_aes_encrypt`):
 * the plaintext includes its terminating NUL (`strlen + 1`), is zero-padded
 * to the block size by hand — mod_gearman's test is `BLOCKSIZE % len != 0`,
 * which for a length that is a multiple of 16 (other than 16 itself) adds a
 * whole extra block of zeros; reproduced here so the bytes match — and is
 * run through AES-256-ECB with OpenSSL's own padding disabled, then base64.
 */
export function encryptPayload(text: string, password: string | Buffer): string {
  let plain = Buffer.concat([Buffer.from(text, "utf8"), Buffer.from([0])]);
  if (GEARMAN_BLOCK_SIZE % plain.length !== 0) {
    plain = Buffer.concat([
      plain,
      Buffer.alloc(GEARMAN_BLOCK_SIZE - (plain.length % GEARMAN_BLOCK_SIZE), 0),
    ]);
  }
  const cipher = crypto.createCipheriv("aes-256-ecb", gearmanKey(password), null);
  cipher.setAutoPadding(false);
  return Buffer.concat([cipher.update(plain), cipher.final()]).toString("base64");
}

/**
 * Decrypt a base64 envelope (`mod_gm_decrypt`): base64-decode (retrying with
 * newlines stripped, as the reference does), drop any trailing partial
 * block, AES-256-ECB with padding off, and cut at the first NUL — the C
 * code treats the result as a C string, which is what discards the zero
 * padding.
 */
export function decryptPayload(base64: string, password: string | Buffer): string {
  let raw = Buffer.from(base64, "base64");
  if (raw.length === 0 && base64.trim().length > 0) {
    raw = Buffer.from(base64.replace(/\n/g, ""), "base64");
  }
  const usable = raw.length - (raw.length % GEARMAN_BLOCK_SIZE);
  if (usable === 0) throw new Error("gearman payload shorter than one AES block");
  const decipher = crypto.createDecipheriv("aes-256-ecb", gearmanKey(password), null);
  decipher.setAutoPadding(false);
  const plain = Buffer.concat([decipher.update(raw.subarray(0, usable)), decipher.final()]);
  const nul = plain.indexOf(0);
  return (nul >= 0 ? plain.subarray(0, nul) : plain).toString("utf8");
}

/** `encryption=no`: base64 of the text, nothing else. */
export function encodePlainPayload(text: string): string {
  return Buffer.from(text, "utf8").toString("base64");
}

export function decodePlainPayload(base64: string): string {
  return Buffer.from(base64.replace(/\n/g, ""), "base64").toString("utf8");
}

export interface EnvelopeOptions {
  /** Shared password; ignored when `encryption` is false. */
  key: string;
  /** Default true. */
  encryption?: boolean;
}

export function encodePayload(text: string, opts: EnvelopeOptions): string {
  return opts.encryption === false ? encodePlainPayload(text) : encryptPayload(text, opts.key);
}

/**
 * Decode either flavour. Mirrors the module's `accept_clear_results`
 * mode: a payload that already starts with `type=` after base64 decoding
 * is taken as plain text, everything else is decrypted.
 */
export function decodePayload(base64: string, opts: EnvelopeOptions): string {
  if (opts.encryption === false) return decodePlainPayload(base64);
  const plain = decodePlainPayload(base64);
  if (plain.startsWith("type=")) return plain;
  return decryptPayload(base64, opts.key);
}

// ---------------------------------------------------------------------------
// 3. Job and result text
// ---------------------------------------------------------------------------

/**
 * `key=value` lines into a record. Only the first `=` splits; later ones
 * belong to the value (`command_line=check_ok message=hello`). Blank lines
 * and lines without `=` are ignored.
 */
export function parseKeyValueText(text: string): Record<string, string> {
  const out: Record<string, string> = {};
  for (const line of text.split("\n")) {
    const eq = line.indexOf("=");
    if (eq <= 0) continue;
    out[line.substring(0, eq)] = line.substring(eq + 1);
  }
  return out;
}

export interface CheckJob {
  type: "host" | "service";
  host_name: string;
  service_description?: string;
  command_line: string;
  /** Defaults to `check_results`. */
  result_queue?: string;
  target_queue?: string;
  /** Seconds since the epoch with microseconds, e.g. `1757930400.123456`. */
  core_time?: string;
  timeout?: number;
  /** Written by the Nagios fork's NEB module only, as `<epoch>.0`. */
  start_time?: string;
  next_check?: string;
}

/** Format seconds-with-microseconds the way the NEB module's `%Lf` does. */
export function formatCoreTime(date: Date = new Date()): string {
  return (date.getTime() / 1000).toFixed(6);
}

/**
 * Job text in the NEB module's field order, ending in the three newlines it
 * writes (`…command_line=%s\n\n\n`). Naemon's module omits `start_time` and
 * `next_check`; the Nagios fork writes them between the names and
 * `core_time`. Both are reproduced from the fixtures.
 */
export function formatJobText(job: CheckJob): string {
  const lines = [`type=${job.type}`];
  lines.push(`result_queue=${job.result_queue ?? "check_results"}`);
  lines.push(`target_queue=${job.target_queue ?? ""}`);
  lines.push(`host_name=${job.host_name}`);
  if (job.type === "service") lines.push(`service_description=${job.service_description ?? ""}`);
  if (job.start_time !== undefined) lines.push(`start_time=${job.start_time}`);
  if (job.next_check !== undefined) lines.push(`next_check=${job.next_check}`);
  lines.push(`core_time=${job.core_time ?? formatCoreTime()}`);
  lines.push(`timeout=${job.timeout ?? 60}`);
  lines.push(`command_line=${job.command_line}`);
  return `${lines.join("\n")}\n\n\n`;
}

export interface CheckResult {
  /** `passive` files it as a passive check; anything else is an active result. */
  type: "active" | "passive";
  host_name: string;
  service_description?: string;
  return_code: number;
  /** Plugin output; embedded newlines are escaped as the two characters `\n` on the wire. */
  output: string;
  start_time?: string;
  finish_time?: string;
  latency?: string;
  source?: string;
  exited_ok?: number;
  core_start_time?: string;
}

/**
 * Result text in send_gearman's field order, ending in the blank line it
 * writes. The worker (mod_gearman_worker / the Go worker) uses a slightly
 * different order and adds `exited_ok` and `core_start_time`; the result
 * thread reads keys by name so order does not matter to the core — this
 * order is the one the fixtures are byte-exact against.
 */
export function formatResultText(result: CheckResult): string {
  const now = formatCoreTime();
  const lines = [
    `type=${result.type}`,
    `host_name=${result.host_name}`,
    `start_time=${result.start_time ?? now}`,
    `finish_time=${result.finish_time ?? now}`,
    `latency=${result.latency ?? "0.000000"}`,
    `return_code=${result.return_code}`,
    `source=${result.source ?? "send_gearman"}`,
  ];
  if (result.exited_ok !== undefined) lines.push(`exited_ok=${result.exited_ok}`);
  if (result.core_start_time !== undefined) {
    lines.push(`core_start_time=${result.core_start_time}`);
  }
  if (result.service_description !== undefined) {
    lines.push(`service_description=${result.service_description}`);
  }
  lines.push(`output=${escapeOutput(result.output)}`);
  return `${lines.join("\n")}\n\n`;
}

/** Newlines inside plugin output travel as the two characters `\n`. */
export function escapeOutput(output: string): string {
  return output.replace(/\n/g, "\\n");
}

export function unescapeOutput(wire: string): string {
  return wire.replace(/\\n/g, "\n");
}

// ---------------------------------------------------------------------------
// Playing the core, playing the worker
// ---------------------------------------------------------------------------

export interface GearmanServer {
  host: string;
  port: number;
}

/**
 * Do what the NEB module does: put an encrypted check job on a queue. Returns
 * the job handle.
 */
export async function submitCheckJob(
  server: GearmanServer,
  queue: string,
  job: CheckJob,
  envelope: EnvelopeOptions,
): Promise<string> {
  const conn = await GearmanConnection.connect(server.host, server.port);
  try {
    const text = formatJobText({ ...job, target_queue: job.target_queue ?? queue });
    const unique =
      job.type === "service" ? `${job.host_name}-${job.service_description}` : job.host_name;
    return await conn.submitBackground(queue, encodePayload(text, envelope), unique);
  } finally {
    conn.close();
  }
}

/** Do what send_gearman does: push a result to `check_results` (or another queue). */
export async function submitCheckResult(
  server: GearmanServer,
  result: CheckResult,
  envelope: EnvelopeOptions,
  queue = "check_results",
): Promise<string> {
  const conn = await GearmanConnection.connect(server.host, server.port);
  try {
    return await conn.submitBackground(queue, encodePayload(formatResultText(result), envelope));
  } finally {
    conn.close();
  }
}

export interface GrabbedPayload<T> {
  job: GearmanJob;
  /** The decoded key=value text. */
  text: string;
  fields: T;
}

/**
 * Register on `queue`, wait for one job, decode it and hand it back along
 * with the raw job so the caller can `complete()` it. Returns `null` on
 * timeout. The worker stays connected (and registered) so the caller can
 * grab again; close it when done.
 */
export async function grabPayload<T = Record<string, string>>(
  worker: GearmanWorker,
  envelope: EnvelopeOptions,
  timeoutMs = 30_000,
): Promise<GrabbedPayload<T> | null> {
  const job = await worker.grab(timeoutMs);
  if (!job) return null;
  const text = decodePayload(job.workload.toString("latin1"), envelope);
  return { job, text, fields: parseKeyValueText(text) as T };
}

// ---------------------------------------------------------------------------
// status.dat
// ---------------------------------------------------------------------------

export interface StatusDat {
  info: Record<string, string>;
  programstatus: Record<string, string>;
  /** Keyed by host_name. */
  hosts: Map<string, Record<string, string>>;
  /** Keyed by `host_name!service_description`. */
  services: Map<string, Record<string, string>>;
}

/**
 * Parse the status file both Nagios Core and Naemon write:
 *
 *   servicestatus {
 *       host_name=nscp-test
 *       service_description=helper
 *       plugin_output=hello
 *       }
 *
 * `check_type` is 0 for an active result and 1 for a passive one;
 * `current_state` is the Nagios state; `last_check` is an epoch.
 */
export function parseStatusDat(text: string): StatusDat {
  const out: StatusDat = {
    info: {},
    programstatus: {},
    hosts: new Map(),
    services: new Map(),
  };
  // Scanned a line at a time rather than with a block regex: a status.dat is
  // whatever the core wrote, and `{\s*\n([\s\S]*?)\n\s*}` backtracks
  // quadratically over a file whose blocks never close.
  let kind: string | null = null;
  let body: string[] = [];
  const closeBlock = () => {
    const fields = parseKeyValueText(body.join("\n"));
    switch (kind) {
      case "info":
        out.info = fields;
        break;
      case "programstatus":
        out.programstatus = fields;
        break;
      case "hoststatus":
        out.hosts.set(fields.host_name, fields);
        break;
      case "servicestatus":
        out.services.set(`${fields.host_name}!${fields.service_description}`, fields);
        break;
      default:
        break;
    }
    kind = null;
    body = [];
  };
  for (const rawLine of text.split("\n")) {
    const line = rawLine.replace(/\r$/, "");
    const trimmed = line.trim();
    if (kind === null) {
      const open = /^(\w+)[ \t]*\{$/.exec(trimmed);
      if (open) kind = open[1];
      continue;
    }
    if (trimmed === "}") {
      closeBlock();
      continue;
    }
    // Only the indentation goes: a value may legitimately end in a space.
    body.push(line.replace(/^[ \t]+/, ""));
  }
  return out;
}

// ---------------------------------------------------------------------------
// Where the job server comes from
// ---------------------------------------------------------------------------

/**
 * An already-running gearmand to use instead of the container, named by
 * `NSCP_GEARMAND=host:port` (port defaults to 4730).
 *
 * The suites below need a job server, not a container: every assertion is
 * made over the wire against `127.0.0.1:<port>`, and the image exists only
 * to put a gearmand there. Docker stays the default because it pins the
 * version and starts clean, but an environment that has a gearmand and no
 * docker daemon - a developer box with the distribution package, or a
 * session whose egress policy blocks the registry - can still run the tier
 * rather than skip it.
 *
 * The one thing an external server cannot provide is `restart()`, so the
 * reconnect case guards on `usesContainer()`.
 */
export function externalGearmand(): GearmanServer | null {
  const raw = process.env.NSCP_GEARMAND?.trim();
  if (!raw) return null;
  const at = raw.lastIndexOf(":");
  if (at <= 0) return { host: raw, port: 4730 };
  const port = Number(raw.slice(at + 1));
  if (!Number.isInteger(port) || port <= 0) {
    throw new Error(`NSCP_GEARMAND is not host:port: ${raw}`);
  }
  return { host: raw.slice(0, at), port };
}

/**
 * `describe` when the gearman tier can run at all - either docker is
 * available or `NSCP_GEARMAND` names a job server - and `describe.skip`
 * otherwise. Use in place of `dockerOrSkip()` on a suite that only wants
 * a gearmand.
 */
export function gearmandOrSkip(): jest.Describe {
  if (externalGearmand()) return describe;
  return process.env.NSCP_SKIP_DOCKER === "1" ? describe.skip : describe;
}

/**
 * A suffix unique to this run of the suite, appended to every queue name the
 * gearman suites use.
 *
 * gearmand holds a background job until somebody registers for its queue, so
 * a result a case did not read stays there. With the container that is
 * harmless - the next run gets an empty server - but an external gearmand
 * (see `externalGearmand`) outlives the run, and a leftover from the last one
 * is then handed to the first reader of the next, which reads as the agent
 * answering the wrong check. Naming the queues per run keeps each run's
 * traffic to itself without asking the job server to forget anything.
 */
export const RUN_SUFFIX = `${process.pid.toString(36)}_${Date.now().toString(36).slice(-5)}`;

/** `name` with this run's suffix: `check_results` -> `check_results_9x_k3l2z`. */
export function runQueue(name: string): string {
  return `${name}_${RUN_SUFFIX}`;
}
