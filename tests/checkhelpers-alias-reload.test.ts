/**
 * A CheckHelpers alias removed from `[/settings/check helpers/alias]` stops
 * resolving after a settings reload.
 *
 * CheckHelpers withdraws a dropped alias from the core on reload. The core used
 * to look for the name among commands only, so the alias stayed registered:
 * calls still reached CheckHelpers, which no longer had a definition and
 * answered "No alias found matching", until the agent restarted. Now the core
 * forgets the name and answers as it does for any unknown command.
 *
 * Runs over REST: the reload has to happen inside one long-lived agent, which
 * the one-shot client-query path cannot show.
 */
import request from "supertest";
import {
  NscpInstance,
  OK,
  REST_URL,
  UNKNOWN,
  executeQuery,
  messageOf,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(180_000);

const ALIASES = "/settings/check helpers/alias";

describe("CheckHelpers alias reload", () => {
  const nscp = new NscpInstance();
  let key: string;

  beforeAll(async () => {
    key = await setupQueryNscp(nscp, "CheckHelpers", {
      [ALIASES]: {
        tch_keep: "check_ok message=kept",
        tch_drop: "check_ok message=dropped",
      },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  /**
   * Log in again once a reload has brought the REST listener back: sessions
   * live in the WEBServer module, which the reload recreates.
   */
  async function loginWhenUp(): Promise<string> {
    const deadline = Date.now() + 60_000;
    for (;;) {
      try {
        const res = await request(REST_URL)
          .get("/api/v1/login")
          .auth("admin", "default-password")
          .trustLocalhost(true);
        if (res.status === 200) return res.body.key as string;
      } catch (error) {
        const code = (error as NodeJS.ErrnoException).code || "";
        if (!["ECONNREFUSED", "ECONNRESET", "EPIPE"].includes(code)) throw error;
      }
      if (Date.now() > deadline) throw new Error("REST API did not come back after the reload");
      await new Promise((r) => setTimeout(r, 250));
    }
  }

  it("drops a removed alias and keeps the others", async () => {
    expect((await executeQuery(key, "tch_drop")).result).toBe(OK);
    expect((await executeQuery(key, "tch_keep")).result).toBe(OK);

    await request(REST_URL)
      .delete("/api/v2/settings")
      .set("Authorization", `Bearer ${key}`)
      .send([{ path: ALIASES, key: "tch_drop" }])
      .trustLocalhost(true)
      .expect(200);
    // Saved first: reload re-reads the settings from disk.
    for (const command of ["save", "reload"]) {
      await request(REST_URL)
        .post("/api/v2/settings/command")
        .set("Authorization", `Bearer ${key}`)
        .send({ command })
        .trustLocalhost(true)
        .expect(200);
    }

    // The reload is scheduled rather than immediate, and takes the listener
    // and its sessions down with it: keep asking, logging in again when the
    // old session is gone, until the alias stops answering OK.
    const deadline = Date.now() + 60_000;
    let dropped = { result: OK } as Awaited<ReturnType<typeof executeQuery>>;
    while (dropped.result === OK && Date.now() < deadline) {
      await new Promise((r) => setTimeout(r, 500));
      try {
        dropped = await executeQuery(key, "tch_drop");
      } catch {
        key = await loginWhenUp();
      }
    }
    expect(dropped.result).toBe(UNKNOWN);
    expect(messageOf(dropped)).toMatch(/Unknown command/);
    expect(messageOf(dropped)).not.toMatch(/No alias found/);

    const kept = await executeQuery(key, "tch_keep");
    expect(kept.result).toBe(OK);
    expect(messageOf(kept)).toContain("kept");
  });
});
