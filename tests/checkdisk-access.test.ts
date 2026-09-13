/**
 * Exercises the `file access` gate on the disk checks which take a
 * caller-supplied path: check_files, check_single_file and check_disk_write.
 *
 * These never return file contents, but check_files walks whole directory
 * trees and reports every name, size and timestamp, and the checksum keywords
 * turn any readable file into a hash oracle. Where the caller chooses the
 * argument - NRPE with `allow arguments`, or REST - that reach is wide enough
 * to be worth narrowing, and this is the setting that does it.
 *
 * Each case runs a one-shot client query - `nscp client --module CheckDisk
 * --boot --query <cmd> ...` - which re-reads the settings file every time, so
 * the mode can be rewritten between cases with no server to restart. That path
 * also passes `k=v` as single tokens, the same way REST does.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

const UNKNOWN = 3;

describe("CheckDisk file access modes", () => {
  let nscp: NscpInstance;
  let scratch: string;
  let allowedDir: string;
  let secretDir: string;
  let allowedLog: string;
  let secretFile: string;

  async function query(command: string, args: string[]): Promise<{ out: string; code: number }> {
    const r = await nscp.run(["client", "--module", "CheckDisk", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  /** Point [/settings/disk] at one mode, always writing both keys. */
  async function setAccess(mode: string, allowed = "-"): Promise<void> {
    await nscp.configure({
      "/settings/disk": { "file access": mode, "allowed files": allowed },
    });
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    scratch = fs.mkdtempSync(path.join(os.tmpdir(), "checkdisk-access-it-"));
    allowedDir = path.join(scratch, "logs");
    secretDir = path.join(scratch, "secret");
    fs.mkdirSync(path.join(allowedDir, "sub"), { recursive: true });
    fs.mkdirSync(secretDir, { recursive: true });

    allowedLog = path.join(allowedDir, "app.log");
    secretFile = path.join(secretDir, "credentials");
    fs.writeFileSync(allowedLog, "one\ntwo\n");
    fs.writeFileSync(path.join(allowedDir, "sub", "deep.log"), "deep\n");
    fs.writeFileSync(secretFile, "hunter2\n");

    await nscp.configure({ "/modules": { CheckDisk: "enabled" } });
  });

  afterAll(() => {
    fs.rmSync(scratch, { recursive: true, force: true });
  });

  // --- any: the default, and what every earlier release did ------------------

  describe("any (default)", () => {
    beforeAll(() => setAccess("any"));

    it("check_files scans any root the caller names", async () => {
      const { out, code } = await query("check_files", [`path=${secretDir}`, "pattern=*", "detail-syntax=%(filename)", "top-syntax=${list}"]);
      expect(code).not.toBe(UNKNOWN);
      expect(out).toMatch(/credentials/);
    });

    it("check_single_file stats any file the caller names", async () => {
      const { out, code } = await query("check_single_file", [`file=${secretFile}`]);
      expect(code).not.toBe(UNKNOWN);
      expect(out).toMatch(/credentials/);
    });
  });

  // --- allowed: only what matches the list ----------------------------------

  describe("allowed", () => {
    beforeAll(() => setAccess("allowed", allowedDir));

    it("check_files scans a root inside the allowed directory", async () => {
      const { out, code } = await query("check_files", [`path=${allowedDir}`, "pattern=*", "detail-syntax=%(filename)", "top-syntax=${list}"]);
      expect(code).not.toBe(UNKNOWN);
      expect(out).toMatch(/app\.log/);
    });

    it("check_files refuses a root outside the allowed directory", async () => {
      const { out, code } = await query("check_files", [`path=${secretDir}`, "pattern=*", "detail-syntax=%(filename)", "top-syntax=${list}"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing file/);
      expect(out).not.toMatch(/credentials/);
    });

    it("check_files refuses a .. traversal out of the allowed directory", async () => {
      const escape = path.join(allowedDir, "..", "secret");
      const { out, code } = await query("check_files", [`path=${escape}`, "pattern=*", "detail-syntax=%(filename)", "top-syntax=${list}"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing file/);
      expect(out).not.toMatch(/credentials/);
    });

    it("check_files gates the paths= alias too", async () => {
      const { out, code } = await query("check_files", [`paths=${allowedDir},${secretDir}`, "pattern=*"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing file/);
    });

    // Only the scan root is held against the allow list, so `pattern` has to
    // stay a file mask: the Windows scanner concatenates it onto the directory
    // it is walking, and a pattern carrying separators walked straight out of
    // the root which had just passed the gate.
    it("check_files refuses a pattern which climbs out of the scan root", async () => {
      for (const pattern of ["../secret/*", "..\\secret\\*", "../../*", "sub/../../secret/*"]) {
        const { out, code } = await query("check_files", [
          `path=${allowedDir}`,
          `pattern=${pattern}`,
          "detail-syntax=%(filename)",
          "top-syntax=${list}",
        ]);
        expect(code).toBe(UNKNOWN);
        expect(out).toMatch(/Refusing pattern/);
        expect(out).not.toMatch(/credentials/);
      }
    });

    it("check_files refuses a pattern naming a subdirectory while restricted", async () => {
      const { out, code } = await query("check_files", [`path=${allowedDir}`, "pattern=sub/*.log"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing pattern/);
    });

    it("check_files still accepts an ordinary file mask", async () => {
      const { out, code } = await query("check_files", [
        `path=${allowedDir}`,
        "pattern=*.log",
        "detail-syntax=%(filename)",
        "top-syntax=${list}",
      ]);
      expect(code).not.toBe(UNKNOWN);
      expect(out).toMatch(/app\.log/);
    });

    // A traversal written with the other separator. On Windows it is a
    // separator and must be flattened before the match; on Linux it is an
    // ordinary character and must not become one afterwards. Either way the
    // secret directory must not be enumerated.
    it("check_files does not enumerate through a backslash traversal", async () => {
      const escape = `${allowedDir}/..\\..\\secret`;
      const { out } = await query("check_files", [`path=${escape}`, "pattern=*", "detail-syntax=%(filename)", "top-syntax=${list}"]);
      expect(out).not.toMatch(/credentials/);
    });

    // The checksum keywords are the reason this matters even though no content
    // keyword exists: a hash of an arbitrary file is a content oracle.
    it("check_files cannot hash a file outside the allowed directory", async () => {
      const { out, code } = await query("check_files", [
        `path=${secretDir}`,
        "pattern=*",
        "detail-syntax=%(filename)=%(sha256_checksum)",
        "top-syntax=${list}",
      ]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing file/);
      expect(out).not.toMatch(/[0-9a-f]{64}/);
    });

    it("check_single_file refuses a file outside the allowed directory", async () => {
      const { out, code } = await query("check_single_file", [`file=${secretFile}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing file/);
    });

    it("check_disk_write refuses a path outside the allowed directory", async () => {
      const target = path.join(secretDir, "probe.tmp");
      const { out, code } = await query("check_disk_write", [`file=${target}`, "size=1k"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing file/);
      expect(fs.existsSync(target)).toBe(false);
    });

    it("check_disk_write still works inside the allowed directory", async () => {
      const { code } = await query("check_disk_write", [`file=${path.join(allowedDir, "probe.tmp")}`, "size=1k"]);
      expect(code).not.toBe(UNKNOWN);
    });

    // A symlink is the other way a string comparison would be fooled.
    (process.platform === "win32" ? it.skip : it)("refuses a symlink leading out of the allowed directory", async () => {
      const link = path.join(allowedDir, "escape.log");
      fs.rmSync(link, { force: true });
      fs.symlinkSync(secretFile, link);
      const { out, code } = await query("check_single_file", [`file=${link}`]);
      fs.rmSync(link, { force: true });
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing file/);
    });

    it("does not disclose the allow list in the refusal", async () => {
      const { out } = await query("check_single_file", [`file=${secretFile}`]);
      expect(out).not.toContain(allowedDir);
    });
  });

  // --- predefined: only names the operator configured ------------------------

  describe("predefined", () => {
    beforeAll(async () => {
      await nscp.configure({ "/settings/disk/files": { logs: allowedDir, app: allowedLog } });
      await setAccess("predefined");
    });

    it("check_files scans a configured name", async () => {
      const { out, code } = await query("check_files", ["path=logs", "pattern=*", "detail-syntax=%(filename)", "top-syntax=${list}"]);
      expect(code).not.toBe(UNKNOWN);
      expect(out).toMatch(/app\.log/);
    });

    it("check_single_file stats a configured name", async () => {
      const { code } = await query("check_single_file", ["file=app"]);
      expect(code).not.toBe(UNKNOWN);
    });

    it("refuses a raw path", async () => {
      const { out, code } = await query("check_single_file", [`file=${allowedLog}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/set to predefined/);
    });

    it("resolves a configured name in any mode too", async () => {
      await setAccess("any");
      const { code } = await query("check_single_file", ["file=app"]);
      expect(code).not.toBe(UNKNOWN);
      await setAccess("predefined");
    });
  });

  // --- a typo in the mode must not read as "no restriction" ------------------

  describe("an invalid mode", () => {
    it("fails closed and names the valid values", async () => {
      await setAccess("allwed", allowedDir);
      const { out, code } = await query("check_files", [`path=${allowedDir}`, "pattern=*"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/expected any, allowed or predefined/);
      await setAccess("any");
    });
  });
});
