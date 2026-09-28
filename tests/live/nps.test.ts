/** Opt-in acceptance tests. Requires an explicitly configured NPS lab. */
import { liveLogin, liveExecute, livePoll, type LiveTarget } from "@fixtures/live-target";
import { OK, messageOf, perfOf } from "@fixtures/queries";

const describeNps = process.env.NSCP_NPS_LIVE === "1" ? describe : describe.skip;
describeNps("live NPS and RADIUS", () => {
  let target: LiveTarget;
  let radius: Record<string, string>;
  beforeAll(async () => {
    for (const name of [
      "NSCP_NPS_HOST",
      "NSCP_NPS_USERNAME",
      "NSCP_NPS_SECRET_FILE",
      "NSCP_NPS_PASSWORD_FILE",
    ]) {
      if (!process.env[name]) throw new Error(`${name} is required for the NPS lab suite`);
    }
    target = await liveLogin();
    radius = {
      host: process.env.NSCP_NPS_HOST!,
      "secret-file": process.env.NSCP_NPS_SECRET_FILE!,
      timeout: "3000",
    };
  });
  it("authenticates a test identity through the real NPS", async () => {
    const result = await liveExecute(target, "check_radius", {
      ...radius,
      mode: "auth",
      username: process.env.NSCP_NPS_USERNAME!,
      "password-file": process.env.NSCP_NPS_PASSWORD_FILE!,
    });
    expect(result.result).toBe(OK);
    expect(messageOf(result)).toContain("reply=access_accept");
  });
  it("receives an authenticated rejection for a fictional identity", async () => {
    const result = await liveExecute(target, "check_radius", {
      ...radius,
      mode: "reject",
      username: "nscp-lab-nonexistent",
    });
    expect(result.result).toBe(OK);
    expect(messageOf(result)).toContain("reply=access_reject");
  });
  it("reads real authentication events with both successes and failures", async () => {
    const result = await livePoll(
      target,
      "check_nps_auth",
      {
        window: "300",
        warning: "none",
        critical: "accepted = 0 or rejected = 0",
        "require-traffic": "true",
      },
      (q) => q.result === OK,
    );
    expect(result.result).toBe(OK);
    expect(Object.keys(perfOf(result)).length).toBeGreaterThanOrEqual(4);
  });
  it("reads the installed counter provider", async () => {
    const result = await liveExecute(target, "check_nps_counters", {
      object: process.env.NSCP_NPS_COUNTER_OBJECT ?? "NPS Authentication Server",
    });
    expect(result.result).toBe(OK);
    expect(Object.keys(perfOf(result)).length).toBeGreaterThan(0);
  });
  it("reads accounting discard events and optionally inspects the configured log", async () => {
    const result = await liveExecute(target, "check_nps_accounting", {
      window: "300",
      ...(process.env.NSCP_NPS_LOG_FILE ? { "log-file": process.env.NSCP_NPS_LOG_FILE } : {}),
    });
    expect(result.result).toBe(OK);
    expect(messageOf(result)).toContain("accounting discards");
  });
});
