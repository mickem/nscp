/**
 * Thin launcher that replaces tests/rest/run-test.bat:
 *
 *  1. Spawn `nscp test --settings tests/rest/nsclient.ini` via the shared
 *     NscpInstance fixture so teardown works the same on Linux and
 *     Windows (kills nscp instead of `taskkill /F /im nscp.exe`).
 *  2. Wait for the WEBServer to bind 8443.
 *  3. Shell out to `npm run test` inside tests/rest/ so the Jest +
 *     supertest suite runs unchanged.
 *
 * This file lives in tests/integration/ rather than tests/rest/ because
 * the integration harness's jest.config.js ignores everything under
 * tests/rest/ — the rest suite has its own Jest project and we don't
 * want it picked up as part of the integration matrix.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import execa from "execa";
import { NscpInstance, bundledLuaScript } from "@fixtures/index";

jest.setTimeout(900_000);

describe("REST launcher", () => {
  let nscp: NscpInstance;
  const restDir = path.resolve(__dirname, "rest");
  const restIni = path.join(restDir, "nsclient.ini");

  beforeAll(async () => {
    if (!fs.existsSync(path.join(restDir, "node_modules"))) {
      await execa("npm", ["install"], { cwd: restDir, stdio: "inherit" });
    }

    // Rewrite the checked-in `mock = mock.lua` to an absolute path so
    // nscp finds the script regardless of where ${scripts} resolves.
    const ini = fs.readFileSync(restIni, "utf8")
      .replace(/^mock\s*=\s*mock\.lua\s*$/m, `mock = ${bundledLuaScript("mock")}`);
    const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-rest-"));
    const settingsFile = path.join(tmpDir, "nsclient.ini");
    fs.writeFileSync(settingsFile, ini);

    nscp = new NscpInstance({ settingsFile });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("runs the existing Jest + supertest suite", async () => {
    const r = await execa("npm", ["run", "test"], {
      cwd: restDir,
      reject: false,
      all: true,
      timeout: 600_000,
    });
    if (r.exitCode !== 0) {
      // eslint-disable-next-line no-console
      console.error(r.all);
    }
    expect(r.exitCode).toBe(0);
  });
});
