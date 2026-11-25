import { describe, expect, it } from "vitest";
import { Settings } from "../../api/api";
import { pendingChanges, SettingChange } from "../kit/changes";
import { CUSTOM } from "../kit/modes";
import {
  argumentsChanges,
  ArgumentsMode,
  DEFAULT_CA,
  DEFAULT_CERTIFICATE,
  detectArguments,
  detectSecurity,
  EXTERNAL_SCRIPTS_PATH,
  INSECURE_CIPHERS,
  NRPE_PATH,
  fieldChange,
  identityProblem,
  readField,
  readTlsFiles,
  SecurityMode,
  securityChanges,
  securityProblem,
} from "./nrpe";

const nrpe = (key: string, value: string): Settings => ({ path: NRPE_PATH, key, value });

// What the agent would store after these changes: a value replaces, null removes.
function apply(stored: Settings[], changes: SettingChange[]): Settings[] {
  let out = [...stored];
  for (const c of changes) {
    out = out.filter((s) => !(s.path === c.path && s.key === c.key));
    if (c.value !== null) out.push({ path: c.path, key: c.key, value: c.value });
  }
  return out;
}

const FILES = {
  certificate: "${certificate-path}/server.pem",
  certificateKey: "${certificate-path}/server.key",
  ca: "${certificate-path}/ca.pem",
};

// Starting points a real agent can be in.
const STATES: Record<string, Settings[]> = {
  "fresh install": [],
  "legacy insecure": [
    nrpe("insecure", "true"),
    nrpe("allowed ciphers", INSECURE_CIPHERS),
    nrpe("verify mode", "none"),
  ],
  "the earlier web wizard, one-way": [
    nrpe("insecure", "false"),
    nrpe("ssl", "true"),
    nrpe("allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"),
    nrpe("ssl options", "no-sslv2,no-sslv3"),
    nrpe("verify mode", "none"),
    nrpe("certificate", "c.pem"),
  ],
  "hand-written two-way": [
    nrpe("verify mode", "peer,fail-if-no-cert"),
    nrpe("certificate", "server.pem"),
    nrpe("ca", "ca.pem"),
  ],
  "something odd": [nrpe("verify mode", "client-once"), nrpe("use ssl", "false")],
};

describe("NRPE security modes", () => {
  it("reads a fresh install as one-way TLS", () => {
    // verify mode defaults to none, certificate to certificate.pem.
    expect(detectSecurity([])).toBe("one-way-tls");
  });

  it.each([
    ["peer-cert", "two-way-tls"],
    ["peer,fail-if-no-cert", "two-way-tls"],
    ["certificate,client-certificate", "two-way-tls"],
    ["fail-if-no-peer-cert,peer", "two-way-tls"],
    ["none", "one-way-tls"],
    ["", "one-way-tls"],
    // A client certificate is checked when offered but not required.
    ["peer", CUSTOM],
    ["client-once", CUSTOM],
    // The agent does not trim, and refuses to start on a token it does not know.
    ["peer, fail-if-no-cert", CUSTOM],
    ["bogus", CUSTOM],
  ])("reads verify mode %j as %s", (verify, mode) => {
    expect(detectSecurity([nrpe("verify mode", verify)])).toBe(mode);
  });

  it("reads the insecure flag together with what it implies", () => {
    expect(detectSecurity([nrpe("insecure", "true")])).toBe("insecure");
    // Insecure, but with a certificate or strong ciphers set by hand: neither mode.
    expect(detectSecurity([nrpe("insecure", "true"), nrpe("certificate", "c.pem")])).toBe(CUSTOM);
    expect(detectSecurity([nrpe("insecure", "true"), nrpe("allowed ciphers", "HIGH")])).toBe(
      CUSTOM,
    );
    // Not insecure, but with the insecure ciphers: neither mode either.
    expect(detectSecurity([nrpe("allowed ciphers", INSECURE_CIPHERS)])).toBe(CUSTOM);
  });

  it("reads plain-text NRPE as custom", () => {
    expect(detectSecurity([nrpe("use ssl", "false")])).toBe(CUSTOM);
  });

  it("reads a two-way mode with no CA as custom", () => {
    expect(detectSecurity([nrpe("verify mode", "peer-cert"), nrpe("ca", "")])).toBe(CUSTOM);
  });

  const MODES: SecurityMode[] = ["two-way-tls", "one-way-tls", "insecure"];
  for (const [name, state] of Object.entries(STATES)) {
    for (const mode of MODES) {
      it(`writes ${mode} so that it reads back as ${mode}, from ${name}`, () => {
        expect(detectSecurity(apply(state, securityChanges(mode, FILES)))).toBe(mode);
      });
    }
  }

  it("writes nothing when the mode is already in effect", () => {
    const state = apply([], securityChanges("two-way-tls", FILES));
    expect(pendingChanges(state, securityChanges("two-way-tls", FILES))).toEqual([]);
    expect(pendingChanges([], securityChanges("one-way-tls", readTlsFiles([])))).toEqual([]);
  });

  it("follows the certificate default that depends on the insecure flag", () => {
    expect(readTlsFiles([]).certificate).toBe(DEFAULT_CERTIFICATE);
    expect(readTlsFiles([nrpe("insecure", "true")]).certificate).toBe("");
    expect(readTlsFiles([]).ca).toBe(DEFAULT_CA);
    // Leaving insecure mode brings the default certificate back without writing it.
    const out = apply([nrpe("insecure", "true")], securityChanges("one-way-tls", readTlsFiles([])));
    expect(out.find((s) => s.key === "certificate")?.value).toBe(DEFAULT_CERTIFICATE);
  });

  it("refuses TLS without the files it needs", () => {
    expect(securityProblem("one-way-tls", { ...FILES, certificate: " " })).toMatch(/certificate/);
    expect(securityProblem("two-way-tls", { ...FILES, ca: "" })).toMatch(/CA/);
    expect(securityProblem("one-way-tls", { ...FILES, ca: "" })).toBeNull();
    expect(securityProblem("insecure", { certificate: "", certificateKey: "", ca: "" })).toBeNull();
  });
});

describe("NRPE arguments", () => {
  const scripts = (value: string): Settings => ({
    path: EXTERNAL_SCRIPTS_PATH,
    key: "allow arguments",
    value,
  });

  it("reads the two flags", () => {
    expect(detectArguments([])).toBe("none");
    expect(detectArguments([scripts("true")])).toBe("none");
    expect(detectArguments([nrpe("allow arguments", "true")])).toBe("built-ins");
    expect(detectArguments([nrpe("allow arguments", "1"), scripts("true")])).toBe("all");
  });

  const MODES: ArgumentsMode[] = ["none", "built-ins", "all"];
  for (const from of MODES) {
    for (const to of MODES) {
      it(`goes from ${from} to ${to}`, () => {
        const start = apply([], argumentsChanges(from));
        expect(detectArguments(apply(start, argumentsChanges(to)))).toBe(to);
      });
    }
  }

  it("leaves the scripts flag alone when switching arguments off", () => {
    // It also decides what REST and NSCP callers may pass to scripts.
    expect(argumentsChanges("none").map((c) => c.path)).toEqual([NRPE_PATH]);
  });
});

describe("NRPE single-value fields", () => {
  const shared = (key: string, value: string): Settings => ({
    path: "/settings/default",
    key,
    value,
  });

  it("reads a key from NRPE's own section, then the shared one, then the default", () => {
    expect(readField([], "timeout")).toEqual({ value: "30", source: "built-in" });
    expect(readField([shared("timeout", "60")], "timeout")).toEqual({
      value: "60",
      source: "default section",
    });
    expect(readField([shared("timeout", "60"), nrpe("timeout", "10")], "timeout")).toEqual({
      value: "10",
      source: "own",
    });
    // Not every key inherits: the payload length is NRPE's own.
    expect(readField([shared("payload length", "4096")], "payload length").value).toBe("1024");
  });

  it("follows the extended-response default that depends on the insecure flag", () => {
    expect(readField([], "extended response").value).toBe("true");
    expect(readField([nrpe("insecure", "true")], "extended response").value).toBe("false");
  });

  it("writes only a value that differs from the one in effect, and only to NRPE", () => {
    const stored = [shared("timeout", "60")];
    expect(pendingChanges(stored, [fieldChange(stored, "timeout", "60")])).toEqual([]);
    expect(pendingChanges(stored, [fieldChange(stored, "timeout", "20")])).toEqual([
      { path: NRPE_PATH, key: "timeout", value: "20", defaultValue: "60" },
    ]);
  });

  it("refuses a client identity from the certificate without two-way TLS", () => {
    expect(identityProblem("two-way-tls", "cn")).toBeNull();
    expect(identityProblem("one-way-tls", "cn")).toMatch(/two-way TLS/);
    expect(identityProblem("insecure", "none")).toBeNull();
    expect(identityProblem("one-way-tls", "dn")).toMatch(/not one the agent knows/);
    // A configuration the page does not recognise is the reader's to judge.
    expect(identityProblem(CUSTOM, "cn")).toBeNull();
  });
});
