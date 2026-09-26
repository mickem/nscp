/**
 * `nscp web install` and `nscp web password` store the shared
 * /settings/default/password hashed (pbkdf2-sha256$...), the same form the
 * per-user rows under /settings/WEB/server/users use, and never hash a value
 * that already is one. Runs the CLI against a scratch INI only: no server is
 * started, the assertions read the file back and re-derive the hash with
 * node's PBKDF2 to prove it is a hash *of the password given*.
 */
import * as crypto from "crypto";
import * as fs from "fs";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const HASH_RE = /^pbkdf2-sha256\$(\d+)\$([0-9a-f]+)\$([0-9a-f]+)$/;

/** Value of `key` under `[section]` in the INI, undefined when absent. */
function iniValue(file: string, section: string, key: string): string | undefined {
  let inSection = false;
  for (const raw of fs.readFileSync(file, "utf8").split(/\r?\n/)) {
    const line = raw.trim();
    if (line === "" || line.startsWith(";") || line.startsWith("#")) continue;
    if (line.startsWith("[")) {
      inSection = line === `[${section}]`;
      continue;
    }
    if (!inSection) continue;
    const eq = line.indexOf("=");
    if (eq < 0) continue;
    if (line.slice(0, eq).trim() === key) return line.slice(eq + 1).trim();
  }
  return undefined;
}

/** True when `stored` is a pbkdf2-sha256 hash of `password` (the KDF in password_hash.cpp). */
function hashMatches(stored: string | undefined, password: string): boolean {
  const m = stored?.match(HASH_RE);
  if (!m) return false;
  const [, iterations, saltHex, hashHex] = m;
  const derived = crypto.pbkdf2Sync(
    password,
    Buffer.from(saltHex, "hex"),
    Number(iterations),
    hashHex.length / 2,
    "sha256",
  );
  return crypto.timingSafeEqual(derived, Buffer.from(hashHex, "hex"));
}

describe("nscp web install / password: the shared default password is stored hashed", () => {
  const SHARED = "/settings/default";
  const ADMIN = "/settings/WEB/server/users/admin";
  let nscp: NscpInstance;

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  const shared = () => iniValue(nscp.settingsFile, SHARED, "password");
  const admin = () => iniValue(nscp.settingsFile, ADMIN, "password");

  it("web install writes a hash of --password to /settings/default and to the admin row", async () => {
    const r = await nscp.run([
      "web",
      "install",
      "--password",
      "first-secret",
      "--allowed-hosts",
      "127.0.0.1",
    ]);
    // The clear text is shown once, at install, and only there.
    expect(r.all).toContain("Login using this password first-secret");
    expect(shared()).toMatch(HASH_RE);
    expect(hashMatches(shared(), "first-secret")).toBe(true);
    expect(hashMatches(admin(), "first-secret")).toBe(true);
    expect(iniValue(nscp.settingsFile, ADMIN, "role")).toBe("full");
  });

  it("re-running web install without --password keeps the hash instead of hashing it again", async () => {
    const before = shared();
    const r = await nscp.run(["web", "install", "--allowed-hosts", "127.0.0.1"]);
    expect(r.all).toContain("Keeping the existing password");
    expect(r.all).not.toContain("first-secret");
    // A hash of the hash would lock the admin out on the next boot.
    expect(shared()).toBe(before);
    expect(admin()).toBe(before);
    expect(hashMatches(admin(), "first-secret")).toBe(true);
  });

  it("web password --display does not reveal a hashed password", async () => {
    const r = await nscp.run(["web", "password", "--display"]);
    expect(r.all).toContain("stored hashed and cannot be displayed");
    expect(r.all).not.toContain("first-secret");
    expect(r.all).not.toContain("pbkdf2-sha256$");
  });

  it("web password --set rotates the shared default and the admin row, both hashed", async () => {
    const r = await nscp.run(["web", "password", "--set", "second-secret"]);
    expect(r.all).toContain("stored hashed");
    expect(r.all).toContain("/settings/default");
    expect(r.all).toContain("admin user");
    expect(hashMatches(shared(), "second-secret")).toBe(true);
    expect(hashMatches(admin(), "second-secret")).toBe(true);
    expect(hashMatches(admin(), "first-secret")).toBe(false);
  });

  it("web password --set --only-web changes the admin row and leaves the shared default alone", async () => {
    const sharedBefore = shared();
    const r = await nscp.run(["web", "password", "--set", "web-only-secret", "--only-web"]);
    expect(r.all).not.toContain("/settings/default");
    expect(shared()).toBe(sharedBefore);
    expect(hashMatches(admin(), "web-only-secret")).toBe(true);
  });

  it("a clear-text value written by hand still verifies and is shown by --display", async () => {
    await nscp.run(["settings", "--path", SHARED, "--key", "password", "--set", "legacy-clear"]);
    expect(shared()).toBe("legacy-clear");
    const r = await nscp.run(["web", "password", "--display"]);
    expect(r.all).toContain("Current password: legacy-clear");
    expect(r.all).toContain("stored in clear text");
  });

  it("re-setting the same clear-text value is how an existing password gets hashed in place", async () => {
    await nscp.run(["web", "password", "--set", "legacy-clear"]);
    expect(shared()).toMatch(HASH_RE);
    expect(hashMatches(shared(), "legacy-clear")).toBe(true);
  });

  it("a value that already carries the hash prefix is stored as it is", async () => {
    // Copying a hash from another agent must not hash it a second time.
    const salt = crypto.randomBytes(16);
    const hash = crypto.pbkdf2Sync("copied-secret", salt, 100000, 32, "sha256");
    const copied = `pbkdf2-sha256$100000$${salt.toString("hex")}$${hash.toString("hex")}`;
    await nscp.run(["web", "password", "--set", copied]);
    expect(shared()).toBe(copied);
    expect(admin()).toBe(copied);
    expect(hashMatches(shared(), "copied-secret")).toBe(true);
  });

  // A password is only text, so it may start with "pbkdf2-sha256$" without
  // being a hash. Deciding on the prefix alone stored such a password
  // unchanged: in the clear, and in a form nothing could verify, so the
  // operator was locked out by a command that reported success.
  const LOOKALIKE = "pbkdf2-sha256$my-secret";

  it("web password --set hashes a password that only looks like a hash", async () => {
    const r = await nscp.run(["web", "password", "--set", LOOKALIKE]);
    expect(r.all).toContain("stored hashed");
    expect(shared()).not.toBe(LOOKALIKE);
    expect(shared()).toMatch(HASH_RE);
    expect(hashMatches(shared(), LOOKALIKE)).toBe(true);
    expect(hashMatches(admin(), LOOKALIKE)).toBe(true);
  });

  it("web password --display refuses the hash it made of it, rather than echoing the password", async () => {
    const r = await nscp.run(["web", "password", "--display"]);
    expect(r.all).toContain("stored hashed and cannot be displayed");
    expect(r.all).not.toContain("my-secret");
  });

  it("web install hashes a --password that only looks like a hash", async () => {
    const r = await nscp.run([
      "web",
      "install",
      "--password",
      LOOKALIKE,
      "--allowed-hosts",
      "127.0.0.1",
    ]);
    expect(r.all).toContain(`Login using this password ${LOOKALIKE}`);
    expect(shared()).not.toBe(LOOKALIKE);
    expect(hashMatches(shared(), LOOKALIKE)).toBe(true);
    expect(hashMatches(admin(), LOOKALIKE)).toBe(true);
  });

  it("re-running web install without --password leaves a clear-text shared value alone", async () => {
    // A certificate rotation is a re-run of install, and it must not migrate a
    // key NSCA may be deriving its encryption from. The admin row is this
    // command's own, so that one is still hashed.
    await nscp.run(["settings", "--path", SHARED, "--key", "password", "--set", "nsca-shared-key"]);
    expect(shared()).toBe("nsca-shared-key");

    const r = await nscp.run(["web", "install", "--allowed-hosts", "127.0.0.1"]);
    expect(r.all).toContain("Keeping the existing password, which is stored in clear text");
    expect(shared()).toBe("nsca-shared-key");
    expect(hashMatches(admin(), "nsca-shared-key")).toBe(true);
  });

  it("web install --password <a real hash> does not tell you to pass --password", async () => {
    const salt = crypto.randomBytes(16);
    const hash = crypto.pbkdf2Sync("given-as-a-hash", salt, 100000, 32, "sha256");
    const given = `pbkdf2-sha256$100000$${salt.toString("hex")}$${hash.toString("hex")}`;
    const r = await nscp.run([
      "web",
      "install",
      "--password",
      given,
      "--allowed-hosts",
      "127.0.0.1",
    ]);
    expect(r.all).toContain("Password stored as given");
    expect(r.all).not.toContain("pass --password to set a new one");
    expect(shared()).toBe(given);
    expect(admin()).toBe(given);
  });

  describe("the NSCA warning", () => {
    const NSCA = "/settings/NSCA/server";

    // NSCAServer encrypts with the password rather than comparing against it,
    // so hashing the shared value stops it loading. Both writers say so on the
    // spot; the upgrade note alone is easy to miss.
    beforeAll(async () => {
      await nscp.run(["settings", "--path", "/modules", "--key", "NSCAServer", "--set", "enabled"]);
    });

    it("web password --set warns when NSCA would lose its key", async () => {
      const r = await nscp.run(["web", "password", "--set", "rotated-secret"]);
      expect(r.all).toContain("NSCAServer is enabled with encryption");
      expect(r.all).toContain(NSCA);
    });

    it("web install --password warns too", async () => {
      const r = await nscp.run([
        "web",
        "install",
        "--password",
        "installed-secret",
        "--allowed-hosts",
        "127.0.0.1",
      ]);
      expect(r.all).toContain("NSCAServer is enabled with encryption");
    });

    it("stays quiet once NSCA has a password of its own", async () => {
      await nscp.run(["settings", "--path", NSCA, "--key", "password", "--set", "the-nsca-key"]);
      const r = await nscp.run(["web", "password", "--set", "rotated-again"]);
      expect(r.all).toContain("stored hashed");
      expect(r.all).not.toContain("NSCAServer is enabled with encryption");
    });

    it("stays quiet when NSCA runs without encryption", async () => {
      await nscp.run(["settings", "--path", NSCA, "--key", "password", "--set", ""]);
      await nscp.run(["settings", "--path", NSCA, "--key", "encryption", "--set", "none"]);
      const r = await nscp.run(["web", "password", "--set", "rotated-once-more"]);
      expect(r.all).toContain("stored hashed");
      expect(r.all).not.toContain("NSCAServer is enabled with encryption");
    });
  });
});
