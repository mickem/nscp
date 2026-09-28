/**
 * Real command dispatch on Windows, including the no-cluster UNKNOWN contract.
 * NSCP_EXPECT_CLUSTER=1 requires live data and rejects unavailable fallbacks.
 * The strict suite is read-only; disruptive lab steps are documented separately.
 */
import { NscpInstance, describeOnWindows } from "@fixtures/index";

jest.setTimeout(120_000);
const expectCluster = process.env.NSCP_EXPECT_CLUSTER === "1";
const commands = ["groups", "resources", "nodes", "networks"];
// Only missing API/local cluster failures are accepted on an unprovisioned host.
// Enumeration/state failures must never count as a successful smoke test.
const unavailable = /Failed to query cluster (groups|resources|nodes|networks): (Failover Clustering API not available|Cluster API not available|OpenClusterEx \(local cluster unavailable or inaccessible\))/;

describeOnWindows("CheckWindowsApps cluster commands", () => {
  let nscp: NscpInstance;
  beforeAll(() => { nscp = new NscpInstance(); });

  async function query(command: string, args: string[] = []) {
    const result = await nscp.run(["client", "--module", "CheckWindowsApps", "--boot", "--query", command, ...args], { allowFailure: true });
    const out = result.all ?? `${result.stdout}\n${result.stderr}`;
    expect(out).not.toMatch(/Command not found|Unknown command|Failed to load|unrecognised option/i);
    if (expectCluster) expect(out).not.toMatch(/Failed to query cluster/);
    if (/Failed to query cluster|Cluster object '.+' not found/.test(out)) expect(result.exitCode).toBe(3);
    return out;
  }

  for (const kind of commands) {
    const command = `check_cluster_${kind}`;
    it(`${command} registers and exposes help without accessing a cluster`, async () => {
      const out = await query(command, ["help"]);
      expect(out).toMatch(/name/);
      expect(out).toMatch(/empty-state/);
      expect(out).not.toMatch(/Failed to query cluster/);
    });

    it(`${command} returns live records or the explicit unavailable contract`, async () => {
      const out = await query(command, ["warning=state_id < -2", "critical=state_id < -2", "detail-syntax=object=${name};state=${state};code=${state_id}"]);
      if (!expectCluster && unavailable.test(out)) {
        expect(out).toMatch(/Windows error \d+/);
        expect(out).not.toMatch(/^OK:/m);
      } else {
        expect(out).toMatch(/^OK:.*object=.+;state=[a-z_]+;code=\d+/m);
      }
    });

    it(`${command} cannot hide a required missing object with empty-state=ok`, async () => {
      const out = await query(command, ["name=NSCP-nonexistent-7cd93612", "empty-state=ok"]);
      if (!expectCluster && unavailable.test(out)) return;
      expect(out).toMatch(/Cluster object 'NSCP-nonexistent-7cd93612' not found/);
      expect(out).not.toMatch(/^OK:/m);
    });

    it(`${command} applies the empty filter contract after successful acquisition`, async () => {
      const out = await query(command, ["filter=name = 'NSCP-nonexistent-7cd93612'", "empty-state=ok"]);
      if (!expectCluster && unavailable.test(out)) return;
      expect(out).toContain(`No cluster ${kind} matched`);
      expect(out).not.toMatch(/Failed to query cluster|CRITICAL|WARNING/);
    });
  }

  it("keeps existing IIS and RDS commands available", async () => {
    expect(await query("check_iis_sites", ["help"])).toMatch(/warning/);
    expect(await query("check_rds_licenses", ["help"])).toMatch(/warning/);
  });
});
