/**
 * `nscp web install` and `nscp web password` store the shared
 * /settings/default/password hashed (pbkdf2-sha256$...), the same form the
 * per-user rows under /settings/WEB/server/users use, and never hash a value
 * that already is one. Runs the CLI against a scratch INI only: no server is
 * started, the assertions read the file back and re-derive the hash with
 * node's PBKDF2 to prove it is a hash *of the password given*.
 */
import { NscpInstance, iniValue, isStoredHash, pbkdf2StoredForm, storedHashMatches } from "@fixtures/index";

jest.setTimeout(120_000);

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
    expect(isStoredHash(shared())).toBe(true);
    expect(storedHashMatches(shared(), "first-secret")).toBe(true);
    expect(storedHashMatches(admin(), "first-secret")).toBe(true);
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
    expect(storedHashMatches(admin(), "first-secret")).toBe(true);
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
    expect(storedHashMatches(shared(), "second-secret")).toBe(true);
    expect(storedHashMatches(admin(), "second-secret")).toBe(true);
    expect(storedHashMatches(admin(), "first-secret")).toBe(false);
  });

  it("web password --set --only-web changes the admin row and leaves the shared default alone", async () => {
    const sharedBefore = shared();
    const r = await nscp.run(["web", "password", "--set", "web-only-secret", "--only-web"]);
    expect(r.all).not.toContain("/settings/default");
    expect(shared()).toBe(sharedBefore);
    expect(storedHashMatches(admin(), "web-only-secret")).toBe(true);
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
    expect(isStoredHash(shared())).toBe(true);
    expect(storedHashMatches(shared(), "legacy-clear")).toBe(true);
  });

  it("a value that already carries the hash prefix is stored as it is", async () => {
    // Copying a hash from another agent must not hash it a second time.
    const copied = pbkdf2StoredForm("copied-secret");
    await nscp.run(["web", "password", "--set", copied]);
    expect(shared()).toBe(copied);
    expect(admin()).toBe(copied);
    expect(storedHashMatches(shared(), "copied-secret")).toBe(true);
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
    expect(isStoredHash(shared())).toBe(true);
    expect(storedHashMatches(shared(), LOOKALIKE)).toBe(true);
    expect(storedHashMatches(admin(), LOOKALIKE)).toBe(true);
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
    expect(storedHashMatches(shared(), LOOKALIKE)).toBe(true);
    expect(storedHashMatches(admin(), LOOKALIKE)).toBe(true);
  });

  it("re-running web install without --password leaves both values alone", async () => {
    // A certificate rotation is a re-run of install, and it must not migrate a
    // key NSCA may be deriving its encryption from - nor touch the admin row,
    // which the operator may have set separately.
    const admin_before = admin();
    await nscp.run(["settings", "--path", SHARED, "--key", "password", "--set", "nsca-shared-key"]);
    expect(shared()).toBe("nsca-shared-key");

    const r = await nscp.run(["web", "install", "--allowed-hosts", "127.0.0.1"]);
    expect(r.all).toContain("Keeping the existing password, which is stored in clear text");
    expect(shared()).toBe("nsca-shared-key");
    expect(admin()).toBe(admin_before);
  });

  it("web install does not revert an admin password diverged with --only-web", async () => {
    // The sequence that used to lose a password: set one, change the admin
    // login on its own, then re-run install for an unrelated reason. The admin
    // row was written from the shared default every time, so hash(A) came back
    // over hash(B) - while install printed "Keeping the existing password".
    const fresh = new NscpInstance();
    const sharedOf = () => iniValue(fresh.settingsFile, SHARED, "password");
    const adminOf = () => iniValue(fresh.settingsFile, ADMIN, "password");

    await fresh.run(["web", "install", "--password", "password-A", "--allowed-hosts", "127.0.0.1"]);
    expect(storedHashMatches(sharedOf(), "password-A")).toBe(true);
    expect(storedHashMatches(adminOf(), "password-A")).toBe(true);

    await fresh.run(["web", "password", "--set", "password-B", "--only-web"]);
    expect(storedHashMatches(adminOf(), "password-B")).toBe(true);
    expect(storedHashMatches(sharedOf(), "password-A")).toBe(true);

    const r = await fresh.run(["web", "install", "--allowed-hosts", "127.0.0.1"]);
    expect(r.all).toContain("Keeping the existing password");
    expect(storedHashMatches(adminOf(), "password-B")).toBe(true);
    expect(storedHashMatches(adminOf(), "password-A")).toBe(false);
    expect(storedHashMatches(sharedOf(), "password-A")).toBe(true);
  });

  it("web install still seeds an admin row that is not there yet", async () => {
    // The gate above must not stop the first install from creating the row:
    // without it the boot loop re-applies the on-disk value and the password
    // install just printed is silently ignored.
    const fresh = new NscpInstance();
    await fresh.run(["web", "install", "--password", "seeded-password", "--allowed-hosts", "127.0.0.1"]);
    expect(storedHashMatches(iniValue(fresh.settingsFile, ADMIN, "password"), "seeded-password")).toBe(true);
    expect(iniValue(fresh.settingsFile, ADMIN, "role")).toBe("full");
  });

  it("web install --password <a real hash> does not tell you to pass --password", async () => {
    const given = pbkdf2StoredForm("given-as-a-hash");
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
});
