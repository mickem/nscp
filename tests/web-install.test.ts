/**
 * `nscp web install` — the command that turns the WEB server on.
 *
 * It has to leave the agent with a WEB server that starts: HTTPS with a
 * generated self-signed certificate, unless the operator asks for cleartext
 * with --insecure. It used to hinge on a `--https` switch that was stored as
 * false whenever it was absent, so a plain `nscp web install` blanked the
 * `certificate` setting and generated nothing — and because the server no
 * longer downgrades to cleartext on its own, it then refused to start.
 *
 * Every case runs the one-shot command against a throwaway settings file, with
 * `certificate-path` pinned to a scratch directory by the harness so the
 * generated certificate lands somewhere writable. The HTTPS case also boots
 * the agent and completes a TLS handshake against it.
 */
import execa from "execa";
import * as fs from "fs";
import * as path from "path";
import { curlHead } from "@fixtures/http";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

/** The keys of one `[section]` of an nsclient.ini, values trimmed. */
function iniSection(iniPath: string, section: string): Record<string, string> {
  const values: Record<string, string> = {};
  let inSection = false;
  for (const raw of fs.readFileSync(iniPath, "utf8").split(/\r?\n/)) {
    const line = raw.trim();
    if (line === "" || line.startsWith(";") || line.startsWith("#")) continue;
    if (line.startsWith("[")) {
      inSection = line === `[${section}]`;
      continue;
    }
    if (!inSection) continue;
    const eq = line.indexOf("=");
    if (eq < 0) continue;
    values[line.slice(0, eq).trim()] = line.slice(eq + 1).trim();
  }
  return values;
}

function certificatePath(nscp: NscpInstance): string {
  return path.join(nscp.pathOverrides["certificate-path"], "certificate.pem");
}

describe("nscp web install", () => {
  it("defaults to HTTPS and generates the self-signed certificate", async () => {
    const nscp = new NscpInstance();
    const cert = certificatePath(nscp);
    expect(fs.existsSync(cert)).toBe(false);

    const r = await nscp.run(["web", "install", "--password", "install-password"]);
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;

    expect(out).toContain("generating a default certificate");
    expect(out).toContain("Point your browser to https://localhost:8443");
    expect(out).not.toContain("WARNING: The certificate");

    // The agent's own identity: key and certificate in the one file, which is
    // what the server loads when `certificate key` is empty.
    expect(fs.existsSync(cert)).toBe(true);
    const pem = fs.readFileSync(cert, "utf8");
    expect(pem).toContain("PRIVATE KEY");
    expect(pem).toContain("BEGIN CERTIFICATE");

    const server = iniSection(nscp.settingsFile, "/settings/WEB/server");
    expect(server["certificate"]).toBe("${certificate-path}/certificate.pem");
    expect(server["certificate key"] ?? "").toBe("");
    expect(server["allow insecure"]).toBe("false");
    expect(server["port"]).toBe("8443");
    expect(iniSection(nscp.settingsFile, "/modules")["WEBServer"]).toBe("enabled");
  });

  it("leaves the agent with a WEB server that starts and answers over TLS", async () => {
    const nscp = new NscpInstance();
    await nscp.run(["web", "install", "--password", "install-password", "--port", "18443"]);

    nscp.start();
    try {
      await nscp.waitForPort(18443, { timeoutMs: 30_000 });
      // Any HTTP status proves the TLS handshake completed against the
      // generated certificate (curl -k) and the server is answering; "000"
      // is what a refused connection or a handshake failure yields.
      const code = await curlHead("https://127.0.0.1:18443/api/v2/info");
      expect(code).not.toBe("000");
    } finally {
      await nscp.stop();
    }
  });

  it("repairs an install that was left without a certificate", async () => {
    // The state the previous behaviour of this command left behind: HTTPS
    // intended, `certificate` blank. A re-run has to fall back to the default
    // and generate it rather than persist the blank again.
    const nscp = new NscpInstance();
    fs.writeFileSync(
      nscp.settingsFile,
      [
        "[/modules]",
        "WEBServer = enabled",
        "",
        "[/settings/WEB/server]",
        "certificate = ",
        "port = 8443",
        "",
      ].join("\n"),
    );

    await nscp.run(["web", "install", "--password", "install-password"]);

    expect(fs.existsSync(certificatePath(nscp))).toBe(true);
    const server = iniSection(nscp.settingsFile, "/settings/WEB/server");
    expect(server["certificate"]).toBe("${certificate-path}/certificate.pem");
  });

  it("hands a generated certificate to the service account", async () => {
    // On a packaged Linux host the command runs under sudo while the service
    // runs as `nsclient`, and the generated key is readable by its owner only:
    // written as root it was unreadable to the service, so the install reported
    // success and the WEB server never came up. The owner of ${data-path} -
    // the state directory packaging chowns to the service account - is who the
    // file is handed to. Root is needed to chown, so the branch that changes an
    // owner runs where the test is root (the package CI); elsewhere the no-op
    // contract is what is asserted, not skipped.
    if (process.platform === "win32") return;
    const stateDir = fs.mkdtempSync(path.join(require("os").tmpdir(), "nscp-state-"));
    const nscp = new NscpInstance({ pathOverrides: { "data-path": stateDir } });
    const cert = certificatePath(nscp);

    const root = process.getuid?.() === 0;
    let serviceUid = -1;
    if (root) {
      const uid = await execa("id", ["-u", "nobody"], { reject: false });
      const gid = await execa("id", ["-g", "nobody"], { reject: false });
      if (uid.exitCode === 0 && gid.exitCode === 0) {
        serviceUid = Number(uid.stdout.trim());
        fs.chownSync(stateDir, serviceUid, Number(gid.stdout.trim()));
      }
    }

    const r = await nscp.run(["web", "install", "--password", "install-password"]);
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;

    expect(fs.existsSync(cert)).toBe(true);
    expect(out).not.toContain("WARNING: Failed");
    const st = fs.statSync(cert);
    expect(st.mode & 0o777).toBe(0o600);
    if (serviceUid >= 0) {
      expect(out).toContain("handed to the service account");
      expect(st.uid).toBe(serviceUid);
    } else {
      // Not root, or root with no unprivileged account to hand it to: the
      // file stays with whoever wrote it, and nothing claims otherwise.
      expect(out).not.toContain("handed to the service account");
      expect(st.uid).toBe(process.getuid?.() ?? st.uid);
    }
  });

  it("--https is still accepted and means the default", async () => {
    const nscp = new NscpInstance();

    const r = await nscp.run(["web", "install", "--https", "--password", "install-password"]);

    expect(r.all ?? r.stdout).toContain("Point your browser to https://localhost:8443");
    expect(fs.existsSync(certificatePath(nscp))).toBe(true);
  });

  it("--insecure opts into cleartext HTTP explicitly", async () => {
    const nscp = new NscpInstance();

    const r = await nscp.run(["web", "install", "--insecure", "--password", "install-password"]);
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;

    expect(out).toContain("WARNING: Serving UNENCRYPTED HTTP");
    // The server moves a cleartext listener off the HTTPS default port at
    // start-up; install writes the port it will actually listen on.
    expect(out).toContain("Point your browser to http://localhost:8080");
    expect(fs.existsSync(certificatePath(nscp))).toBe(false);

    const server = iniSection(nscp.settingsFile, "/settings/WEB/server");
    expect(server["allow insecure"]).toBe("true");
    expect(server["certificate"] ?? "").toBe("");
    expect(server["port"]).toBe("8080");
  });

  it("--insecure keeps an explicitly chosen port", async () => {
    const nscp = new NscpInstance();

    await nscp.run([
      "web",
      "install",
      "--insecure",
      "--port",
      "9090",
      "--password",
      "install-password",
    ]);

    expect(iniSection(nscp.settingsFile, "/settings/WEB/server")["port"]).toBe("9090");
  });

  it("refuses --https together with --insecure", async () => {
    const nscp = new NscpInstance();

    const r = await nscp.run(
      ["web", "install", "--https", "--insecure", "--password", "install-password"],
      {
        allowFailure: true,
      },
    );

    expect(r.all ?? `${r.stdout}\n${r.stderr}`).toContain("mutually exclusive");
    expect(fs.existsSync(certificatePath(nscp))).toBe(false);
    expect(iniSection(nscp.settingsFile, "/modules")["WEBServer"]).toBeUndefined();
  });

  it("warns when the operator points at a certificate that does not exist", async () => {
    const nscp = new NscpInstance();
    const missing = path.join(nscp.workDir, "no-such.pem");

    const r = await nscp.run([
      "web",
      "install",
      "--certificate",
      missing,
      "--password",
      "install-password",
    ]);
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;

    // Only the default name is generated: an operator-supplied path is theirs
    // to provide, and the output says the server will not start without it.
    expect(out).toContain("WARNING: The certificate");
    expect(out).toContain("does not exist");
    expect(fs.existsSync(missing)).toBe(false);
    expect(iniSection(nscp.settingsFile, "/settings/WEB/server")["certificate"]).toBe(missing);
  });
});
