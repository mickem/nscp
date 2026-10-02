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
 *
 * One thing the assertions must not assume is that the counter inventory holds
 * still. It does not: observed in CI, the whole `\Processor Performance` object
 * and its three counters disappeared between two consecutive `--list Processor`
 * runs seconds apart, and an exact comparison of the two listings failed on
 * three lines the test had never asked about. Providers come and go - that one
 * is processor power management, which a VM host need not keep exposed.
 *
 * So every comparison between two listings is bracketed: the reference query is
 * run again afterwards, and only the counters present in both of its captures
 * are compared. See `stableOnly`. This costs one extra enumeration per test and
 * gives up nothing that matters - a real matching regression makes `--list`
 * return nothing at all, not three counters fewer.
 */
import { NscpInstance, describeOnWindows } from "@fixtures/index";

jest.setTimeout(180_000);

describeOnWindows("CheckSystem pdh counter listing", () => {
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

  /**
   * Runs a comparison of several captures, taking them all again when it
   * fails. stableOnly() covers a counter set that appears or goes away for
   * good during the test, but Windows also drops a set for a moment and brings
   * it back, and a capture that lands in that moment lacks it while both
   * references have it. That does not survive a few fresh attempts; a real
   * regression fails every one, so the comparison keeps its full strength.
   */
  async function eventually(compare: () => Promise<void>, attempts = 3): Promise<void> {
    for (let attempt = 1; ; ++attempt) {
      try {
        await compare();
        return;
      } catch (e) {
        if (attempt >= attempts) throw e;
      }
    }
  }

  /**
   * Restricts a listing to the counters that were stably present, given two
   * captures of the reference query taken either side of the queries being
   * compared. Sorted, because the comparisons here are about which counters
   * came back and not about enumeration order.
   */
  function stableOnly(first: string[], second: string[]): (lines: string[]) => string[] {
    const present = new Set(second);
    const stable = new Set(first.filter((line) => present.has(line)));
    return (lines) => lines.filter((line) => stable.has(line)).sort();
  }

  // Matching ignores case (pinned below), so the per-line checks do too: a
  // listing may legitimately return "Hanteringsprocessor" for "Processor".
  it("filters on the value given to --list", async () => {
    const lines = await list(["--list", "Processor", "--all", "--no-instances"]);
    expect(lines.length).toBeGreaterThan(0);
    for (const line of lines) expect(line.toLowerCase()).toContain("processor");
  });

  it("narrows rather than replaces when --filter is added", async () => {
    const reference = ["--list", "Processor", "--all", "--no-instances"];
    const wide = await list(reference);
    const narrow = await list([...reference, "--filter", "Frequency"]);
    const wideAgain = await list(reference);

    expect(narrow.length).toBeGreaterThan(0);
    expect(narrow.length).toBeLessThan(wide.length);
    // Both terms, on every line. The regression this pins is a line matching
    // only the --filter term, which is what came back while the two options
    // shared one variable.
    for (const line of narrow) {
      expect(line.toLowerCase()).toContain("processor");
      expect(line.toLowerCase()).toContain("frequency");
    }
    // ... and the result really is a subset of the unfiltered listing. Kept as
    // a genuine subset check rather than filtered through stableOnly, which
    // would make it vacuous: only a line the re-read shows to have appeared
    // after `wide` was captured is excused, since `wide` could not have had it.
    const wideSet = new Set(wide);
    const appearedLater = new Set(wideAgain.filter((line) => !wideSet.has(line)));
    for (const line of narrow) {
      if (appearedLater.has(line)) continue;
      expect(wide).toContain(line);
    }
  });

  it("applies every --filter given", async () => {
    const one = await list(["--list", "Processor", "--all", "--no-instances", "--filter", "Frequency"]);
    const two = await list(["--list", "Processor", "--all", "--no-instances", "--filter", "Frequency", "--filter", "Maximum"]);

    expect(two.length).toBeGreaterThan(0);
    expect(two.length).toBeLessThan(one.length);
    for (const line of two) {
      expect(line.toLowerCase()).toContain("processor");
      expect(line.toLowerCase()).toContain("frequency");
      expect(line.toLowerCase()).toContain("maximum");
    }
  });

  it("matches the instance in the path, not only the counter name", async () => {
    const lines = await list(["--list", "Processor Information(0,0)", "--all"]);
    expect(lines.length).toBeGreaterThan(0);
    for (const line of lines) expect(line.toLowerCase()).toContain("processor information(0,0)");
  });

  it("ignores case, in --list and in --filter alike", async () => {
    // The point of the browser is finding a name you do not know yet, and PDH
    // capitalises its own names inconsistently - so the casing must not be
    // something you have to guess right before you get any output.
    const reference = ["--list", "Processor", "--all", "--no-instances"];
    await eventually(async () => {
      const asWritten = await list(reference);
      expect(asWritten.length).toBeGreaterThan(0);

      const lower = await list(["--list", "processor", "--all", "--no-instances"]);
      const upper = await list(["--list", "PROCESSOR", "--all", "--no-instances"]);
      const mixed = await list(["--list", "PROCESSOR", "--all", "--no-instances", "--filter", "frequency"]);
      const mixedAsWritten = await list([...reference, "--filter", "Frequency"]);

      // Second capture of the reference, after every query above: what survived
      // both is what these listings can fairly be compared over. Without this the
      // case fails whenever a provider unregisters mid-test.
      const keep = stableOnly(asWritten, await list(reference));

      expect(keep(asWritten).length).toBeGreaterThan(0);
      expect(keep(lower)).toEqual(keep(asWritten));
      expect(keep(upper)).toEqual(keep(asWritten));

      expect(keep(mixed).length).toBeGreaterThan(0);
      expect(keep(mixed)).toEqual(keep(mixedAsWritten));
    });
  });

  it("treats a valueless --filter as no filter at all", async () => {
    const reference = ["--list", "Processor", "--all", "--no-instances"];
    await eventually(async () => {
      const without = await list(reference);
      const bare = await list([...reference, "--filter"]);
      const keep = stableOnly(without, await list(reference));

      // Still catches the regression this pins - a bare --filter that filtered
      // everything out leaves nothing to match the reference with.
      expect(keep(without).length).toBeGreaterThan(0);
      expect(keep(bare)).toEqual(keep(without));
    });
  });

  it("narrows the instances of a single --counter too", async () => {
    const wide = await list(["--list", "--counter", "Processor Information", "--no-instances"]);
    const narrow = await list(["--list", "--counter", "Processor Information", "--no-instances", "--filter", "Frequency"]);

    expect(wide.length).toBeGreaterThan(0);
    expect(narrow.length).toBeGreaterThan(0);
    expect(narrow.length).toBeLessThan(wide.length);
    for (const line of narrow) expect(line.toLowerCase()).toContain("frequency");
  });
});

/**
 * The index lookups and the legacy CheckCounter, ported from the legacy
 * scripts/python/test_w32_system.py. PDH names are localised but their
 * indexes are not, so this round trip is how an operator writes a counter
 * that works whatever language the host is installed in.
 *
 * The verbs read the opposite way to their names: --lookup-name takes an
 * index and prints its name, --lookup-index takes a name and prints its
 * index.
 */
describeOnWindows("CheckSystem pdh index lookup", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({ "/modules": { CheckSystem: "enabled" } });
  });

  /** One `nscp sys -- <args> --porcelain` run: its exit code and its one line of output. */
  async function lookup(args: string[]): Promise<{ code: number; value: string }> {
    const r = await nscp.run(["sys", "--", ...args, "--porcelain"], { allowFailure: true });
    const lines = r.stdout
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter((line) => line.length > 0);
    return { code: r.exitCode, value: lines[lines.length - 1] ?? "" };
  }

  /**
   * The legacy CheckCounter through a one-shot client query: the exit code
   * (the Nagios status), the result line, and everything the process printed
   * so a failure says why.
   */
  async function checkCounter(args: string[]): Promise<{ code: number; out: string; all: string }> {
    const r = await nscp.run(
      // --log debug: the shim logs the check_pdh arguments it builds, and
      // check_pdh what it did with each counter.
      ["client", "--log", "debug", "--module", "CheckSystem", "--boot", "--query", "CheckCounter", ...args],
      {
        allowFailure: true,
      },
    );
    return { code: r.exitCode, out: r.stdout, all: r.all ?? `${r.stdout}\n${r.stderr}` };
  }

  const names: Record<number, string> = {};

  // 4 is the Memory object and 26 its Committed Bytes counter.
  it.each([4, 26])(
    "--lookup-name %i and --lookup-index of the result round-trip",
    async (index) => {
      const name = await lookup(["--lookup-name", String(index)]);
      expect(name.code).toBe(0);
      expect(name.value.length).toBeGreaterThan(0);
      names[index] = name.value;

      const back = await lookup(["--lookup-index", name.value]);
      expect(back.code).toBe(0);
      expect(back.value).toBe(String(index));
    },
  );

  // CheckCounter answers UNKNOWN when it cannot read the counter, so anything
  // else - with a message and perfdata - means the path resolved.
  it.each([
    ["by index", ["Counter=\\4\\26", "ShowAll", "MaxWarn=10"]],
    ["by index with the index flag", ["Counter=\\4\\26", "index", "ShowAll", "MaxWarn=10"]],
  ])("CheckCounter reads a counter %s", async (_label, args) => {
    const { code, out, all } = await checkCounter(args);
    expect([code, all]).not.toEqual([3, expect.anything()]);
    const [message, perf] = out.trim().split("|");
    expect(message.trim().length).toBeGreaterThan(0);
    expect((perf ?? "").trim().length).toBeGreaterThan(0);
  });

  it("CheckCounter reads a counter by the names the lookup returned", async () => {
    // Runs after the round trip above, which recorded the names.
    expect(names[4]).toBeDefined();
    expect(names[26]).toBeDefined();
    const { code, out, all } = await checkCounter([
      `Counter=\\${names[4]}\\${names[26]}`,
      "ShowAll",
      "MaxWarn=10",
    ]);
    expect([code, all]).not.toEqual([3, expect.anything()]);
    expect(out).toContain("|");
  });
});
