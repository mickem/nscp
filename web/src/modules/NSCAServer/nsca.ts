// What the NSCA server page knows about NSCAServer's settings
// (modules/NSCAServer/NSCAServer.cpp, libs/nscpcrypt): the keys with their
// defaults, and what the encryption choice means.
import { Settings } from "../../api/api.ts";
import { readStored } from "../kit/changes.ts";
import { fixed, KeyTable, tableIni } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";
import { coreServerKeys } from "../kit/socketServer.ts";
import { socketTlsKeys, SocketTlsDefaults } from "../kit/tls.ts";

export const NSCA_MODULE = "NSCAServer";
export const NSCA_PATH = "/settings/NSCA/server";

export const NSCA_TLS: SocketTlsDefaults = {
  ssl: false,
  ciphers: "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH",
  dh: "",
};

export const NSCA_TABLE: KeyTable = {
  [NSCA_PATH]: {
    port: { builtIn: fixed("5667") },
    ...coreServerKeys(),
    encryption: { builtIn: fixed("aes256") },
    // The encryption key, inherited from nowhere: /settings/default holds a
    // hashed password to verify callers against, which is no use as a key.
    password: { builtIn: fixed("") },
    inbox: { builtIn: fixed("inbox"), inherits: true },
    timezone: { builtIn: fixed("utc") },
    "performance data": { builtIn: fixed("true") },
    "payload length": { builtIn: fixed("512") },
    ...socketTlsKeys(NSCA_TLS),
  },
};

// The agent also registers the socket servers' TLS options here, but NSCA
// protects results with its own encryption: the page leaves TLS to expert mode.
export const NSCA_TLS_KEYS = Object.keys(socketTlsKeys(NSCA_TLS));

// The page offers the one secure cipher; any other is set in expert mode.
// nscpcrypt reads aes and rijndael256 as the same cipher.
export const SECURE_ENCRYPTION = "aes256";
const SECURE_SPELLINGS = new Set(["aes256", "aes", "rijndael256"]);

export function isSecureEncryption(encryption: string): boolean {
  return SECURE_SPELLINGS.has(encryption);
}

// What the encryption field offers: aes256, plus whatever is stored now so
// it is shown rather than silently replaced.
export function encryptionOptions(current: string): string[] {
  return isSecureEncryption(current) || current === ""
    ? [current || SECURE_ENCRYPTION]
    : [SECURE_ENCRYPTION, current];
}

// Whether TLS was turned on (in expert mode): standard send_nsca cannot
// connect then.
export function tlsOn(stored: Settings[] | undefined): boolean {
  const v = (readStored(stored, NSCA_PATH, "use ssl") ?? "false").toLowerCase();
  return v === "true" || v === "1";
}

const STRONG = new Set([
  "aes256",
  "aes",
  "rijndael256",
  "aes192",
  "rijndael192",
  "aes128",
  "rijndael128",
  "serpent",
  "twofish",
]);

export function encryptionRisk(encryption: string, tls: boolean): Risk | null {
  if (encryption === "none" || encryption === "") {
    return tls
      ? null
      : {
          severity: "error",
          title: "Results are accepted unencrypted",
          reason:
            "Without NSCA encryption or TLS, any host on the allowed-hosts list can submit results, and anyone on the network path can read or change them.",
        };
  }
  if (encryption === "xor") {
    return {
      severity: "error",
      title: "xor is not encryption",
      reason: "It hides nothing from anyone who looks. Use aes256 if the sending hosts support it.",
    };
  }
  if (!STRONG.has(encryption)) {
    return {
      severity: "warning",
      title: `${encryption} is an old cipher`,
      reason: "Use aes256 if the sending hosts support it; every sender has to use the same one.",
    };
  }
  return null;
}

// With encryption and no key the module loads but opens no listener, since
// an empty key is a well-known one.
export function passwordProblem(encryption: string, password: string): string | null {
  if (encryption === "none" || encryption === "") return null;
  return password === ""
    ? "NSCA encryption needs a password (the shared key every sending host uses); without one the server opens no port."
    : null;
}

export function nscaIni(stored: Settings[] | undefined, includeDefaults: boolean): string {
  return tableIni(stored, NSCA_MODULE, NSCA_TABLE, includeDefaults);
}
