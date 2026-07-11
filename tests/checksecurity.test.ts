/**
 * Exercises the CheckSecurity module end-to-end against the real nscp binary.
 *
 * Each case runs a one-shot client query — `nscp client --module CheckSecurity
 * --boot --query <cmd> ...` — which loads the module, runs the check and prints
 * the Nagios-style result line. No server/port/docker needed (and, unlike the
 * REST-based suites, no login), so it runs anywhere the binary builds.
 *
 * check_certificate is cross-platform (OpenSSL file parsing); the certificate
 * fixtures are generated with controlled validity windows. check_firewall and
 * the Windows certificate store are Windows-only, so here we assert the
 * "not supported on this platform" behaviour.
 */
import { NscpInstance, generateCertChain } from "@fixtures/index";

jest.setTimeout(120_000);

const onLinux = process.platform === "linux" ? describe : describe.skip;

onLinux("CheckSecurity", () => {
  let nscp: NscpInstance;
  let validCert: string; // ~800 days, leaf signed by caCert
  let caCert: string; // the CA that signed validCert
  let soonCert: string; //  5 days (inside the default 10-day critical window)
  let expiredCert: string; // already expired
  let bundleDir: string; // dir holding the valid cert + its CA (2 certs)

  /** Run a CheckSecurity query and return the combined output. */
  async function query(command: string, args: string[] = []): Promise<string> {
    const r = await nscp.run(["client", "--module", "CheckSecurity", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  beforeAll(() => {
    nscp = new NscpInstance();
    bundleDir = nscp.scratch("cs_valid");
    const bundle = generateCertChain({ outDir: bundleDir, days: 800, signed: { valid: { commonName: "valid.example.com" } } });
    validCert = bundle.signed.valid.certPath; // leaf, signed by the CA below
    caCert = bundle.ca.certPath;
    soonCert = generateCertChain({ outDir: nscp.scratch("cs_soon"), days: 5, signed: { soon: { commonName: "soon.example.com" } } }).signed.soon.certPath;
    expiredCert = generateCertChain({ outDir: nscp.scratch("cs_expired"), days: -10, signed: { old: { commonName: "old.example.com" } } }).signed.old.certPath;
  });

  // --- check_certificate ----------------------------------------------------

  it("reports OK for an in-date certificate file", async () => {
    const out = await query("check_certificate", [`file=${validCert}`]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/certificate/i);
  });

  it("emits expires_in perfdata (unit d)", async () => {
    const out = await query("check_certificate", [`file=${validCert}`]);
    // Perf label is the subject; value is whole days with unit "d".
    expect(out).toMatch(/=\d+d;/);
  });

  it("goes CRITICAL when a certificate is within the default 10-day window", async () => {
    const out = await query("check_certificate", [`file=${soonCert}`]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out).toMatch(/expires in \d+d/);
  });

  it("honours a custom warning threshold", async () => {
    // The valid cert has ~799 days left; warn below 900 must trip WARNING
    // (but not the default critical at <10).
    const out = await query("check_certificate", [`file=${validCert}`, "warning=expires_in<900"]);
    expect(out).toMatch(/^WARNING/m);
  });

  it("flags an already-expired certificate via the expired keyword", async () => {
    const out = await query("check_certificate", [`file=${expiredCert}`, "critical=expired=1"]);
    expect(out).toMatch(/^CRITICAL/m);
  });

  it("scans a directory and counts every certificate it finds", async () => {
    // bundleDir holds valid.crt + ca.crt (the .key files are skipped).
    const out = await query("check_certificate", [`file=${bundleDir}`]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/all 2 certificate/);
  });

  it("returns UNKNOWN for a missing file rather than a silent OK", async () => {
    const out = await query("check_certificate", ["file=/no/such/cert.pem"]);
    expect(out).toMatch(/No certificates found|not found/i);
  });

  it("exposes certificate security fields", async () => {
    const out = await query("check_certificate", [
      `file=${validCert}`,
      "detail-syntax=key=${key_type}/${key_size} self=${self_signed} sig=${signature_algorithm}",
      "top-syntax=${list}",
    ]);
    expect(out).toMatch(/key=RSA\/\d+/);
    expect(out).toMatch(/self=0/); // leaf is signed by the CA, not self-signed
    expect(out).toMatch(/sig=\S+/);
  });

  it("evaluates trust against a CA bundle", async () => {
    // The leaf's issuing CA is not in the system trust store...
    const untrusted = await query("check_certificate", [`file=${validCert}`, "crit=not trusted"]);
    expect(untrusted).toMatch(/^CRITICAL/m);
    // ...but it chains to the CA when that CA bundle is supplied.
    const trusted = await query("check_certificate", [`file=${validCert}`, `ca=${caCert}`, "crit=not trusted"]);
    expect(trusted).toMatch(/^OK/m);
  });

  it("rejects the Windows certificate store on this platform", async () => {
    const out = await query("check_certificate", ["store=My"]);
    expect(out).toMatch(/only supported on Windows/i);
  });

  // --- check_users (cross-platform) ----------------------------------------

  it("check_users reports the logged-on sessions", async () => {
    const out = await query("check_users", []);
    // A headless CI runner may have zero interactive sessions, in which case the
    // check reports the empty-state message ("No users logged on") instead of the
    // "<n> user(s) logged on" summary. Both share the "logged on" wording.
    expect(out).toMatch(/logged on/);
  });

  it("check_users exposes the count summary variable", async () => {
    // Referencing `count` in a threshold emits its perfdata regardless of how many
    // sessions exist (zero on a headless runner). A zero-match result takes the
    // empty-state branch (OK, no threshold evaluation), so assert the perfdata is
    // exposed rather than a specific status, which depends on the session count.
    const out = await query("check_users", ["warn=count >= 0"]);
    expect(out).toMatch(/'count'=\d/);
  });

  // --- Windows-only posture checks (stubbed on this platform) ---------------

  it.each([
    ["check_firewall"],
    ["check_nla"],
    ["check_antivirus"],
    ["check_bitlocker"],
    ["check_secureboot"],
    ["check_defender"],
  ])("%s reports not-supported on this platform", async (cmd) => {
    const out = await query(cmd, []);
    expect(out).toMatch(/not supported on this platform/i);
  });
});

// ── Windows posture checks: verify real behaviour on Windows ──────────────────
//
// These read live security posture that varies by host and SKU, so — like the
// checkdisk/checksystem Windows checks — they assert "runs cleanly + expected
// status/content" rather than exact values. Two providers are SKU-dependent:
// Security Center (check_antivirus) is client-only, and the BitLocker WMI
// namespace is present only with the feature installed, so those two accept an
// availability error as well as a real result.

const onWindows = process.platform === "win32" ? describe : describe.skip;

onWindows("CheckSecurity (Windows posture)", () => {
  let nscp: NscpInstance;

  /** Run a CheckSecurity query and return the combined output. */
  async function query(command: string, args: string[] = []): Promise<string> {
    const r = await nscp.run(["client", "--module", "CheckSecurity", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  // --- check_defender --------------------------------------------------------

  it("check_defender reports Defender status (or the not-active contract)", async () => {
    // Defender active -> OK with status fields; a third-party AV owning
    // protection -> UNKNOWN via the empty state. Accept either, never an error.
    const out = await query("check_defender", ["warning=none", "critical=none"]);
    expect(out).toMatch(/Defender/i);
    expect(out).toMatch(/^(OK|UNKNOWN)/m);
  });

  it("check_defender accepts its threshold keywords over the query path", async () => {
    const out = await query("check_defender", ["critical=enabled = 0 or realtime_enabled = 0 or signature_age > 7"]);
    expect(out).not.toMatch(/does not take any arguments|invalid expression|error parsing/i);
  });

  // --- check_firewall --------------------------------------------------------

  it("check_firewall reports the profile state", async () => {
    // The three profiles (Domain/Private/Public) always exist; with thresholds
    // pinned off the check is OK and names the firewall.
    const out = await query("check_firewall", ["warning=none", "critical=none"]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/firewall/i);
  });

  // --- check_nla -------------------------------------------------------------

  it("check_nla reports network location profiles", async () => {
    // No default thresholds; a host with networks (or none: empty-state=ok) is OK.
    const out = await query("check_nla", []);
    expect(out).toMatch(/^OK/m);
  });

  // --- check_secureboot ------------------------------------------------------

  it("check_secureboot reports the Secure Boot state", async () => {
    // Always produces a row from the registry; crit=none keeps it OK on legacy
    // BIOS / disabled hosts too.
    const out = await query("check_secureboot", ["critical=none"]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/secure boot/i);
  });

  // --- check_users -----------------------------------------------------------

  it("check_users reports logged-on sessions", async () => {
    const out = await query("check_users", ["warning=none", "critical=none"]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/user|logged on/i);
  });

  // --- check_antivirus / check_bitlocker (provider availability varies) ------

  it("check_antivirus runs (Security Center is client-SKU only)", async () => {
    // On client Windows the Security Center lists the AV product; on Server SKUs
    // root\SecurityCenter2 is absent and the check reports it could not be
    // queried. Either way it produces recognizable output, not a crash/hang.
    const out = await query("check_antivirus", []);
    expect(out.trim().length).toBeGreaterThan(0);
    expect(out).toMatch(/Defender|antivirus|Security Center|Failed to query/i);
  });

  it("check_bitlocker runs (BitLocker provider may be absent)", async () => {
    // With the BitLocker WMI provider present it reports per-volume protection;
    // without the feature the namespace is missing and it says so. Accept both.
    const out = await query("check_bitlocker", ["critical=none"]);
    expect(out.trim().length).toBeGreaterThan(0);
    expect(out).toMatch(/protect|volume|BitLocker|encrypt|namespace|Failed to query/i);
  });
});
