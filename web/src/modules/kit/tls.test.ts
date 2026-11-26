import { describe, expect, it } from "vitest";
import { Settings } from "../../api/api";
import { applyToStored, pendingChanges } from "./changes";
import { CUSTOM } from "./modes";
import {
  detectSocketTls,
  readSocketTlsFiles,
  socketTlsChanges,
  SocketTlsDefaults,
  SocketTlsMode,
  tlsFilesProblem,
  tlsVersionRisk,
} from "./tls";

const P = "/settings/X/server";
const s = (key: string, value: string): Settings => ({ path: P, key, value });
const OFF: SocketTlsDefaults = { ssl: false, ciphers: "C", dh: "" };
const ON: SocketTlsDefaults = { ssl: true, ciphers: "C", dh: "" };
const FILES = { certificate: "server.pem", certificateKey: "", ca: "ca.pem" };

describe("socket server TLS", () => {
  it("reads a fresh install by the module's `use ssl` default", () => {
    expect(detectSocketTls([], P, OFF)).toBe("plain");
    expect(detectSocketTls([], P, ON)).toBe("one-way-tls");
  });

  it.each([
    ["peer-cert", "two-way-tls"],
    ["peer,fail-if-no-cert", "two-way-tls"],
    ["none", "one-way-tls"],
    ["peer", CUSTOM],
    ["peer, fail-if-no-cert", CUSTOM],
  ])("reads verify mode %j as %s", (verify, mode) => {
    expect(detectSocketTls([s("use ssl", "true"), s("verify mode", verify)], P, OFF)).toBe(mode);
  });

  it("reads TLS without a certificate, or two-way without a CA, as custom", () => {
    expect(detectSocketTls([s("use ssl", "true"), s("certificate", "")], P, OFF)).toBe(CUSTOM);
    expect(
      detectSocketTls([s("use ssl", "true"), s("verify mode", "peer-cert"), s("ca", "")], P, OFF),
    ).toBe(CUSTOM);
  });

  const STATES: Record<string, Settings[]> = {
    fresh: [],
    "two-way": [s("use ssl", "true"), s("verify mode", "peer-cert")],
    odd: [s("use ssl", "true"), s("verify mode", "client-once")],
  };
  const MODES: SocketTlsMode[] = ["plain", "one-way-tls", "two-way-tls"];
  for (const d of [OFF, ON]) {
    for (const [name, state] of Object.entries(STATES)) {
      for (const mode of MODES) {
        it(`writes ${mode} so that it reads back, from ${name}, ssl default ${d.ssl}`, () => {
          const after = applyToStored(state, socketTlsChanges(P, mode, FILES, d));
          expect(detectSocketTls(after, P, d)).toBe(mode);
        });
      }
    }
  }

  it("writes nothing when the mode is already in effect", () => {
    expect(
      pendingChanges([], socketTlsChanges(P, "plain", readSocketTlsFiles([], P), OFF)),
    ).toEqual([]);
    expect(
      pendingChanges([], socketTlsChanges(P, "one-way-tls", readSocketTlsFiles([], P), ON)),
    ).toEqual([]);
  });

  it("refuses TLS without the files it needs", () => {
    expect(tlsFilesProblem("one-way-tls", { ...FILES, certificate: "" })).toMatch(/certificate/);
    expect(tlsFilesProblem("two-way-tls", { ...FILES, ca: " " })).toMatch(/CA/);
    expect(tlsFilesProblem("plain", { certificate: "", certificateKey: "", ca: "" })).toBeNull();
  });
});

describe("TLS version risk", () => {
  it.each([
    ["tlsv1.2+", false],
    ["tlsv1.3", false],
    ["tlsv1.0+", true],
    ["sslv3", true],
    ["1.2+", false],
    ["1.1", true],
    ["any", true],
  ])("%s outdated: %s", (version, risky) => {
    expect(tlsVersionRisk(version) !== null).toBe(risky);
  });
});
