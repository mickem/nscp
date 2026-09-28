/** Real command dispatch against independent Node crypto + UDP RADIUS fixtures. */
import * as crypto from "node:crypto";
import * as dgram from "node:dgram";
import * as fs from "node:fs";
import * as path from "node:path";
import type { AddressInfo } from "node:net";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);
const secret = "radius-test-secret";
const password = "a password longer than sixteen bytes";
type Fault =
  | "none"
  | "timeout"
  | "authenticator"
  | "hmac"
  | "missing-ma"
  | "short"
  | "attribute"
  | "wrong-id"
  | "stray";

async function server(code = 2, fault: Fault = "none", ipv6 = false) {
  const socket = dgram.createSocket(ipv6 ? "udp6" : "udp4");
  const errors: string[] = [];
  const requests: Buffer[] = [];
  const users: string[] = [];
  const passwords: string[] = [];
  socket.on("error", (error) => errors.push(String(error)));
  socket.on("message", (request, remote) => {
    try {
      requests.push(request);
      if (request.length !== request.readUInt16BE(2)) throw new Error("Invalid request length");
      const attributes = new Map<number, Buffer>();
      let ma = 0;
      for (let offset = 20; offset < request.length; offset += request[offset + 1]) {
        const length = request[offset + 1];
        if (length < 2 || offset + length > request.length)
          throw new Error("Malformed request attribute");
        attributes.set(request[offset], request.subarray(offset + 2, offset + length));
        if (request[offset] === 80) ma = offset + 2;
      }
      if (!ma) throw new Error("Missing request Message-Authenticator");
      const unsigned = Buffer.from(request);
      unsigned.fill(0, ma, ma + 16);
      if (
        !crypto.timingSafeEqual(
          crypto.createHmac("md5", secret).update(unsigned).digest(),
          request.subarray(ma, ma + 16),
        )
      )
        throw new Error("Invalid request Message-Authenticator");
      if (request[0] === 1) {
        users.push(attributes.get(1)!.toString());
        const encrypted = attributes.get(2)!;
        const plain = Buffer.alloc(encrypted.length);
        let previous: Buffer = request.subarray(4, 20);
        for (let i = 0; i < encrypted.length; i += 16) {
          const hash = crypto.createHash("md5").update(secret).update(previous).digest();
          for (let j = 0; j < 16; ++j) plain[i + j] = encrypted[i + j] ^ hash[j];
          previous = encrypted.subarray(i, i + 16);
        }
        passwords.push(plain.toString().replace(/\0+$/, ""));
      } else if (request[0] !== 12 || attributes.has(1) || attributes.has(2)) {
        throw new Error("Invalid Status-Server request");
      }
      if (fault === "timeout") return;
      const reply = Buffer.concat([
        Buffer.from([code, request[1], 0, 38]),
        request.subarray(4, 20),
        Buffer.from([80, 18]),
        Buffer.alloc(16),
      ]);
      crypto.createHmac("md5", secret).update(reply).digest().copy(reply, 22);
      if (fault === "hmac") reply[22] ^= 1;
      crypto.createHash("md5").update(reply).update(secret).digest().copy(reply, 4);
      if (fault === "authenticator") reply[4] ^= 1;
      if (fault === "attribute") reply[21] = 0;
      if (fault === "wrong-id") reply[1] ^= 1;
      if (fault === "stray") {
        const stranger = dgram.createSocket(ipv6 ? "udp6" : "udp4");
        stranger.send(reply, remote.port, remote.address, () => stranger.close());
        return;
      }
      if (fault === "missing-ma") {
        reply[3] = 20;
        socket.send(reply.subarray(0, 20), remote.port, remote.address);
      } else
        socket.send(fault === "short" ? reply.subarray(0, 10) : reply, remote.port, remote.address);
    } catch (error) {
      errors.push(String(error));
    }
  });
  await new Promise<void>((resolve) => socket.bind(0, ipv6 ? "::1" : "127.0.0.1", resolve));
  return {
    socket,
    port: (socket.address() as AddressInfo).port,
    errors,
    requests,
    users,
    passwords,
  };
}

describe("CheckNet RADIUS", () => {
  let nscp: NscpInstance;
  let secretFile: string;
  let passwordFile: string;
  beforeAll(() => {
    nscp = new NscpInstance();
    const dir = nscp.scratch("radius");
    secretFile = path.join(dir, "secret.txt");
    passwordFile = path.join(dir, "password.txt");
    fs.writeFileSync(secretFile, secret + "\r\n", { mode: 0o600 });
    fs.writeFileSync(passwordFile, password + "\n", { mode: 0o600 });
  });
  async function query(port: number, args: string[] = [], host = "127.0.0.1") {
    const result = await nscp.run(
      [
        "client",
        "--module",
        "CheckNet",
        "--boot",
        "--query",
        "check_radius",
        `host=${host}`,
        `port=${port}`,
        `secret-file=${secretFile}`,
        "timeout=300",
        ...args,
      ],
      { allowFailure: true },
    );
    const output = result.all ?? `${result.stdout}\n${result.stderr}`;
    expect(output).not.toContain(secret);
    expect(output).not.toContain(password);
    return output;
  }
  const auth = () => ["username=test-user", `password-file=${passwordFile}`];

  it("performs PAP including multi-block password hiding and emits timing", async () => {
    const target = await server();
    try {
      const output = await query(target.port, auth());
      expect(output).toMatch(/OK:.*reply=access_accept/);
      expect(output).toMatch(/=\d+ms;/);
      expect(target.users).toEqual(["test-user"]);
      expect(target.passwords).toEqual([password]);
      expect(target.errors).toEqual([]);
    } finally {
      target.socket.close();
    }
  });
  it.each([3, 11])("does not mistake reply %i for successful authentication", async (code) => {
    const target = await server(code);
    try {
      expect(await query(target.port, auth())).toMatch(/CRITICAL:.*unexpected_response/);
    } finally {
      target.socket.close();
    }
  });
  it("explicit reject mode tests responsiveness", async () => {
    const target = await server(3);
    try {
      expect(await query(target.port, ["mode=reject"])).toMatch(/OK:.*reply=access_reject/);
      expect(target.users).toEqual(["nsclient-radius-probe"]);
    } finally {
      target.socket.close();
    }
  });
  it.each([2, 5])("supports authenticated Status-Server response %i", async (code) => {
    const target = await server(code);
    try {
      expect(await query(target.port, ["mode=status"])).toMatch(/OK:/);
      expect(target.requests[0][0]).toBe(12);
      expect(target.errors).toEqual([]);
    } finally {
      target.socket.close();
    }
  });
  it.each<[Fault, string]>([
    ["timeout", "timeout"],
    ["stray", "timeout"],
    ["authenticator", "invalid_authenticator"],
    ["hmac", "invalid_message_authenticator"],
    ["missing-ma", "missing_message_authenticator"],
    ["short", "short_response"],
    ["attribute", "invalid_attribute"],
    ["wrong-id", "wrong_identifier"],
  ])("rejects %s responses without reporting a reply type", async (fault, expected) => {
    const target = await server(2, fault);
    try {
      expect(await query(target.port, auth())).toMatch(
        new RegExp(`CRITICAL:.*${expected}, reply=none`),
      );
      expect(target.errors).toEqual([]);
    } finally {
      target.socket.close();
    }
  });
  it("allows timing thresholds through command argument parsing", async () => {
    const target = await server();
    try {
      expect(await query(target.port, [...auth(), "warning=time >= 0"])).toMatch(/WARNING:/);
    } finally {
      target.socket.close();
    }
  });
  it("works over IPv6", async () => {
    const target = await server(2, "none", true);
    try {
      expect(await query(target.port, [...auth(), "address-family=ipv6"], "::1")).toMatch(/OK:/);
    } finally {
      target.socket.close();
    }
  });
  it("rejects invalid configuration before sending", async () => {
    expect(await query(1812)).toMatch(/auth mode requires username and password-file/);
    expect(await query(1812, ["mode=invalid"])).toMatch(/mode must be auth, reject or status/);
    expect(await query(0, ["mode=status"])).toMatch(/port must be/);
    expect(await query(1812, ["mode=status", "username=unexpected"])).toMatch(
      /status mode does not send a username/,
    );
  });
});
