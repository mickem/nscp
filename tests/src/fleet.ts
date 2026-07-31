/**
 * Helpers for the fleet onboarding / sync suites (fleet-sync*.test.ts).
 *
 * Bundles are built here rather than with a zip dependency: the agent reads
 * them with its own unzip backend, so a hand-written store-method archive with
 * a real CRC32 is both sufficient and exact about what ends up in the file -
 * which matters for the hostile cases (path traversal, oversized entries).
 */
import crypto from "crypto";
import forge from "node-forge";

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c;
  }
  return t;
})();

function crc32(buf: Buffer): number {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

export interface ZipEntry {
  name: string;
  data: string | Buffer;
  /**
   * Uncompressed size to declare in the headers, overriding the real one.
   * Only for tests that need a lying archive (the zip-bomb guard reads the
   * declared size before unpacking anything).
   */
  declaredSize?: number;
}

/** A ZIP archive (store method, real CRC32) containing exactly `entries`. */
export function makeZip(entries: ZipEntry[]): Buffer {
  const locals: Buffer[] = [];
  const centrals: Buffer[] = [];
  let offset = 0;
  for (const e of entries) {
    const name = Buffer.from(e.name, "utf8");
    const data = Buffer.isBuffer(e.data) ? e.data : Buffer.from(e.data, "utf8");
    const declared = e.declaredSize ?? data.length;
    const crc = crc32(data);
    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4); // version needed
    local.writeUInt16LE(0x21, 12); // dos date 1980-01-01
    local.writeUInt32LE(crc, 14);
    local.writeUInt32LE(data.length, 18);
    local.writeUInt32LE(declared, 22);
    local.writeUInt16LE(name.length, 26);
    locals.push(local, name, data);
    const central = Buffer.alloc(46);
    central.writeUInt32LE(0x02014b50, 0);
    central.writeUInt16LE(20, 4);
    central.writeUInt16LE(20, 6);
    central.writeUInt16LE(0x21, 14);
    central.writeUInt32LE(crc, 16);
    central.writeUInt32LE(data.length, 20);
    central.writeUInt32LE(declared, 24);
    central.writeUInt16LE(name.length, 28);
    central.writeUInt32LE(offset, 42);
    centrals.push(central, name);
    offset += 30 + name.length + data.length;
  }
  const cd = Buffer.concat(centrals);
  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(0x06054b50, 0);
  eocd.writeUInt16LE(entries.length, 8);
  eocd.writeUInt16LE(entries.length, 10);
  eocd.writeUInt32LE(cd.length, 12);
  eocd.writeUInt32LE(offset, 16);
  return Buffer.concat([...locals, cd, eocd]);
}

/** Ed25519 signature over the 32-byte SHA-256 digest, base64 - the fleet wire format. */
export function signBundle(privateKey: crypto.KeyObject, bytes: Buffer): string {
  const digest = crypto.createHash("sha256").update(bytes).digest();
  return crypto.sign(null, digest, privateKey).toString("base64");
}

export function sha256Hex(bytes: Buffer): string {
  return crypto.createHash("sha256").update(bytes).digest("hex");
}

/** A parseable (RSA, self-signed) certificate valid for `days`, for cert_pem. */
export function makeCertPem(days: number, commonName = "fleet-agent"): string {
  const keys = forge.pki.rsa.generateKeyPair(2048);
  const cert = forge.pki.createCertificate();
  cert.publicKey = keys.publicKey;
  cert.serialNumber = "01";
  cert.validity.notBefore = new Date(Date.now() - 3600_000);
  cert.validity.notAfter = new Date(Date.now() + days * 24 * 3600_000);
  const attrs = [{ name: "commonName", value: commonName }];
  cert.setSubject(attrs);
  cert.setIssuer(attrs);
  cert.sign(keys.privateKey);
  return forge.pki.certificateToPem(cert);
}

/** One bundle as it appears in a desired-state response. */
export function bundleEntry(id: string, zip: Buffer, signingKey: crypto.KeyObject, extra: Record<string, unknown> = {}) {
  return {
    id,
    name: id,
    version: "1.0",
    sha256: sha256Hex(zip),
    signature: signBundle(signingKey, zip),
    url: `/agent/v1/bundles/${id}`,
    priority: 100,
    ...extra,
  };
}

/** Where a verified bundle is cached on disk (id + first 16 digest chars). */
export function cacheFileName(id: string, zip: Buffer): string {
  return `${id}-${sha256Hex(zip).substring(0, 16)}.zip`;
}
