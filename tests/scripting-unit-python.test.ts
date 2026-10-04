/**
 * The in-process Python test scripts (`scripts/python/test_*.py`), run through
 * `nscp unit` the way the retired acceptance-tests.sh/.bat did - but from
 * jest, so they share the one runner and the one report.
 *
 * `nscp unit` boots PythonScript, runs the script's `__main__` to write the
 * module configuration, reloads, and queries `py_unittest`. What is left here
 * is what only a script loaded into the agent can test: the scripting API
 * calling back into the core. Everything observable from outside the process
 * lives in the module suites.
 *
 * The scripts come from this checkout (see scriptingUnitInstance), not from
 * the install, which does not ship them.
 */
import {
  NscpInstance,
  describeWithModules,
  runScriptingUnit,
  scriptingUnitInstance,
} from "@fixtures/index";

jest.setTimeout(300_000);

describeWithModules("PythonScript")("nscp unit --language python", () => {
  let nscp: NscpInstance;

  beforeAll(() => {
    nscp = scriptingUnitInstance("python");
  });

  // Simple submit, subscription and query round trips through one script.
  it("test_channels", async () => {
    const r = await runScriptingUnit(nscp, "python", "test_channels");
    expect(r.all).toMatch(/OK: \d+ test\(s\) successfull/);
    expect(r.exitCode).toBe(0);
  });

  // Issue #748's perfdata round trip, selected by --case (the case below is
  // what proves a non-matching --case is not a silent pass).
  it("test_python --case perfdata", async () => {
    const r = await runScriptingUnit(nscp, "python", "test_python", {
      cases: ["perfdata"],
      showAll: true,
    });
    expect(r.all).toMatch(/OK: \d+ test\(s\) successfull/);
    expect(r.all).toContain("PythonScript perfdata round-trip");
    expect(r.exitCode).toBe(0);
  });

  // A case naming no suite is a failure, not a silent pass of zero tests.
  it("fails when --case matches no suite", async () => {
    const r = await runScriptingUnit(nscp, "python", "test_python", { cases: ["no-such-suite"] });
    expect(r.all).toMatch(/ERROR: \d+\/\d+ test\(s\) failed/);
    expect(r.exitCode).not.toBe(0);
  });
});
