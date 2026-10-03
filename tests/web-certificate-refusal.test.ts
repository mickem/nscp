/**
 * The WEB server never falls back to cleartext because its certificate did
 * not load.
 *
 * `allow insecure = false` used to be enforced only for a *missing*
 * certificate file. A file that exists but does not load - no private key, a
 * key for another certificate, garbage - left the REST API, login included, on
 * plain HTTP on 8443 with nothing but a log line to show for it. The HTTP
 * layer now refuses to start a server whose certificate failed to load.
 */
import * as fs from "fs";
import * as path from "path";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(900_000);

const WEB_PORT = 8443;

describe("WEB server with a certificate that does not load", () => {
  let nscp: NscpInstance | undefined;

  afterAll(async () => {
    await nscp?.stop();
  });

  it("does not start rather than serve plain HTTP", async () => {
    nscp = new NscpInstance();
    const junk = path.join(nscp.workDir, "junk.pem");
    fs.writeFileSync(junk, "this is not a certificate\n");
    await nscp.configure({
      "/modules": { WEBServer: "enabled" },
      "/settings/default": { "allowed hosts": "127.0.0.1" },
      "/settings/WEB/server": { certificate: junk },
      "/settings/WEB/server/users/admin": { role: "full", password: "web-password" },
    });
    await nscp.waitForPortFree(WEB_PORT, { timeoutMs: 30_000 });
    nscp.start();
    const deadline = Date.now() + 30_000;
    while (
      Date.now() < deadline &&
      !nscp.capturedStdout().includes("The WEB server has NOT been started")
    ) {
      await new Promise((r) => setTimeout(r, 250));
    }
    expect(nscp.capturedStdout()).toContain(
      "could not be loaded: refusing to serve the WEB server in cleartext HTTP",
    );
    expect(nscp.capturedStdout()).not.toContain("Loading webserver on port");
    await expect(nscp.waitForPort(WEB_PORT, { timeoutMs: 2_000 })).rejects.toThrow();
  });
});
