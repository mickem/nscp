/**
 * Exercises the DEB/RPM post-install migration that moves writable state out of
 * the package directory.
 *
 * Until 0.16.x the fleet identity (`agent-state.json`) and the fleet managed
 * directory were written under the package directory, which is root-owned,
 * while the service runs as `nsclient`. The agent enrolled successfully and
 * then never appeared in the fleet, because the sync thread could not read the
 * manifest. Both now live under the state directory, and the maintainer scripts
 * have to move what an older install left behind.
 *
 * That migration only ever runs during a real package upgrade, which is exactly
 * why it is worth testing here: the code path is otherwise exercised for the
 * first time on somebody's production host, and it moves the only copy of the
 * agent's private key.
 *
 * The scripts are `configure_file(@ONLY)` templates, so this renders them the
 * same way CMake does - with the install paths pointed at a scratch tree - and
 * runs them with /bin/sh. No build, no docker, no root required.
 *
 * See docs/design/linux-writable-state.md.
 */
import { execFileSync } from "child_process";
import * as fs from "fs";
import * as os from "os";
import * as path from "path";

jest.setTimeout(60_000);

// POSIX shell only; the templates are the unix maintainer scripts.
const onPosix = process.platform === "win32" ? describe.skip : describe;

const repoRoot = path.resolve(__dirname, "..");

interface Tree {
  root: string;
  script: string;
  pkglib: string;
  state: string;
  etc: string;
}

/**
 * Render a maintainer-script template the way `configure_file(... @ONLY)` does,
 * with every install path redirected into `root`.
 */
function render(templateRelativePath: string, root: string, scriptPath: string): void {
  const substitutions: Record<string, string> = {
    NSCP_PKGLIBDIR: `${root}/usr/lib/nsclient`,
    NSCP_PKGSTATEDIR: `${root}/var/lib/nsclient`,
    NSCP_LOGDIR: `${root}/var/log/nsclient`,
    NSCP_PKGSYSCONFDIR: `${root}/etc/nsclient`,
  };
  let body = fs.readFileSync(path.join(repoRoot, templateRelativePath), "utf8");
  for (const [key, value] of Object.entries(substitutions)) {
    body = body.split(`@${key}@`).join(value);
  }
  // Neutralise what needs a real system (accounts, systemd, SELinux). Every
  // path operation - the part under test - is left to run for real.
  body = body
    .replace(/^\s*(useradd|adduser|addgroup)[^\n]*$/gm, ": account")
    .replace(/^\s*systemctl [^\n]*$/gm, ": systemctl")
    .replace(/chown -R nsclient:nsclient/g, ": chown -R")
    .replace(/chown nsclient:nsclient/g, ": chown");
  fs.writeFileSync(scriptPath, body);
}

/** A tree that looks like an install of the previous version. */
function oldLayout(scratch: string, templateRelativePath: string): Tree {
  const root = fs.mkdtempSync(path.join(scratch, "root-"));
  const tree: Tree = {
    root,
    script: path.join(scratch, `${path.basename(templateRelativePath)}.sh`),
    pkglib: path.join(root, "usr", "lib", "nsclient"),
    state: path.join(root, "var", "lib", "nsclient"),
    etc: path.join(root, "etc", "nsclient"),
  };
  render(templateRelativePath, root, tree.script);
  fs.mkdirSync(path.join(tree.pkglib, "security"), { recursive: true });
  fs.mkdirSync(path.join(tree.pkglib, "fleet", "cache"), { recursive: true });
  fs.mkdirSync(tree.etc, { recursive: true });
  fs.mkdirSync(tree.state, { recursive: true });
  fs.writeFileSync(path.join(tree.pkglib, "security", "agent-state.json"), '{"version":1,"private_key_pem":"SECRET"}');
  fs.writeFileSync(path.join(tree.pkglib, "security", "certificate.pem"), "CERT");
  // Shipped read-only material that shares the directory and must not move.
  fs.writeFileSync(path.join(tree.pkglib, "security", "nrpe_dh_2048.pem"), "DHPARAMS");
  fs.writeFileSync(path.join(tree.pkglib, "fleet", "fleet.ini"), "; managed");
  fs.writeFileSync(path.join(tree.pkglib, "fleet", "applied-state.json"), '{"state_hash":"abc"}');
  fs.writeFileSync(path.join(tree.pkglib, "fleet", "cache", "bundle.zip"), "BUNDLE");
  fs.writeFileSync(path.join(tree.etc, "nsclient.ini"), "[/includes]\nfleet = ${shared-path}/fleet/fleet.ini\n");
  return tree;
}

// `args` is how each packager invokes the script on an upgrade, `freshArgs` on
// a first install: dpkg passes the previous version as $2, rpm passes the
// number of installed instances as $1.
const packages = [
  { name: "deb postinst", template: "files/deb/postinst.in", args: ["configure", "0.16.1"], freshArgs: ["configure"] },
  { name: "rpm postinstall", template: "files/rpm/postinstall.sh.in", args: ["2"], freshArgs: ["1"] },
];

onPosix("package upgrade migrates writable state out of the package directory", () => {
  let scratch: string;

  beforeAll(() => {
    scratch = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-packaging-"));
  });

  afterAll(() => {
    fs.rmSync(scratch, { recursive: true, force: true });
  });

  function upgrade(tree: Tree, args: string[]): void {
    execFileSync("/bin/sh", [tree.script, ...args], { stdio: "pipe" });
  }

  describe.each(packages)("$name", ({ template, args, freshArgs }) => {
    it("moves the identity and the fleet directory to the state directory", () => {
      const tree = oldLayout(scratch, template);
      upgrade(tree, args);

      expect(fs.readFileSync(path.join(tree.state, "security", "agent-state.json"), "utf8")).toBe(
        '{"version":1,"private_key_pem":"SECRET"}',
      );
      expect(fs.existsSync(path.join(tree.pkglib, "security", "agent-state.json"))).toBe(false);
      expect(fs.readFileSync(path.join(tree.state, "fleet", "fleet.ini"), "utf8")).toBe("; managed");
      expect(fs.readFileSync(path.join(tree.state, "fleet", "applied-state.json"), "utf8")).toBe('{"state_hash":"abc"}');
      expect(fs.readFileSync(path.join(tree.state, "fleet", "cache", "bundle.zip"), "utf8")).toBe("BUNDLE");
      expect(fs.existsSync(path.join(tree.pkglib, "fleet"))).toBe(false);
    });

    it("leaves everything else in the package security directory alone", () => {
      const tree = oldLayout(scratch, template);
      upgrade(tree, args);
      // Only the manifest moves. The rest of that directory is still resolved
      // through ${certificate-path}, which keeps pointing into the package
      // directory:
      //   - the DH parameters are version-tied package content;
      //   - certificate.pem is the server TLS certificate the WEB, NRPE, NSCA
      //     and NSClient listeners load. Moving it would take HTTPS down on
      //     upgrade. The fleet's own certificate lives inside the manifest.
      expect(fs.readFileSync(path.join(tree.pkglib, "security", "nrpe_dh_2048.pem"), "utf8")).toBe("DHPARAMS");
      expect(fs.readFileSync(path.join(tree.pkglib, "security", "certificate.pem"), "utf8")).toBe("CERT");
      expect(fs.existsSync(path.join(tree.state, "security", "certificate.pem"))).toBe(false);
    });

    it("rewrites the fleet.ini include that enrollment wrote", () => {
      const tree = oldLayout(scratch, template);
      upgrade(tree, args);
      const ini = fs.readFileSync(path.join(tree.etc, "nsclient.ini"), "utf8");
      expect(ini).toContain("${data-path}/fleet/fleet.ini");
      expect(ini).not.toContain("${shared-path}/fleet/fleet.ini");
    });

    it("is idempotent", () => {
      const tree = oldLayout(scratch, template);
      upgrade(tree, args);
      upgrade(tree, args);
      expect(fs.readFileSync(path.join(tree.state, "security", "agent-state.json"), "utf8")).toBe(
        '{"version":1,"private_key_pem":"SECRET"}',
      );
      const ini = fs.readFileSync(path.join(tree.etc, "nsclient.ini"), "utf8");
      expect(ini.match(/\$\{data-path\}\/fleet\/fleet\.ini/g)).toHaveLength(1);
    });

    it("never overwrites an identity already in the state directory", () => {
      // A half-finished previous migration, or a re-enrolled host. The identity
      // in place is the live one; the leftover must not replace it.
      const tree = oldLayout(scratch, template);
      fs.mkdirSync(path.join(tree.state, "security"), { recursive: true });
      fs.writeFileSync(path.join(tree.state, "security", "agent-state.json"), "CURRENT");
      upgrade(tree, args);
      expect(fs.readFileSync(path.join(tree.state, "security", "agent-state.json"), "utf8")).toBe("CURRENT");
    });

    it("creates the state directories on a fresh install with nothing to move", () => {
      const root = fs.mkdtempSync(path.join(scratch, "fresh-"));
      const script = path.join(scratch, `fresh-${path.basename(template)}.sh`);
      render(template, root, script);
      fs.mkdirSync(path.join(root, "etc", "nsclient"), { recursive: true });
      execFileSync("/bin/sh", [script, ...freshArgs], { stdio: "pipe" });
      expect(fs.existsSync(path.join(root, "var", "lib", "nsclient", "security"))).toBe(true);
      expect(fs.existsSync(path.join(root, "var", "lib", "nsclient", "fleet"))).toBe(true);
    });
  });
});
