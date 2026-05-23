import * as forge from "node-forge";
import * as fs from "fs";
import * as path from "path";

export interface CertPair {
  certPath: string;
  keyPath: string;
  certPem: string;
  keyPem: string;
}

export interface CertBundle {
  ca: CertPair;
  /** map keyed by the commonName the caller supplied. */
  signed: Record<string, CertPair>;
}

export interface GenerateOptions {
  outDir: string;
  caCommonName?: string;
  /**
   * Certs to issue under the CA.
   *
   *   { server: { commonName: "localhost", isServer: true },
   *     client: { commonName: "localhost" },
   *     denied: { commonName: "denied-client" } }
   */
  signed: Record<string, { commonName: string; isServer?: boolean }>;
  /** Validity in days. Default 365. */
  days?: number;
}

/**
 * Generate a self-signed CA plus a configurable set of certs signed by it.
 * Drops .crt / .key files into outDir and returns the parsed bundle.
 *
 * Replaces the openssl-CLI cert generation used by tests/nrpe/run-test.bat
 * so the tests don't depend on an openssl binary being on PATH.
 */
export function generateCertChain(opts: GenerateOptions): CertBundle {
  fs.mkdirSync(opts.outDir, { recursive: true });
  const days = opts.days ?? 365;
  const caKp = forge.pki.rsa.generateKeyPair(2048);
  const caCert = makeCert({
    subjectCommonName: opts.caCommonName ?? "root-ca",
    issuerCommonName: opts.caCommonName ?? "root-ca",
    publicKey: caKp.publicKey,
    issuerPrivateKey: caKp.privateKey,
    isCa: true,
    days,
    serial: "01",
  });
  const ca = writePair(opts.outDir, "ca", caCert, caKp.privateKey);

  const signed: Record<string, CertPair> = {};
  let serial = 2;
  for (const [name, spec] of Object.entries(opts.signed)) {
    const kp = forge.pki.rsa.generateKeyPair(2048);
    const cert = makeCert({
      subjectCommonName: spec.commonName,
      issuerCommonName: opts.caCommonName ?? "root-ca",
      publicKey: kp.publicKey,
      issuerPrivateKey: caKp.privateKey,
      isCa: false,
      isServer: spec.isServer === true,
      days,
      serial: serial.toString(16).padStart(2, "0"),
    });
    signed[name] = writePair(opts.outDir, name, cert, kp.privateKey);
    serial += 1;
  }
  return { ca, signed };
}

interface MakeCertArgs {
  subjectCommonName: string;
  issuerCommonName: string;
  publicKey: forge.pki.rsa.PublicKey;
  issuerPrivateKey: forge.pki.rsa.PrivateKey;
  isCa: boolean;
  isServer?: boolean;
  days: number;
  serial: string;
}

function makeCert(args: MakeCertArgs): forge.pki.Certificate {
  const cert = forge.pki.createCertificate();
  cert.publicKey = args.publicKey;
  cert.serialNumber = args.serial;
  cert.validity.notBefore = new Date();
  cert.validity.notAfter = new Date();
  cert.validity.notAfter.setDate(cert.validity.notBefore.getDate() + args.days);
  cert.setSubject([{ name: "commonName", value: args.subjectCommonName }]);
  cert.setIssuer([{ name: "commonName", value: args.issuerCommonName }]);

  // node-forge typings don't expose a public Extension union; setExtensions
  // accepts a loosely-typed array of attribute records (see node-forge's
  // x509.js). Use unknown[] + cast on the way in.
  const extensions: Record<string, unknown>[] = [];
  if (args.isCa) {
    extensions.push({ name: "basicConstraints", cA: true });
    extensions.push({
      name: "keyUsage",
      keyCertSign: true,
      cRLSign: true,
      digitalSignature: true,
    });
  } else {
    extensions.push({ name: "basicConstraints", cA: false });
    extensions.push({
      name: "keyUsage",
      digitalSignature: true,
      keyEncipherment: true,
    });
    extensions.push({
      name: "extKeyUsage",
      serverAuth: args.isServer === true,
      clientAuth: true,
    });
    if (args.isServer) {
      extensions.push({
        name: "subjectAltName",
        altNames: [
          { type: 2, value: "localhost" },
          { type: 7, ip: "127.0.0.1" },
        ],
      });
    }
  }
  cert.setExtensions(extensions as unknown as Parameters<typeof cert.setExtensions>[0]);
  cert.sign(args.issuerPrivateKey, forge.md.sha256.create());
  return cert;
}

function writePair(
  dir: string,
  name: string,
  cert: forge.pki.Certificate,
  key: forge.pki.rsa.PrivateKey,
): CertPair {
  const certPem = forge.pki.certificateToPem(cert);
  const keyPem = forge.pki.privateKeyToPem(key);
  const certPath = path.join(dir, `${name}.crt`);
  const keyPath = path.join(dir, `${name}.key`);
  fs.writeFileSync(certPath, certPem);
  fs.writeFileSync(keyPath, keyPem);
  return { certPath, keyPath, certPem, keyPem };
}
