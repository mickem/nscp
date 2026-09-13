/**
 * Exercises the `file access` gate on CheckLogFile: which files a caller may
 * name in `file=` once an operator has restricted it.
 *
 * check_logfile reads whatever path it is given with the privileges of the
 * agent, so on a host where callers pass arguments (NRPE with `allow
 * arguments`, or REST) that argument decides how much of the machine a single
 * check can read back. The three modes here are what lets an operator narrow
 * that, and the cases which matter most are the two ways a plain string
 * comparison would be fooled: `..` out of an allowed directory, and a symlink
 * planted inside one.
 *
 * Each case runs a one-shot client query — `nscp client --module CheckLogFile
 * --boot --query check_logfile ...` — which loads the module, re-reads the
 * settings file and runs the check. That still passes `k=v` as single tokens,
 * so it exercises the same argument parsing REST does, with no web server to
 * stand up. Settings are rewritten between cases because `--boot` reads them
 * fresh on every invocation.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

const UNKNOWN = 3;

describe("CheckLogFile file access modes", () => {
  let nscp: NscpInstance;
  let scratch: string;
  let allowedDir: string;
  let secretDir: string;
  let allowedLog: string;
  let allowedNotes: string;
  let secretFile: string;

  /** Run check_logfile and return its output plus the Nagios exit code. */
  async function check(args: string[]): Promise<{ out: string; code: number }> {
    const r = await nscp.run(
      [
        "client",
        "--module",
        "CheckLogFile",
        "--boot",
        "--query",
        "check_logfile",
        ...args,
        "filter=column1 like 'SECRET'",
        "warning=count > 0",
        "empty-state=ok",
      ],
      { allowFailure: true },
    );
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  /**
   * Point [/settings/logfile] at one mode. Both keys are always written so a
   * previous case's allow list can never leak into the next one.
   */
  async function setAccess(mode: string, allowed = "-"): Promise<void> {
    await nscp.configure({
      "/settings/logfile": { "file access": mode, "allowed files": allowed },
    });
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    scratch = fs.mkdtempSync(path.join(os.tmpdir(), "logfile-access-it-"));
    allowedDir = path.join(scratch, "logs");
    secretDir = path.join(scratch, "secret");
    fs.mkdirSync(path.join(allowedDir, "sub"), { recursive: true });
    fs.mkdirSync(secretDir, { recursive: true });

    allowedLog = path.join(allowedDir, "app.log");
    allowedNotes = path.join(allowedDir, "notes.txt");
    secretFile = path.join(secretDir, "credentials");
    fs.writeFileSync(allowedLog, "SECRET in an allowed file\n");
    fs.writeFileSync(allowedNotes, "SECRET in an allowed directory\n");
    fs.writeFileSync(path.join(allowedDir, "sub", "deep.log"), "SECRET deeper down\n");
    fs.writeFileSync(secretFile, "SECRET which must not come back\n");

    await nscp.configure({ "/modules": { CheckLogFile: "enabled" } });
  });

  afterAll(() => {
    fs.rmSync(scratch, { recursive: true, force: true });
  });

  // --- any: the default, and what every earlier release did -----------------

  describe("any (default)", () => {
    beforeAll(() => setAccess("any"));

    it("reads a file the caller names", async () => {
      const { out } = await check([`file=${secretFile}`]);
      expect(out).toMatch(/SECRET which must not come back/);
    });

    it("still reads a file when an allow list is configured but unused", async () => {
      await setAccess("any", allowedDir);
      const { out } = await check([`file=${secretFile}`]);
      expect(out).toMatch(/SECRET which must not come back/);
      await setAccess("any");
    });
  });

  // --- allowed: only what matches the list ----------------------------------

  describe("allowed", () => {
    beforeAll(() => setAccess("allowed", allowedDir));

    it("reads a file inside an allowed directory", async () => {
      const { out } = await check([`file=${allowedLog}`]);
      expect(out).toMatch(/SECRET in an allowed file/);
    });

    it("reads a file nested deeper inside an allowed directory", async () => {
      const { out } = await check([`file=${path.join(allowedDir, "sub", "deep.log")}`]);
      expect(out).toMatch(/SECRET deeper down/);
    });

    it("refuses a file outside the allowed directory", async () => {
      const { out, code } = await check([`file=${secretFile}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing file/);
      expect(out).toMatch(/allowed files/);
      expect(out).not.toMatch(/SECRET which must not come back/);
    });

    // The reason a path needs resolving before it is matched: as plain text
    // this one reads as "under the allowed directory" and is not.
    it("refuses a .. traversal out of the allowed directory", async () => {
      const escape = path.join(allowedDir, "..", "secret", "credentials");
      const { out, code } = await check([`file=${escape}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing file/);
      expect(out).not.toMatch(/SECRET which must not come back/);
    });

    // The same traversal written with the other separator. On Windows `\` is
    // a separator and has to be flattened before the match; on Linux it is an
    // ordinary character and must not turn into one afterwards, which is how
    // this escaped an allow list (folding happened after resolution).
    it("refuses a .. traversal written with a backslash", async () => {
      for (const escape of [
        `${allowedDir}/..\\secret\\credentials`,
        `${allowedDir}/..\\..\\secret/credentials`,
        `${allowedDir}\\..\\secret\\credentials`,
      ]) {
        const { out, code } = await check([`file=${escape}`]);
        expect(code).toBe(UNKNOWN);
        expect(out).not.toMatch(/SECRET which must not come back/);
      }
    });

    // `files=` is the comma-separated form of `file=`, and it goes through the
    // same gate: it used to be parsed before the arguments were read, so it
    // did nothing at all.
    it("gates the files= alias, and it actually reads now", async () => {
      const both = await check([`files=${allowedLog}`]);
      expect(both.out).toMatch(/SECRET in an allowed file/);

      const mixed = await check([`files=${allowedLog},${secretFile}`]);
      expect(mixed.code).toBe(UNKNOWN);
      expect(mixed.out).toMatch(/Refusing file/);
      expect(mixed.out).not.toMatch(/SECRET which must not come back/);
    });

    it("refuses a sibling directory sharing the allowed prefix", async () => {
      const sibling = path.join(scratch, "logs-private");
      fs.mkdirSync(sibling, { recursive: true });
      fs.writeFileSync(path.join(sibling, "x.log"), "SECRET next door\n");
      const { out, code } = await check([`file=${path.join(sibling, "x.log")}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).not.toMatch(/SECRET next door/);
    });

    it("refuses every file when one of several is not allowed", async () => {
      const { out, code } = await check([`file=${allowedLog}`, `file=${secretFile}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).not.toMatch(/SECRET in an allowed file/);
      expect(out).not.toMatch(/SECRET which must not come back/);
    });

    it("honours a wildcard entry", async () => {
      await setAccess("allowed", path.join(allowedDir, "*.log"));
      const allowed = await check([`file=${allowedLog}`]);
      expect(allowed.out).toMatch(/SECRET in an allowed file/);

      const refused = await check([`file=${allowedNotes}`]);
      expect(refused.code).toBe(UNKNOWN);
      expect(refused.out).toMatch(/Refusing file/);

      await setAccess("allowed", allowedDir);
    });

    it("accepts several entries", async () => {
      await setAccess("allowed", `${allowedLog},${secretDir}`);
      expect((await check([`file=${allowedLog}`])).out).toMatch(/SECRET in an allowed file/);
      expect((await check([`file=${secretFile}`])).out).toMatch(/SECRET which must not come back/);
      expect((await check([`file=${allowedNotes}`])).code).toBe(UNKNOWN);
      await setAccess("allowed", allowedDir);
    });

    // A symlink is the other way the string comparison lies; the path is
    // resolved before it is matched, so the link is followed out of the
    // allowed directory and refused there.
    (process.platform === "win32" ? it.skip : it)(
      "refuses a symlink leading out of the allowed directory",
      async () => {
        const link = path.join(allowedDir, "escape.log");
        fs.rmSync(link, { force: true });
        fs.symlinkSync(secretFile, link);
        const { out, code } = await check([`file=${link}`]);
        fs.rmSync(link, { force: true });
        expect(code).toBe(UNKNOWN);
        expect(out).toMatch(/Refusing file/);
        expect(out).not.toMatch(/SECRET which must not come back/);
      },
    );

    it("does not disclose the allow list in the refusal", async () => {
      const { out } = await check([`file=${secretFile}`]);
      expect(out).not.toContain(allowedDir);
    });
  });

  // --- predefined: only names the operator configured ------------------------

  describe("predefined", () => {
    beforeAll(async () => {
      await nscp.configure({ "/settings/logfile/files": { app: allowedLog } });
      await setAccess("predefined");
    });

    it("reads a file by its configured name", async () => {
      const { out } = await check(["file=app"]);
      expect(out).toMatch(/SECRET in an allowed file/);
    });

    it("refuses a raw path", async () => {
      const { out, code } = await check([`file=${secretFile}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/set to predefined/);
      expect(out).not.toMatch(/SECRET which must not come back/);
    });

    it("refuses a name which is not configured", async () => {
      const { out, code } = await check(["file=nosuchname"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/set to predefined/);
    });

    // Operator-authored names resolve in every mode, which is what lets a site
    // name its checks first and tighten the mode afterwards.
    it("resolves a configured name in any mode too", async () => {
      await setAccess("any");
      const { out } = await check(["file=app"]);
      expect(out).toMatch(/SECRET in an allowed file/);
      await setAccess("predefined");
    });
  });

  // --- a typo in the mode must not read as "no restriction" ------------------

  describe("an invalid mode", () => {
    it("fails closed and names the valid values", async () => {
      await setAccess("allwed", allowedDir);
      const { out, code } = await check([`file=${allowedLog}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/expected any, allowed or predefined/);
      expect(out).not.toMatch(/SECRET in an allowed file/);
      await setAccess("any");
    });
  });
});
