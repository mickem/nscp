/**
 * Where `[/attachments]` put their files, and what `[/includes]` can name.
 *
 * This is the bug the path series started from (#1557). An attachment target
 * with neither a token nor a root was resolved against the process working
 * directory - `C:\Windows\System32` for a Windows service - so the file
 * downloaded successfully and landed somewhere nobody looks. On Linux the
 * shipped systemd unit happens to set WorkingDirectory to the package
 * directory, which is what ${shared-path} resolves to, so it worked there by
 * accident and the bug stayed hidden.
 *
 * settings-host-placeholders.test.ts already covers the ${host} chain, but it
 * writes absolute paths throughout, so it never exercises the relative case
 * that was actually broken. This does, along with the rest of the resolution
 * rules an operator can hit.
 *
 * Everything asserted here was captured from the built agent first.
 */
import http from "http";
import { AddressInfo } from "net";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

const PAYLOAD = "[/settings/default]\nallowed hosts = 1.2.3.4\n";

describe("settings attachments and includes", () => {
  let server: http.Server;
  let baseUrl: string;
  let served: Record<string, string> = {};

  beforeAll(async () => {
    server = http.createServer((req, res) => {
      const url = decodeURIComponent(req.url ?? "/");
      const body = served[url];
      if (body === undefined) {
        res.writeHead(404);
        res.end("not found");
        return;
      }
      res.writeHead(200, { "Content-Type": "text/plain" });
      res.end(body);
    });
    await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
    baseUrl = `http://127.0.0.1:${(server.address() as AddressInfo).port}`;
  });

  afterAll(async () => {
    await new Promise<void>((resolve) => server.close(() => resolve()));
  });

  /**
   * A sandbox with its own ${shared-path}, a boot.ini that permits plain http
   * (standing up TLS here would test OpenSSL, not path resolution), and a
   * local bootstrap ini that includes `fleetIni` from the fake config server.
   */
  function sandbox(fleetIni: string) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-attach-"));
    const shared = path.join(root, "shared");
    // The agent runs from here: deliberately NOT the install root, so a target
    // that fell back to the working directory shows up as a file in `cwd`.
    const cwd = path.join(root, "cwd");
    fs.mkdirSync(shared, { recursive: true });
    fs.mkdirSync(cwd, { recursive: true });

    const bootIni = path.join(root, "boot.ini");
    fs.writeFileSync(bootIni, "[tls]\nallow plaintext = true\n");

    served["/fleet.ini"] = fleetIni;
    served["/payload.ini"] = PAYLOAD;

    const nscp = new NscpInstance({
      workDir: cwd,
      settingsFile: path.join(root, "nsclient.ini"),
      pathOverrides: { "shared-path": shared, "boot-conf": bootIni },
    });
    fs.writeFileSync(nscp.settingsFile, `[/includes]\nfleet = ${baseUrl}/fleet.ini\n`);
    return { root, shared, cwd, nscp };
  }

  /** Boot the settings subsystem once and return everything it printed. */
  async function boot(nscp: NscpInstance): Promise<string> {
    const r = await nscp.run(["settings", "--list"], { allowFailure: true });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  describe("attachment targets", () => {
    it("put every shape of target where the setting says, and nothing in the working directory", async () => {
      const outside = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-attach-abs-"));
      const absolute = path.join(outside, "absolute.ini");
      const { shared, cwd, nscp } = sandbox(
        [
          "[/attachments]",
          `bare.ini = ${baseUrl}/payload.ini`,
          `\${shared-path}/token.ini = ${baseUrl}/payload.ini`,
          `${absolute} = ${baseUrl}/payload.ini`,
          `deep/nested/made-up.ini = ${baseUrl}/payload.ini`,
          "",
        ].join("\n"),
      );

      await boot(nscp);

      // The fix: a bare name is rooted at ${shared-path}, not at wherever the
      // agent happened to be started from.
      expect(fs.readFileSync(path.join(shared, "bare.ini"), "utf8")).toBe(PAYLOAD);
      // A token naming the same folder is the same answer, written explicitly.
      expect(fs.readFileSync(path.join(shared, "token.ini"), "utf8")).toBe(PAYLOAD);
      // An absolute target is the operator's call and is used as given.
      expect(fs.readFileSync(absolute, "utf8")).toBe(PAYLOAD);
      // A relative target with directories is rooted the same way, and the
      // directories are created - the download used to fail outright because
      // nothing made the parent, which is the other half of #1557.
      expect(fs.readFileSync(path.join(shared, "deep", "nested", "made-up.ini"), "utf8")).toBe(
        PAYLOAD,
      );

      // Nothing landed in the working directory. This is the assertion that
      // would have failed before the fix, when a bare target resolved against
      // whatever directory the agent was started from. (The fixture puts its
      // own `security` folder here for ${certificate-path}, so look for the
      // attachments specifically rather than for an empty directory.)
      const strays = fs
        .readdirSync(cwd, { withFileTypes: true })
        .filter((e) => e.isFile() || e.name === "deep")
        .map((e) => e.name);
      expect(strays).toEqual([]);
    });

    it("skips one unresolvable target, names it, and keeps fetching the rest", async () => {
      const { shared, nscp } = sandbox(
        [
          "[/attachments]",
          `\${scripst}/typo.ini = ${baseUrl}/payload.ini`,
          `survivor.ini = ${baseUrl}/payload.ini`,
          "",
        ].join("\n"),
      );

      const out = await boot(nscp);

      // Reported with the token that was wrong, so it can be corrected...
      expect(out).toMatch(/Skipping attachment .*\$\{scripst\}\/typo\.ini/);
      expect(out).toMatch(/scripst/);
      // ...and the attachment listed after it still arrives. An unknown token
      // is an error for the setting that carries it, not for the settings
      // load: the configuration already in hand is worth more than the add-on.
      expect(fs.existsSync(path.join(shared, "survivor.ini"))).toBe(true);
    });
  });

  describe("include resolution", () => {
    /** Put `value` in [/includes] of a local ini and report what happened. */
    async function includeResolves(value: string): Promise<{ loaded: boolean; output: string }> {
      const root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-include-"));
      const shared = path.join(root, "shared");
      const cwd = path.join(root, "cwd");
      fs.mkdirSync(shared, { recursive: true });
      fs.mkdirSync(cwd, { recursive: true });
      fs.writeFileSync(
        path.join(shared, "included.ini"),
        "[/settings/default]\nallowed hosts = from-the-include\n",
      );

      const nscp = new NscpInstance({
        workDir: cwd,
        settingsFile: path.join(root, "nsclient.ini"),
        pathOverrides: { "shared-path": shared },
      });
      fs.writeFileSync(
        nscp.settingsFile,
        `[/includes]\nx = ${value.split("<shared>").join(shared)}\n`,
      );
      const output = await boot(nscp);
      return { loaded: /allowed hosts=from-the-include/.test(output), output };
    }

    it("loads an absolute include", async () => {
      expect((await includeResolves("<shared>/included.ini")).loaded).toBe(true);
    });

    it("loads an include written with a path token", async () => {
      expect((await includeResolves("${shared-path}/included.ini")).loaded).toBe(true);
    });

    it("refuses a bare relative include rather than guessing a folder for it", async () => {
      // Unlike an attachment target, an include is not rooted at anything: it
      // goes to create_instance, which needs a protocol it recognises or a
      // file it can find, and a bare name is neither. Worth pinning because
      // the two sections sit next to each other and look symmetrical.
      const r = await includeResolves("included.ini");
      expect(r.loaded).toBe(false);
      expect(r.output).toMatch(/Failed to load child included\.ini/);
    });

    it("loads a remote url named directly", async () => {
      // The usual way to pull in fleet configuration: no local file, no path
      // resolution, just the url. The bootstrap ini every case here uses does
      // exactly this, so it is worth asserting rather than leaving implied.
      served["/remote-include.ini"] =
        "[/settings/default]\nallowed hosts = from-the-remote-include\n";
      const root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-remoteinc-"));
      const bootIni = path.join(root, "boot.ini");
      fs.writeFileSync(bootIni, "[tls]\nallow plaintext = true\n");
      const nscp = new NscpInstance({
        workDir: root,
        settingsFile: path.join(root, "nsclient.ini"),
        pathOverrides: { "shared-path": path.join(root, "shared"), "boot-conf": bootIni },
      });
      fs.writeFileSync(nscp.settingsFile, `[/includes]\nextra = ${baseUrl}/remote-include.ini\n`);
      expect(await boot(nscp)).toMatch(/allowed hosts=from-the-remote-include/);
    });
  });

  // What attachments are actually for: shipping a check script to the fleet.
  describe("attaching a script and running it", () => {
    const onWindows = process.platform === "win32";
    const ext = onWindows ? "bat" : "sh";
    const body = onWindows
      ? "@echo off\r\necho OK: attached script ran\r\n"
      : "#!/bin/sh\necho 'OK: attached script ran'\n";

    it("lands in ${scripts} and runs, with the command naming it absolutely", async () => {
      const root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-attachscript-"));
      const shared = path.join(root, "shared");
      const scripts = path.join(shared, "scripts");
      const cwd = path.join(root, "cwd");
      fs.mkdirSync(scripts, { recursive: true });
      fs.mkdirSync(cwd, { recursive: true });
      const bootIni = path.join(root, "boot.ini");
      fs.writeFileSync(bootIni, "[tls]\nallow plaintext = true\n");

      const landed = path.join(scripts, `hello.${ext}`);
      const runner = onWindows ? `cmd /c ${landed}` : `/bin/sh ${landed}`;

      served[`/hello.${ext}`] = body;
      served["/fleet-script.ini"] = [
        "[/modules]",
        "CheckExternalScripts = enabled",
        "",
        "[/attachments]",
        // ${scripts} is where a script belongs, and the target expands it.
        `\${scripts}/hello.${ext} = ${baseUrl}/hello.${ext}`,
        "",
        "[/settings/external scripts/scripts]",
        // ...but the command does NOT expand it - this section is a command
        // line handed to the OS, so it has to name the file absolutely. The
        // two sections sit three lines apart and disagree on purpose.
        `check_attached = ${runner}`,
        "",
      ].join("\n");

      const nscp = new NscpInstance({
        workDir: cwd,
        settingsFile: path.join(root, "nsclient.ini"),
        pathOverrides: { "shared-path": shared, scripts, "boot-conf": bootIni },
      });
      fs.writeFileSync(nscp.settingsFile, `[/includes]\nfleet = ${baseUrl}/fleet-script.ini\n`);

      const r = await nscp.run(
        ["client", "--module", "CheckExternalScripts", "--boot", "--query", "check_attached"],
        { allowFailure: true },
      );

      // One boot is enough here: the command comes from the fleet file itself
      // rather than from a file the fleet file includes, so nothing has to
      // land on disk before it can be read.
      expect(fs.readFileSync(landed, "utf8")).toBe(body);
      expect(r.all ?? "").toMatch(/OK: attached script ran/);
    });

    it("shows what happens when the command uses the token instead", async () => {
      // The mistake the asymmetry invites, pinned so the error is a known
      // one: ${scripts} reaches the shell literally and the script is not
      // found, even though the attachment put it exactly where the token says.
      const root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-attachscript-tok-"));
      const shared = path.join(root, "shared");
      const scripts = path.join(shared, "scripts");
      fs.mkdirSync(scripts, { recursive: true });
      const bootIni = path.join(root, "boot.ini");
      fs.writeFileSync(bootIni, "[tls]\nallow plaintext = true\n");

      served[`/hello.${ext}`] = body;
      served["/fleet-token.ini"] = [
        "[/modules]",
        "CheckExternalScripts = enabled",
        "",
        "[/attachments]",
        `\${scripts}/hello.${ext} = ${baseUrl}/hello.${ext}`,
        "",
        "[/settings/external scripts/scripts]",
        `check_tok = ${onWindows ? "cmd /c" : "/bin/sh"} \${scripts}/hello.${ext}`,
        "",
      ].join("\n");

      const nscp = new NscpInstance({
        workDir: root,
        settingsFile: path.join(root, "nsclient.ini"),
        pathOverrides: { "shared-path": shared, scripts, "boot-conf": bootIni },
      });
      fs.writeFileSync(nscp.settingsFile, `[/includes]\nfleet = ${baseUrl}/fleet-token.ini\n`);

      const r = await nscp.run(
        ["client", "--module", "CheckExternalScripts", "--boot", "--query", "check_tok"],
        { allowFailure: true },
      );

      // The file is there...
      expect(fs.existsSync(path.join(scripts, `hello.${ext}`))).toBe(true);
      // ...and the command still does not find it.
      expect(r.all ?? "").not.toMatch(/OK: attached script ran/);
    });
  });

  /**
   * The pairing an operator actually writes, across the layouts.
   *
   * The two lines resolve through completely different machinery and
   * ${scripts} is not involved in either: the attachment target is rooted at
   * ${shared-path}, while the command is a command line whose relative path is
   * measured against the *child process's* working directory. So whether they
   * meet depends on the relationship between those two, which is what these
   * cases vary.
   *
   * The shapes are modelled with path overrides rather than by running on each
   * OS, because the deciding quantity is that relationship and not the
   * platform. What differs per platform is only where the child's working
   * directory comes from: on Windows the launcher passes ${base-path} to
   * CreateProcess, while the unix launcher sets nothing and the child inherits
   * the agent's. Linux-only for the /bin/sh runner.
   */
  (process.platform === "linux" ? describe : describe.skip)(
    "a script attachment paired with a relative command",
    () => {
      interface Shape {
        name: string;
        /** The child's working directory. */
        cwd: "shared" | "base" | "elsewhere";
        /** Whether ${shared-path} and ${base-path} are the same folder. */
        sharedIsBase: boolean;
      }

      // ${scripts} is ${exe-path}/scripts on Windows and ${shared-path}/scripts
      // on unix, and on Windows it deliberately does NOT move to %ProgramData%
      // with the writable state - only ${shared-path} does. That is what splits
      // the two Windows rows apart.
      const shapes: Shape[] = [
        { name: "linux, shipped systemd unit", cwd: "shared", sharedIsBase: true },
        { name: "linux, any other launch", cwd: "elsewhere", sharedIsBase: true },
        { name: "windows service, legacy layout", cwd: "base", sharedIsBase: true },
        { name: "windows service, modern layout", cwd: "base", sharedIsBase: false },
      ];

      /** Run one shape against one target spelling; report whether it ran. */
      async function attempt(shape: Shape, target: string): Promise<boolean> {
        const root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-layout-"));
        const base = path.join(root, "base");
        const shared = shape.sharedIsBase ? base : path.join(root, "programdata");
        // ${scripts} follows ${base-path} when the two are split, which is the
        // Windows arrangement; when they are the same folder both readings
        // agree, which is every other row.
        const scripts = path.join(shape.sharedIsBase ? shared : base, "scripts");
        const elsewhere = path.join(root, "elsewhere");
        for (const d of [base, shared, scripts, elsewhere]) fs.mkdirSync(d, { recursive: true });

        const cwd = { shared, base, elsewhere }[shape.cwd];
        const bootIni = path.join(root, "boot.ini");
        fs.writeFileSync(bootIni, "[tls]\nallow plaintext = true\n");

        const key = `/layout-${Math.random().toString(36).slice(2)}.ini`;
        served["/hello.sh"] = "#!/bin/sh\necho 'OK: attached script ran'\n";
        served[key] = [
          "[/modules]",
          "CheckExternalScripts = enabled",
          "",
          "[/attachments]",
          `${target} = ${baseUrl}/hello.sh`,
          "",
          "[/settings/external scripts/scripts]",
          "check = /bin/sh scripts/hello.sh",
          "",
        ].join("\n");

        const nscp = new NscpInstance({
          workDir: cwd,
          settingsFile: path.join(root, "nsclient.ini"),
          pathOverrides: { "shared-path": shared, scripts, "boot-conf": bootIni },
        });
        fs.writeFileSync(nscp.settingsFile, `[/includes]\nfleet = ${baseUrl}${key}\n`);

        const r = await nscp.run(
          ["client", "--module", "CheckExternalScripts", "--boot", "--query", "check"],
          { allowFailure: true },
        );
        return /OK: attached script ran/.test(r.all ?? "");
      }

      // `scripts/hello.sh` as the target is rooted at ${shared-path}, so it
      // meets a cwd-relative command only where the cwd IS ${shared-path}.
      it("with a relative target, works only where the working directory is ${shared-path}", async () => {
        const got: Record<string, boolean> = {};
        for (const shape of shapes) got[shape.name] = await attempt(shape, "scripts/hello.sh");
        console.log(
          "\n  target scripts/hello.sh + command scripts/hello.sh\n" +
            shapes
              .map((s) => `    ${s.name.padEnd(32)} ${got[s.name] ? "runs" : "DOES NOT RUN"}`)
              .join("\n"),
        );

        expect(got["linux, shipped systemd unit"]).toBe(true);
        expect(got["windows service, legacy layout"]).toBe(true);
        // The two that break, for different reasons: nothing sets the child's
        // cwd on a non-systemd unix launch, and under the modern layout the
        // attachment goes to %ProgramData%\NSClient++\scripts while the
        // command looks in <install>\scripts.
        expect(got["linux, any other launch"]).toBe(false);
        expect(got["windows service, modern layout"]).toBe(false);
      });

      // ${scripts}/hello.sh instead puts the file where ${scripts} says, and
      // ${scripts} sits under the folder the child starts in on every standard
      // launch - including the modern layout, because ${scripts} does not move.
      it("with a ${scripts} target, works on every standard launch", async () => {
        const got: Record<string, boolean> = {};
        for (const shape of shapes) got[shape.name] = await attempt(shape, "${scripts}/hello.sh");
        console.log(
          "\n  target ${scripts}/hello.sh + command scripts/hello.sh\n" +
            shapes
              .map((s) => `    ${s.name.padEnd(32)} ${got[s.name] ? "runs" : "DOES NOT RUN"}`)
              .join("\n"),
        );

        expect(got["linux, shipped systemd unit"]).toBe(true);
        expect(got["windows service, legacy layout"]).toBe(true);
        // The one this spelling fixes: the file now lands beside the executable
        // rather than in the writable state, which is where the command looks.
        expect(got["windows service, modern layout"]).toBe(true);
        // The one it cannot fix. Nothing sets a working directory for the child
        // on unix, so a relative command still has no fixed meaning; only an
        // absolute path in the command covers this.
        expect(got["linux, any other launch"]).toBe(false);
      });
    },
  );

  describe("an attachment that is then included", () => {
    it("converges on the second boot with a token, and never with a bare name", async () => {
      const { shared, nscp } = sandbox(
        [
          "[/attachments]",
          `extra.ini = ${baseUrl}/extra.ini`,
          "",
          "[/includes]",
          "bare = extra.ini",
          "tok = ${shared-path}/extra.ini",
          "",
        ].join("\n"),
      );
      served["/extra.ini"] = "[/settings/default]\nallowed hosts = from-the-attachment\n";

      // Boot one resolves the include chain from the freshly downloaded config
      // *before* it fetches the attachments, so neither include has a file to
      // open yet. The download still happens.
      const first = await boot(nscp);
      expect(first).not.toMatch(/from-the-attachment/);
      expect(fs.existsSync(path.join(shared, "extra.ini"))).toBe(true);

      // Boot two finds it - but only through the token. This is the trap the
      // attachment fix creates: a bare target now lands in ${shared-path},
      // while a bare include still resolves nowhere, so the natural pairing of
      // `extra.ini = <url>` with `extra = extra.ini` never converges.
      const second = await boot(nscp);
      expect(second).toMatch(/from-the-attachment/);
      expect(second).toMatch(/Failed to load child extra\.ini/);
    });
  });
});
