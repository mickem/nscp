import * as net from "net";

/**
 * A minimal NRPE v2 client over plain TCP, for driving the agent's NRPEServer
 * from the load workers without spawning a client process per request (the
 * `nscp nrpe` CLI the functional suites use costs a process start each, which
 * would measure the runner, not the agent).
 *
 * NRPE is what the stress harness drives the dispatch path through because its
 * socket server has a real worker pool (`thread pool`, 10 by default): two
 * requests run on two threads, so a check that blocks a shared lock in the core
 * or a script engine shows up as other requests stalling. The WEB server runs
 * every request on one thread, so over REST everything serialises regardless
 * (see slow-check-isolation.stress.ts).
 *
 * Wire format (include/net/nrpe/packet.hpp): a 1036-byte packet, big-endian -
 * int16 version (2), int16 type (1 query, 2 response, 3 more-response), uint32
 * CRC32 of the whole packet with this field zeroed, int16 result, then the
 * NUL-terminated payload from offset 10, padded to 1024 bytes plus two bytes
 * of struct alignment. Arguments ride in the payload separated by `!`, which
 * the server only accepts with `allow arguments = true`.
 */

const PAYLOAD_LENGTH = 1024;
const PACKET_LENGTH = 12 + PAYLOAD_LENGTH;
const PAYLOAD_OFFSET = 10;
const VERSION_2 = 2;
const QUERY = 1;
const RESPONSE = 2;
const MORE_RESPONSE = 3;

// CRC-32 (IEEE 802.3, the zlib one NRPE uses), table-driven. Not zlib.crc32:
// that only exists from Node 20.15, and the regular suites also run on the
// Node 18 the Debian/Ubuntu integration job installs.
const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();

export function crc32(buf: Buffer): number {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

function encodeQuery(payload: string): Buffer {
  const body = Buffer.from(payload, "utf8");
  if (body.length >= PAYLOAD_LENGTH) throw new Error(`NRPE payload too long: ${body.length} bytes`);
  const p = Buffer.alloc(PACKET_LENGTH);
  p.writeInt16BE(VERSION_2, 0);
  p.writeInt16BE(QUERY, 2);
  p.writeInt16BE(0, 8);
  body.copy(p, PAYLOAD_OFFSET);
  p.writeUInt32BE(crc32(p), 4);
  return p;
}

interface Packet {
  type: number;
  result: number;
  payload: string;
}

function decode(p: Buffer): Packet {
  const version = p.readInt16BE(0);
  if (version !== VERSION_2) throw new Error(`NRPE answered version ${version}`);
  const crc = p.readUInt32BE(4);
  const copy = Buffer.from(p);
  copy.writeUInt32BE(0, 4);
  if (crc32(copy) !== crc) throw new Error("NRPE answer failed its CRC32");
  const body = p.subarray(PAYLOAD_OFFSET, PAYLOAD_OFFSET + PAYLOAD_LENGTH);
  const end = body.indexOf(0);
  return {
    type: p.readInt16BE(2),
    result: p.readInt16BE(8),
    payload: body.subarray(0, end < 0 ? body.length : end).toString("utf8"),
  };
}

export interface NrpeAnswer {
  /** The Nagios status, 0 OK .. 3 UNKNOWN. */
  result: number;
  /** The whole `message|perfdata` text, every response packet joined. */
  text: string;
  /** The text before the first `|`. */
  message: string;
  /**
   * How long the request waited for a local port before it could be sent at
   * all (see query()). Time the agent never saw: subtract it from a latency
   * that is meant to measure the agent.
   */
  localWaitMs: number;
}

export class NrpeClient {
  constructor(
    private readonly port: number,
    private readonly host = "127.0.0.1",
    private readonly timeoutMs = 120_000,
  ) {}

  /** One query on its own connection, as check_nrpe does. */
  async query(command: string, args: string[] = []): Promise<NrpeAnswer> {
    // NRPE is one TCP connection per request, so a flood of a few thousand a
    // second runs the *client's* ephemeral ports out (TIME_WAIT on Windows
    // holds them for minutes) and connect() fails locally with EADDRINUSE.
    // That request never reached the agent, so it is retried rather than
    // counted; any error after the connection is up still fails the request.
    // On Linux, TIME_WAIT holds a port for 60 s, so a sustained few thousand
    // requests a second exhausts the range; the wait is reported in
    // localWaitMs and counted in localRetries.
    const started = performance.now();
    for (let attempt = 0; ; attempt++) {
      const sent = performance.now();
      try {
        const a = await this.queryOnce(command, args);
        return { ...a, localWaitMs: sent - started };
      } catch (e) {
        if ((e as NodeJS.ErrnoException).code !== "EADDRINUSE" || attempt >= 20) {
          throw new Error(`NRPE ${command}: ${(e as Error).message}`);
        }
        this.localRetries++;
        await new Promise((r) => setTimeout(r, 50 * (attempt + 1)));
      }
    }
  }

  /** Requests that had to wait for a local port, summed over every query. */
  localRetries = 0;

  private queryOnce(command: string, args: string[]): Promise<Omit<NrpeAnswer, "localWaitMs">> {
    const payload = [command, ...args].join("!");
    return new Promise<Omit<NrpeAnswer, "localWaitMs">>((resolve, reject) => {
      const socket = net.connect({ port: this.port, host: this.host });
      const chunks: Buffer[] = [];
      let received = 0;
      const packets: Packet[] = [];
      let settled = false;
      const done = (err?: Error) => {
        if (settled) return;
        settled = true;
        socket.destroy();
        if (err) {
          reject(err);
          return;
        }
        const text = packets.map((p) => p.payload).join("");
        const last = packets[packets.length - 1];
        resolve({ result: last.result, text, message: text.split("|")[0] });
      };
      socket.setTimeout(this.timeoutMs, () => done(new Error(`no answer in ${this.timeoutMs} ms`)));
      socket.on("error", (e) => done(e));
      socket.on("connect", () => socket.write(encodeQuery(payload)));
      socket.on("data", (data) => {
        chunks.push(data);
        received += data.length;
        // Take whole packets off the front; the server sends more-response
        // packets ahead of the final response when the output is long.
        while (received >= PACKET_LENGTH) {
          const all = Buffer.concat(chunks);
          let p: Packet;
          try {
            p = decode(all.subarray(0, PACKET_LENGTH));
          } catch (e) {
            done(e as Error);
            return;
          }
          packets.push(p);
          chunks.length = 0;
          const rest = all.subarray(PACKET_LENGTH);
          if (rest.length) chunks.push(rest);
          received = rest.length;
          if (p.type === RESPONSE) {
            done();
            return;
          }
          if (p.type !== MORE_RESPONSE) {
            done(new Error(`unexpected packet type ${p.type}`));
            return;
          }
        }
      });
      socket.on("close", () => {
        if (!settled) done(new Error(`connection closed after ${packets.length} packet(s)`));
      });
    });
  }

  /** Query and throw unless the status and the message are the ones expected. */
  async expect(command: string, args: string[], message: string, status = 0): Promise<NrpeAnswer> {
    const a = await this.query(command, args);
    if (a.result !== status || a.message !== message) {
      throw new Error(
        `${command} answered ${a.result} ${JSON.stringify(a.message)}, expected ${status} ${JSON.stringify(message)}`,
      );
    }
    return a;
  }
}
