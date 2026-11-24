/**
 * Exercises the CheckSecurity module end-to-end against the real nscp binary.
 *
 * Each case runs a one-shot client query — `nscp client --module CheckSecurity
 * --boot --query <cmd> ...` — which loads the module, runs the check and prints
 * the Nagios-style result line. No server/port/docker needed (and, unlike the
 * REST-based suites, no login), so it runs anywhere the binary builds.
 *
 * check_certificate is cross-platform (OpenSSL file parsing); the certificate
 * fixtures are generated with controlled validity windows. check_secureboot,
 * check_local_accounts and check_group_members read local files on Linux
 * (efivars, /etc/passwd + /etc/shadow, /etc/group) and are asserted against
 * what this host's own files say; macOS stubs them. check_firewall, the Windows
 * certificate store and the remaining posture checks are Windows-only, so here
 * we assert the "not supported on this platform" behaviour.
 */
import { execFileSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";

import {
  NscpInstance,
  generateCertChain,
  describeOnWindows,
  describeOnUnix,
  describeOnLinux,
  itOnDarwin,
} from "@fixtures/index";

jest.setTimeout(120_000);

describeOnUnix("CheckSecurity", () => {
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
    ["check_defender"],
    ["check_activation"],
    ["check_file_security"],
    ["check_firewall_rules"],
  ])("%s reports not-supported on this platform", async (cmd) => {
    const out = await query(cmd, []);
    expect(out).toMatch(/not supported on this platform/i);
  });

  // check_secureboot, check_local_accounts and check_group_members have Linux
  // sources (see the "CheckSecurity (Linux posture)" suite) but no Darwin reader.
  itOnDarwin.each([["check_secureboot"], ["check_local_accounts"], ["check_group_members"]])(
    "%s reports not-supported on macOS",
    async (cmd) => {
      const out = await query(cmd, []);
      expect(out).toMatch(/not supported on this platform/i);
    },
  );
});

// ── Linux posture checks: efivars, /etc/passwd + /etc/shadow, /etc/group ──────
//
// The answers depend on the host (UEFI or BIOS, root or not, which admin group
// exists), so each case reads the same files the check does and asserts the
// documented contract for what it finds, with thresholds pinned so the status
// does not depend on the host's posture.

describeOnLinux("CheckSecurity (Linux posture)", () => {
  let nscp: NscpInstance;

  async function query(command: string, args: string[] = []): Promise<string> {
    const r = await nscp.run(
      ["client", "--module", "CheckSecurity", "--boot", "--query", command, ...args],
      {
        allowFailure: true,
      },
    );
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  function readable(file: string): boolean {
    try {
      fs.accessSync(file, fs.constants.R_OK);
      return true;
    } catch {
      return false;
    }
  }

  /** Group names in /etc/group, in file order. */
  function localGroups(): string[] {
    return fs
      .readFileSync("/etc/group", "utf8")
      .split("\n")
      .filter((l) => l && !l.startsWith("#") && !l.startsWith("+") && !l.startsWith("-"))
      .map((l) => l.split(":")[0]);
  }

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  // --- check_secureboot ------------------------------------------------------

  it("check_secureboot reports the UEFI Secure Boot state", async () => {
    const out = await query("check_secureboot", [
      "warning=none",
      "critical=none",
      "top-syntax=${status}: ${list}",
      "detail-syntax=enabled=${enabled} supported=${supported} setup_mode=${setup_mode}",
    ]);
    if (!fs.existsSync("/sys/firmware/efi")) {
      // Documented: no EFI directory is a legacy BIOS boot (or a container that
      // masks /sys/firmware), not an error.
      expect(out).toMatch(/^OK: enabled=0 supported=0 setup_mode=unknown/m);
    } else {
      // A UEFI host: a state, or an explicit error when the variables cannot be
      // read - never a guessed "disabled".
      expect(out).toMatch(
        /^OK: enabled=[01] supported=[01] setup_mode=(0|1|unknown)|Cannot read UEFI variables|Failed to read|Malformed EFI variable/m,
      );
    }
  });

  it("check_secureboot defaults to critical unless Secure Boot is on", async () => {
    const out = await query("check_secureboot", []);
    if (!fs.existsSync("/sys/firmware/efi")) {
      expect(out).toMatch(/^CRITICAL: secure boot enabled=0 supported=0/m);
    } else {
      expect(out).toMatch(
        /^(OK|CRITICAL): secure boot|Cannot read UEFI variables|Failed to read|Malformed EFI variable/m,
      );
    }
  });

  // --- check_local_accounts --------------------------------------------------

  it("check_local_accounts maps root onto the built-in administrator", async () => {
    // allow-unreadable-shadow=true is the REST-style valued boolean (the
    // bool_switch trap) and keeps this independent of whether we are root.
    const out = await query("check_local_accounts", [
      "allow-unreadable-shadow=true",
      "filter=name = 'root'",
      "warning=none",
      "critical=none",
      "top-syntax=${status}: ${list}",
      "detail-syntax=${name} sid=${sid} uid=${uid} gid=${gid} admin=${is_builtin_admin} extra=${is_extra_uid0} guest=${is_builtin_guest}",
    ]);
    expect(out).toMatch(/^OK: root sid=0 uid=0 gid=0 admin=1 extra=0 guest=0/m);
  });

  it("check_local_accounts flags no system account as passwordless by default", async () => {
    // nologin-style accounts with '*' or '!' hashes require a password (none can
    // match), so they never meet the default critical. Pinned: only accounts
    // that cannot log on interactively, default thresholds.
    const out = await query("check_local_accounts", [
      "allow-unreadable-shadow",
      "filter=interactive = 0 and password_state != 'empty'",
    ]);
    expect(out).not.toMatch(/^CRITICAL/m);
    expect(out).toMatch(/^OK/m);
  });

  it("check_local_accounts honours the /etc/shadow contract", async () => {
    const out = await query("check_local_accounts", [
      "filter=name = 'root'",
      "warning=none",
      "critical=none",
      "top-syntax=${status}: ${list}",
      "detail-syntax=${name} pw=${password_state} req=${password_required} enabled=${enabled}",
    ]);
    if (readable("/etc/shadow")) {
      expect(out).toMatch(/^OK: root pw=(set|empty|locked|none) req=[01] enabled=[01]/m);
    } else {
      // Unprivileged agent: UNKNOWN with the reason, never a made-up OK.
      expect(out).toMatch(/Cannot read \/etc\/shadow: .* - password and expiry state is unknown/);
      expect(out).not.toMatch(/^OK/m);
      const accepted = await query("check_local_accounts", [
        "allow-unreadable-shadow=true",
        "filter=name = 'root'",
        "top-syntax=${status}: ${list}",
        "detail-syntax=${name} pw=${password_state} req=${password_required} enabled=${enabled}",
      ]);
      expect(accepted).toMatch(/^OK: root pw=unknown req=unknown enabled=unknown/m);
    }
  });

  it("check_local_accounts never prints a password hash", async () => {
    const out = await query("check_local_accounts", [
      "allow-unreadable-shadow",
      "warning=none",
      "critical=none",
      "top-syntax=${status}: ${list}",
      "detail-syntax=${name}:${password_state}:${sid}:${shell}",
    ]);
    expect(out).toMatch(/^OK: /m);
    expect(out).not.toMatch(/\$(1|2[aby]?|5|6|y|gy|7)\$/);
  });

  it("check_local_accounts flags extra UID 0 accounts", async () => {
    const extra = fs
      .readFileSync("/etc/passwd", "utf8")
      .split("\n")
      .filter(
        (l) => l.split(":")[2] === "0" && l.split(":")[0] !== "root" && !l.startsWith("#"),
      ).length;
    const out = await query("check_local_accounts", [
      "allow-unreadable-shadow",
      "filter=uid = 0",
      "warning=none",
      "critical=is_extra_uid0 = 1",
      "top-syntax=${status}: N=${count}",
      "ok-syntax=${status}: N=${count}",
    ]);
    expect(out).toMatch(new RegExp(`^${extra > 0 ? "CRITICAL" : "OK"}: N=${extra + 1}\\b`, "m"));
  });

  // --- check_group_members ---------------------------------------------------

  it("check_group_members defaults to the first of sudo and wheel", async () => {
    const groups = localGroups();
    const expected = ["sudo", "wheel"].find((g) => groups.includes(g));
    const out = await query("check_group_members", [
      "top-syntax=${status}: group=${list}",
      "detail-syntax=${group}",
      "empty-syntax=%(status): empty",
    ]);
    if (expected) {
      expect(out).toMatch(new RegExp(`^OK: (group=${expected}\\b|empty)`, "m"));
    } else {
      expect(out).toMatch(/No default administrators group on this host .* pass group=<name>/);
    }
  });

  it("check_group_members includes primary-group members", async () => {
    // root's primary gid is 0, so the root group always lists root even when
    // its member field in /etc/group is empty.
    const out = await query("check_group_members", [
      "group=root",
      "top-syntax=${status}: ${list}",
      "detail-syntax=${group}/${member} [${type}] sid=${sid} domain='${domain}'",
    ]);
    expect(out).toMatch(/^OK: .*root\/root \[user\] sid=0 domain=''/m);
  });

  it("check_group_members flags drift against an exact allow-list", async () => {
    // Linux names are case-sensitive: "ROOT" does not excuse root.
    const drift = await query("check_group_members", ["group=root", "expected=ROOT"]);
    expect(drift).toMatch(/^CRITICAL: .*root \(user\)/m);
    const clean = await query("check_group_members", [
      "group=root",
      "expected=root",
      "filter=member = 'root'",
    ]);
    expect(clean).toMatch(/^OK/m);
  });

  it("check_group_members reports a missing group", async () => {
    const out = await query("check_group_members", ["group=NSCP_NoSuchGroup_zzz"]);
    expect(out).toMatch(/Local group not found: NSCP_NoSuchGroup_zzz/);
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

describeOnWindows("CheckSecurity (Windows posture)", () => {
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

  it("check_firewall exposes the policy source keyword", async () => {
    // `policy` says whether a profile's effective settings come from the local
    // store or are enforced through group policy (#1351). Either value is valid
    // on a runner; assert the keyword renders rather than a specific source.
    const out = await query("check_firewall", [
      "warning=none",
      "critical=none",
      "detail-syntax=${profile}: ${enabled} (${policy})",
      "top-syntax=${status}: ${list}",
    ]);
    expect(out).toMatch(/Domain: [01] \((local|group policy)\)/);
    expect(out).toMatch(/Public: [01] \((local|group policy)\)/);
  });

  // --- check_firewall_rules --------------------------------------------------

  it("check_firewall_rules enumerates the rule set", async () => {
    // Every Windows host ships hundreds of rules. The default output is a
    // summary rather than the list, and `count` carries the rule count.
    const out = await query("check_firewall_rules", []);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/rule\(s\) checked/);
    expect(out).toMatch(/'count'=[1-9]\d*/);
  });

  it("check_firewall_rules exposes the per-rule keywords", async () => {
    // Scope to one enabled inbound rule and render every field the check reads.
    const out = await query("check_firewall_rules", [
      "filter=enabled = 1 and direction = 'in'",
      "top-syntax=${list}",
      "ok-syntax=${list}",
      "detail-syntax=[${direction}/${action}/${protocol}/${profiles}/${local_ports}/${remote_addresses}]",
    ]);
    // Ports and addresses are never empty: an unrestricted field reads "*".
    expect(out).toMatch(/\[in\/(allow|block)\/\S+\/\S+\/[^/\]]+\/[^/\]]+\]/);
  });

  it("check_firewall_rules is CRITICAL when an expected rule is absent", async () => {
    const out = await query("check_firewall_rules", ["expect=NSCP no such rule zzz"]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out).toMatch(/not in effect: no rule with this name/);
  });

  it("check_firewall_rules is satisfied by a rule that really exists", async () => {
    // Rule names are localized, so take one from this machine rather than
    // hard-coding an English name that only exists on an English Windows.
    const listed = await query("check_firewall_rules", [
      "filter=enabled = 1",
      "top-syntax=${list}",
      "ok-syntax=${list}",
      "detail-syntax=${name}",
      "list-separator=\n",
    ]);
    const name = listed.split("\n").map((l) => l.trim()).filter((l) => l.length > 0 && !/^L\s/.test(l))[0];
    expect(name).toBeTruthy();

    const out = await query("check_firewall_rules", [`expect=${name}`]);
    expect(out).toMatch(/^OK/m);
  });

  it("check_firewall_rules keeps an expect= assertion visible through a filter", async () => {
    // A filter must not swallow the hole row that stands in for a missing rule:
    // filter="enabled = 1" would otherwise drop it (holes are enabled=0) and
    // report OK for a rule the check was told to guarantee.
    const out = await query("check_firewall_rules", ["filter=enabled = 1", "expect=NSCP no such rule zzz"]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out).toMatch(/not in effect: no rule with this name/);
  });

  it("check_firewall_rules accepts the scope keywords in filters", async () => {
    // any_any is offered as a keyword rather than a default threshold; it must
    // parse, and on a normal client it does match (app rules are wide open).
    const out = await query("check_firewall_rules", [
      "filter=any_any = 1",
      "warning=none",
      "critical=none",
      "top-syntax=${count} wide open",
      "ok-syntax=${count} wide open",
    ]);
    expect(out).not.toMatch(/does not take any arguments|invalid expression|error parsing/i);
    expect(out).toMatch(/^\d+ wide open/m);
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

  // --- check_local_accounts --------------------------------------------------

  it("check_local_accounts lists local accounts", async () => {
    // Every Windows host has local accounts; with thresholds off it is OK.
    const out = await query("check_local_accounts", ["warning=none", "critical=none"]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/account/i);
  });

  it("check_local_accounts finds the built-in Administrator (RID 500)", async () => {
    // Exactly one local account has RID 500; scope to it and render the count.
    const out = await query("check_local_accounts", [
      "filter=is_builtin_admin = 1",
      "warning=count > 0",
      "critical=none",
      "top-syntax=NBUILTIN=${count}",
    ]);
    expect(out).toMatch(/NBUILTIN=1\b/);
  });

  it("check_local_accounts accepts its hygiene keywords", async () => {
    const out = await query("check_local_accounts", [
      "critical=enabled = 1 and password_required = 0",
      "warning=enabled = 1 and is_builtin_guest = 1",
    ]);
    expect(out).not.toMatch(/does not take any arguments|invalid expression|error parsing/i);
  });

  // --- check_group_members ---------------------------------------------------

  /**
   * The Administrators group under the name an operator sees, which follows the
   * language Windows was installed in. Deliberately the display name and not
   * S-1-5-32-544: looking a group up by its localized name is what this check
   * does in the field. The suite runs on English and Swedish hosts; any other
   * language fails here, so the table gets a row instead of a guess.
   */
  function administratorsGroup(): string {
    const language = execFileSync(
      "powershell",
      ["-NoProfile", "-Command", "[Globalization.CultureInfo]::InstalledUICulture.TwoLetterISOLanguageName"],
      { encoding: "utf8" },
    ).trim();
    const names: Record<string, string> = { en: "Administrators", sv: "Administratörer" };
    const name = names[language];
    if (!name) throw new Error(`No Administrators group name known for Windows install language '${language}'`);
    return name;
  }

  it("check_group_members lists the Administrators group", async () => {
    // No allow-list -> every member is expected -> OK.
    const out = await query("check_group_members", [`group=${administratorsGroup()}`]);
    expect(out).toMatch(/^OK/m);
  });

  it("check_group_members flags membership drift against an expected list", async () => {
    // No real member has this name, so every member is unexpected -> CRITICAL.
    const out = await query("check_group_members", [`group=${administratorsGroup()}`, "expected=NSCP_nobody_zzz"]);
    expect(out).toMatch(/^CRITICAL/m);
  });

  it("check_group_members reports a missing group", async () => {
    const out = await query("check_group_members", ["group=NSCP_NoSuchGroup_zzz"]);
    expect(out).toMatch(/not found/i);
  });

  // --- check_activation ------------------------------------------------------

  it("check_activation reports the Windows licensing state", async () => {
    // Every Windows install has a Windows row in SoftwareLicensingProduct; with
    // the thresholds pinned off the result is OK regardless of activation state.
    const out = await query("check_activation", ["warning=none", "critical=none"]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/Windows/i);
    // The default detail syntax renders "<name>: <status> (<genuine>, grace <n>d)".
    expect(out).toMatch(/(licensed|unlicensed|grace|notification)/);
  });

  it("check_activation exposes the licensing keywords", async () => {
    const out = await query("check_activation", [
      "warning=none",
      "critical=none",
      "detail-syntax=status=${activation_status} licensed=${licensed} windows=${is_windows} key=${key} grace=${grace_days}",
      "top-syntax=${list}",
    ]);
    expect(out).toMatch(/status=\w+ licensed=[01] windows=1 key=\S+ grace=\d+/);
  });

  it("check_activation still accepts the deprecated status alias", async () => {
    // `status` used to be the word-form keyword before it was renamed to
    // activation_status (it clashes with the generic status summary keyword);
    // existing filters and detail-syntax templates must keep working.
    const out = await query("check_activation", [
      "warning=none",
      "critical=none",
      "filter=status != 'no_such_status'",
      "detail-syntax=old=${status} new=${activation_status}",
      "top-syntax=${list}",
    ]);
    expect(out).toMatch(/old=(\w+) new=\1/);
  });

  it("check_activation accepts its boolean options as valued flags", async () => {
    // Over REST a flag arrives as the single token "x=true", which bool_switch
    // would reject; both options must take a value.
    const out = await query("check_activation", ["skip-genuine=true", "all-products=true", "warning=none", "critical=none"]);
    expect(out).not.toMatch(/does not take any arguments|invalid expression|error parsing/i);
    expect(out).toMatch(/^OK/m);
    // Genuine evaluation skipped -> the state is reported as unknown, not faked.
    expect(out).toMatch(/unknown/);
  });

  it("check_activation goes CRITICAL when the product is not licensed", async () => {
    // The default critical is licensed = 0; invert it to prove the threshold is
    // wired to the real value on this (activated or not) host.
    const licensed = await query("check_activation", ["warning=none", "critical=none", "detail-syntax=LICENSED=${licensed}"]);
    const isLicensed = /LICENSED=1/.test(licensed);
    const out = await query("check_activation", [`critical=licensed = ${isLicensed ? 1 : 0}`, "warning=none"]);
    expect(out).toMatch(/^CRITICAL/m);
  });

  // --- check_file_security ---------------------------------------------------

  it("check_file_security reports the owner and DACL of a system folder", async () => {
    // System32 is owned by TrustedInstaller and writable only by SYSTEM and the
    // administrators, so the defaults must find nothing to complain about.
    const out = await query("check_file_security", ["path=C:\\Windows\\System32"]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/no unexpected write access/);
  });

  it("check_file_security requires something to inspect", async () => {
    const out = await query("check_file_security", []);
    expect(out).toMatch(/No path specified/i);
  });

  it("check_file_security goes CRITICAL for a path that is gone", async () => {
    const out = await query("check_file_security", ["path=C:\\NSCP_no_such_dir_zzz"]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out).toMatch(/does not exist/);
  });

  it("check_file_security flags a world-writable directory (inherited included)", async () => {
    // Grant Everyone (by SID, so it also works on a localized Windows) modify
    // rights on a scratch tree; both the folder and the child that inherits the
    // entry must come back CRITICAL.
    const dir = nscp.scratch("world_writable");
    const child = path.join(dir, "child");
    fs.mkdirSync(child, { recursive: true });
    // Inheritance is cut first: the scratch tree sits under %TEMP%, whose ACL
    // can carry write grants of its own (a sandbox group, say) that would turn
    // the allow-listed run below WARNING on one machine and not another.
    execFileSync(
      "icacls",
      [dir, "/inheritance:r", "/grant", `${process.env.USERNAME}:(OI)(CI)F`, "/grant", "*S-1-1-0:(OI)(CI)M"],
      { stdio: "ignore" },
    );

    const out = await query("check_file_security", [`path=${dir}`, `path=${child}`]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out.match(/world writable by/g) ?? []).toHaveLength(2);

    // Allow-listing Everyone by SID silences it again.
    const allowed = await query("check_file_security", [`path=${dir}`, "allow-write=S-1-1-0", `allow-write=${process.env.USERNAME}`]);
    expect(allowed).toMatch(/^OK/m);
  });

  it("check_file_security inspects the binary behind a service", async () => {
    // The Windows Event Log service exists on every SKU; its image path is a
    // system binary, so the defaults pass and the resolved path is shown.
    const out = await query("check_file_security", ["service=EventLog", "detail-syntax=${service} -> ${path}: ${state}", "top-syntax=${status}: ${list}"]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/EventLog -> .*\.exe/i);
  });

  it("check_file_security reports a service that does not exist", async () => {
    const out = await query("check_file_security", ["service=NSCP_NoSuchService_zzz"]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out).toMatch(/Service not found/i);
  });

  it("check_file_security alerts on an unexpected owner", async () => {
    // System32 is owned by TrustedInstaller, so demanding SYSTEM must trip.
    const out = await query("check_file_security", ["path=C:\\Windows\\System32", "expected-owner=NT AUTHORITY\\SYSTEM"]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out).toMatch(/unexpected owner/);
  });

  // --- empty result sets -----------------------------------------------------

  // A filter that matches nothing must land on each check's documented empty
  // contract, never an error (#1499 was check_service dying on this path).
  // Client-query output carries no status word when the empty syntax has
  // none, so the UNKNOWN cases assert the message alone.

  it("check_firewall with a filter that matches nothing reports no profiles", async () => {
    const out = await query("check_firewall", ["filter=profile = 'nosuchprofile-1499'"]);
    expect(out).toMatch(/No firewall profiles found/);
    expect(out).not.toMatch(/^(OK|WARNING|CRITICAL)/m);
  });

  it("check_firewall_rules with a filter that matches nothing is OK", async () => {
    // The default crit is `present = 0`, an object-bound keyword.
    const out = await query("check_firewall_rules", ["filter=name = 'nosuchrule-1499'"]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/No firewall rules matched/);
  });

  it("check_local_accounts with a filter that matches nothing is OK", async () => {
    const out = await query("check_local_accounts", ["filter=name = 'nosuchaccount-1499'"]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/No local accounts found/);
  });

  it("check_nla with a filter that matches nothing reports no networks", async () => {
    const out = await query("check_nla", ["filter=network = 'nosuchnetwork-1499'"]);
    expect(out).toMatch(/No networks found/);
    expect(out).not.toMatch(/^(WARNING|CRITICAL)/m);
  });

  it("check_certificate with a store filter that matches nothing reports no certificates", async () => {
    const out = await query("check_certificate", ["store=MY", "filter=subject = 'nosuchsubject-1499'"]);
    expect(out).toMatch(/No certificates found/);
    expect(out).not.toMatch(/^(OK|WARNING|CRITICAL)/m);
  });

  it("check_file_security with a filter that matches nothing returns UNKNOWN", async () => {
    const out = await query("check_file_security", ["path=C:\\Windows", "filter=path = 'nosuchpath-1499'"]);
    expect(out).toMatch(/^UNKNOWN/m);
    expect(out).toMatch(/No paths checked/);
  });
});
