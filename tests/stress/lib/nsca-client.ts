import * as crypto from "crypto";
import * as net from "net";

import { crc32 } from "./nrpe-client";

/**
 * A minimal NSCA (packet version 3) client, for flooding the agent's
 * NSCAServer from the load workers without a `nscp nsca` process per
 * submission (which would measure process start-up, not the agent).
 *
 * The exchange (include/net/nsca/client/nsca_client_protocol.hpp): the client
 * connects, the server sends an IV packet - 128 bytes of IV and its own Unix
 * time, big-endian - the client builds one data packet stamped with that
 * time, encrypts it with the key and the IV, writes it and hangs up. There is
 * no acknowledgement; whether a submission landed is only visible at the
 * sink, which is why the scenario reads the result cache back.
 *
 * Data packet (include/net/nsca/nsca_packet.hpp, all big-endian):
 * int16 version (3), two bytes of alignment, uint32 CRC32 of the packet with
 * this field zeroed, uint32 timestamp, int16 return code, then from offset 14
 * the host (64 bytes), the service (128 bytes) and the result (`payload
 * length` bytes), each NUL-terminated and zero-padded, plus two bytes of
 * struct alignment at the end: 720 bytes at the protocol's 512.
 *
 * Ciphers, as libs/nscpcrypt/nscpcrypt.cpp speaks them: `none`; `xor`, the
 * packet XORed with the IV and the key, both rotating; and the AES family,
 * CFB mode with an 8-bit feedback (crypto++'s CFB with feedback size 1, which
 * node spells `aes-*-cfb8`), the key being the password zero-padded or cut to
 * the key size and the IV the first 16 of the 128 transmitted bytes. The
 * other eleven ciphers (des, blowfish, serpent, gost, ...) are not here:
 * nsca-loopback.test.ts proves them one by one, and a flood needs a
 * representative cipher, not all of them.
 */

export const NSCA_CIPHERS = ["none", "xor", "aes128", "aes192", "aes256"] as const;
export type NscaCipher = (typeof NSCA_CIPHERS)[number];

export function isNscaCipher(name: string): name is NscaCipher {
  return (NSCA_CIPHERS as readonly string[]).includes(name);
}

const HOST_LENGTH = 64;
const DESC_LENGTH = 128;
/** sizeof(nsca::data::data_packet): 14 bytes of fields and two of alignment. */
const HEADER_LENGTH = 16;
/** Where the host field starts; the service and the result follow it. */
const DATA_OFFSET = 14;
const IV_LENGTH = 128;
const IV_PACKET_LENGTH = IV_LENGTH + 4;
const VERSION_3 = 3;
const AES_KEY_BYTES: Record<string, number> = { aes128: 16, aes192: 24, aes256: 32 };

export interface NscaSubmission {
  /** What the sender claims to be; the server passes it through as the result's host. */
  host: string;
  /** The service name; the key the result cache stores the result under. */
  service: string;
  /** The Nagios status, 0 OK .. 3 UNKNOWN. */
  status: number;
  message: string;
}

/** The clear-text data packet for `s`, stamped with the server's `timestamp`. */
export function encodePacket(s: NscaSubmission, payloadLength: number, timestamp: number): Buffer {
  const p = Buffer.alloc(HEADER_LENGTH + HOST_LENGTH + DESC_LENGTH + payloadLength);
  p.writeInt16BE(VERSION_3, 0);
  p.writeUInt32BE(timestamp, 8);
  p.writeInt16BE(s.status, 12);
  const field = (what: string, value: string, offset: number, max: number): void => {
    const body = Buffer.from(value, "utf8");
    // The server reads the field with strnlen over `max` bytes and refuses
    // one that fills it, so the terminator has to fit.
    if (body.length >= max) throw new Error(`NSCA ${what} too long: ${body.length} >= ${max}`);
    body.copy(p, offset);
  };
  field("host", s.host, DATA_OFFSET, HOST_LENGTH);
  field("service", s.service, DATA_OFFSET + HOST_LENGTH, DESC_LENGTH);
  field("result", s.message, DATA_OFFSET + HOST_LENGTH + DESC_LENGTH, payloadLength);
  p.writeUInt32BE(crc32(p), 4);
  return p;
}

/** `packet` encrypted the way nscpcrypt does it for `cipher`; `xor` works in place. */
export function encryptPacket(
  packet: Buffer,
  cipher: NscaCipher,
  password: string,
  iv: Buffer,
): Buffer {
  if (cipher === "none") return packet;
  if (cipher === "xor") {
    const key = Buffer.from(password, "utf8");
    for (let y = 0; y < packet.length; y++) {
      packet[y] ^= iv[y % IV_LENGTH];
      if (key.length > 0) packet[y] ^= key[y % key.length];
    }
    return packet;
  }
  const keyBytes = AES_KEY_BYTES[cipher];
  const key = Buffer.alloc(keyBytes);
  Buffer.from(password, "utf8").copy(key, 0, 0, keyBytes);
  const c = crypto.createCipheriv(`aes-${keyBytes * 8}-cfb8`, key, iv.subarray(0, 16));
  return Buffer.concat([c.update(packet), c.final()]);
}

export interface NscaClientOptions {
  port: number;
  host?: string;
  cipher: NscaCipher;
  password: string;
  /** The server's `payload length`; default 512. */
  payloadLength?: number;
  timeoutMs?: number;
}

export class NscaClient {
  private readonly port: number;
  private readonly host: string;
  private readonly cipher: NscaCipher;
  private readonly password: string;
  private readonly payloadLength: number;
  private readonly timeoutMs: number;

  /** Submissions that had to wait for a local port, summed over every submit(). */
  localRetries = 0;

  constructor(opts: NscaClientOptions) {
    this.port = opts.port;
    this.host = opts.host ?? "127.0.0.1";
    this.cipher = opts.cipher;
    this.password = opts.password;
    this.payloadLength = opts.payloadLength ?? 512;
    this.timeoutMs = opts.timeoutMs ?? 120_000;
  }

  /**
   * One submission on its own connection, as send_nsca does. Resolves once
   * the packet has been flushed and the connection closed; whether the server
   * accepted it is not something the protocol says.
   *
   * `localWaitMs` is how long the submission waited for a local port before
   * it could connect at all: NSCA is one connection per submission, and a
   * flood runs the client's ephemeral ports into TIME_WAIT (see
   * NrpeClient.query() for the same retry).
   */
  async submit(s: NscaSubmission): Promise<{ localWaitMs: number }> {
    const started = performance.now();
    for (let attempt = 0; ; attempt++) {
      const sent = performance.now();
      try {
        await this.submitOnce(s);
        return { localWaitMs: sent - started };
      } catch (e) {
        if ((e as NodeJS.ErrnoException).code !== "EADDRINUSE" || attempt >= 20) {
          throw new Error(`NSCA ${s.service}: ${(e as Error).message}`);
        }
        this.localRetries++;
        await new Promise((r) => setTimeout(r, 50 * (attempt + 1)));
      }
    }
  }

  private submitOnce(s: NscaSubmission): Promise<void> {
    return new Promise<void>((resolve, reject) => {
      const socket = net.connect({ port: this.port, host: this.host });
      let iv = Buffer.alloc(0);
      let sent = false;
      let settled = false;
      const done = (err?: Error) => {
        if (settled) return;
        settled = true;
        socket.destroy();
        if (err) reject(err);
        else resolve();
      };
      socket.setTimeout(this.timeoutMs, () =>
        done(new Error(`no IV packet in ${this.timeoutMs} ms`)),
      );
      socket.on("error", (e) => done(e));
      socket.on("data", (chunk: Buffer) => {
        // The server sends the IV packet and nothing else.
        if (sent) return;
        iv = Buffer.concat([iv, chunk]);
        if (iv.length < IV_PACKET_LENGTH) return;
        sent = true;
        let packet: Buffer;
        try {
          const timestamp = iv.readUInt32BE(IV_LENGTH);
          packet = encryptPacket(
            encodePacket(s, this.payloadLength, timestamp),
            this.cipher,
            this.password,
            iv.subarray(0, IV_LENGTH),
          );
        } catch (e) {
          done(e as Error);
          return;
        }
        // end() sends the FIN after the packet; the callback fires once both
        // are out, which is as much as the protocol lets a sender know.
        socket.end(packet, () => done());
      });
      socket.on("close", () => {
        if (!settled) {
          done(
            new Error(
              sent
                ? "connection closed before the packet was flushed"
                : `connection closed after ${iv.length} of ${IV_PACKET_LENGTH} IV bytes`,
            ),
          );
        }
      });
    });
  }
}
