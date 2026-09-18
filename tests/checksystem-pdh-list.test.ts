/**
 * Exercises the PDH counter browser on CheckSystem's command line — the
 * `nscp sys -- --list ...` path that an operator uses to find a counter name
 * before wiring it into a check.
 *
 * The point here is the filtering. `--list` takes a substring as its value and
 * `--filter` adds more, and every one of them has to match, so a listing can be
 * narrowed down one word at a time. They used to share a single variable, which
 * meant `--filter` silently threw away whatever `--list` had been given and
 * answered a different question than the one asked.
 *
 * Matching is a plain case-sensitive substring test against the whole rendered
 * `\object(instance)\counter` path, which is what lets one filter name the
 * object and the next the counter.
 *
 * PDH is Windows-only, so the suite is skipped elsewhere. The counters used
 * below are on every Windows install; the assertions are about which lines
 * survive the filters rather than about any value.
 */
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

const onWindows = process.platform === "win32" ? describe : describe.skip;

onWindows("CheckSystem pdh counter listing", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({ "/modules": { CheckSystem: "enabled" } });
  });

  /** One `nscp sys -- <args>` run, returning the counter paths it listed. */
  async function list(args: string[]): Promise<string[]> {
    const r = await nscp.run(["sys", "--", ...args], { allowFailure: true });
    return (r.all ?? r.stdout)
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter((line) => line.startsWith("\\"));
  }

  it("filters on the value given to --list", async () => {
    const lines = await list(["--list", "Processor", "--all", "--no-instances"]);
    expect(lines.length).toBeGreaterThan(0);
    for (const line of lines) expect(line).toContain("Processor");
  });

  it("narrows rather than replaces when --filter is added", async () => {
    const wide = await list(["--list", "Processor", "--all", "--no-instances"]);
    const narrow = await list(["--list", "Processor", "--all", "--no-instances", "--filter", "Frequency"]);

    expect(narrow.length).toBeGreaterThan(0);
    expect(narrow.length).toBeLessThan(wide.length);
    // Both terms, on every line. The regression this pins is a line matching
    // only the --filter term, which is what came back while the two options
    // shared one variable.
    for (const line of narrow) {
      expect(line).toContain("Processor");
      expect(line).toContain("Frequency");
    }
    // ... and the result really is a subset of the unfiltered listing.
    for (const line of narrow) expect(wide).toContain(line);
  });

  it("applies every --filter given", async () => {
    const one = await list(["--list", "Processor", "--all", "--no-instances", "--filter", "Frequency"]);
    const two = await list(["--list", "Processor", "--all", "--no-instances", "--filter", "Frequency", "--filter", "Maximum"]);

    expect(two.length).toBeGreaterThan(0);
    expect(two.length).toBeLessThan(one.length);
    for (const line of two) {
      expect(line).toContain("Processor");
      expect(line).toContain("Frequency");
      expect(line).toContain("Maximum");
    }
  });

  it("matches the instance in the path, not only the counter name", async () => {
    const lines = await list(["--list", "Processor Information(0,0)", "--all"]);
    expect(lines.length).toBeGreaterThan(0);
    for (const line of lines) expect(line).toContain("Processor Information(0,0)");
  });

  it("ignores case, in --list and in --filter alike", async () => {
    // The point of the browser is finding a name you do not know yet, and PDH
    // capitalises its own names inconsistently - so the casing must not be
    // something you have to guess right before you get any output.
    const asWritten = await list(["--list", "Processor", "--all", "--no-instances"]);
    expect(asWritten.length).toBeGreaterThan(0);
    expect(await list(["--list", "processor", "--all", "--no-instances"])).toEqual(asWritten);
    expect(await list(["--list", "PROCESSOR", "--all", "--no-instances"])).toEqual(asWritten);

    const mixed = await list(["--list", "PROCESSOR", "--all", "--no-instances", "--filter", "frequency"]);
    expect(mixed.length).toBeGreaterThan(0);
    expect(mixed).toEqual(await list(["--list", "Processor", "--all", "--no-instances", "--filter", "Frequency"]));
  });

  it("treats a valueless --filter as no filter at all", async () => {
    const without = await list(["--list", "Processor", "--all", "--no-instances"]);
    const bare = await list(["--list", "Processor", "--all", "--no-instances", "--filter"]);
    expect(bare).toEqual(without);
  });

  it("narrows the instances of a single --counter too", async () => {
    const wide = await list(["--list", "--counter", "Processor Information", "--no-instances"]);
    const narrow = await list(["--list", "--counter", "Processor Information", "--no-instances", "--filter", "Frequency"]);

    expect(wide.length).toBeGreaterThan(0);
    expect(narrow.length).toBeGreaterThan(0);
    expect(narrow.length).toBeLessThan(wide.length);
    for (const line of narrow) expect(line).toContain("Frequency");
  });
});
