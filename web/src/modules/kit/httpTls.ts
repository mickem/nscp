// The TLS keys of the HTTP listeners (WEB, NCPA): a certificate, a key, a
// version range, a cipher list and whether a missing certificate may mean
// serving in clear.
import { fixed, KeySpec } from "./keys.ts";
import type { Risk } from "./RiskBanner.tsx";
import { DEFAULT_CERTIFICATE } from "./tls.ts";

export function httpTlsKeys(): Record<string, KeySpec> {
  return {
    certificate: { builtIn: fixed(DEFAULT_CERTIFICATE) },
    "certificate key": { builtIn: fixed("") },
    "tls version": { builtIn: fixed("1.2+") },
    "allowed ciphers": { builtIn: fixed("") },
    "allow insecure": { builtIn: fixed("false") },
  };
}

const isTrue = (v: string) => v.toLowerCase() === "true" || v === "1";

// `what` names what a clear connection gives away: "the token", "passwords
// and session keys".
export function insecureRisk(allowInsecure: string, what: string): Risk | null {
  return isTrue(allowInsecure)
    ? {
        severity: "warning",
        title: "A missing certificate means serving in clear",
        reason: `If the certificate file is missing, the listener starts on plain HTTP and ${what} cross the network unencrypted. Only for a TLS-terminating proxy or loopback.`,
      }
    : null;
}

// Both listeners share the WEB server's failed-login limiter.
export function rateLimitRisk(maxFailures: string): Risk | null {
  return maxFailures.trim() === "0"
    ? {
        severity: "warning",
        title: "Failed logins are not limited",
        reason: "With the limit at 0, a caller can guess credentials as fast as it likes.",
      }
    : null;
}
