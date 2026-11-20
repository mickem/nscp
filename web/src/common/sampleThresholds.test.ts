import { describe, expect, it } from "vitest";
import { QueryHelp } from "../api/api";
import { makeVocabulary } from "./queryHelp";
import {
  addSamples,
  quoteArgument,
  readSimpleMode,
  removeSamples,
  sampleThresholds,
  splitArguments,
  writeSimpleMode,
} from "./sampleThresholds";

const option = (name: string, default_value = "") => ({
  name,
  default_value,
  required: false,
  repeatable: false,
  content_type: "string",
  short_description: "",
  long_description: "",
});

const field = (name: string) => ({ name, short_description: "", long_description: "" });

const help = (name: string, warning: string | undefined, critical: string | undefined, fields: string[] = []): QueryHelp => ({
  name,
  keyword_source: name,
  parameters: [
    ...(warning === undefined ? [] : [option("warning", warning)]),
    ...(critical === undefined ? [] : [option("critical", critical)]),
    option("filter"),
  ],
  fields: fields.map(field),
});

const vocabularyOf = (h: QueryHelp) => makeVocabulary(h);

describe("sampleThresholds", () => {
  it("offers the check's own defaults", () => {
    const sample = sampleThresholds("check_cpu", vocabularyOf(help("check_cpu", "load > 80", "load > 90", ["load"])));
    expect(sample).toEqual({ warning: ["load > 80"], critical: ["load > 90"], source: "defaults" });
  });

  it("offers the level that has a default when only one does", () => {
    const sample = sampleThresholds("check_tasksched", vocabularyOf(help("check_tasksched", "", "exit_code < 0")));
    expect(sample).toEqual({ warning: [], critical: ["exit_code < 0"], source: "defaults" });
  });

  it("has nothing for a check with no defaults and no curated sample", () => {
    expect(sampleThresholds("check_os_version", vocabularyOf(help("check_os_version", "", "", ["os"])))).toBeUndefined();
  });

  it("has nothing for a command that takes no thresholds at all", () => {
    expect(sampleThresholds("my_script", vocabularyOf(help("my_script", undefined, undefined)))).toBeUndefined();
    expect(sampleThresholds("my_script", makeVocabulary(undefined))).toBeUndefined();
  });

  it("uses a curated sample for a check that declares no default", () => {
    const sample = sampleThresholds("check_load", vocabularyOf(help("check_load", "", "", ["load1", "load5", "load15"])));
    expect(sample).toEqual({ warning: ["load5 > 4"], critical: ["load5 > 8"], source: "sample" });
  });

  it("keeps check_process's two critical defaults apart", () => {
    // The help can only say "state = 'stopped', count = 0", which is not an
    // expression; the curated entry carries the two it was joined from.
    const h = help("check_process", "state not in ('started')", "state = 'stopped', count = 0", ["state", "exe"]);
    expect(sampleThresholds("check_process", vocabularyOf(h))).toEqual({
      warning: ["state not in ('started')"],
      critical: ["state = 'stopped'", "count = 0"],
      source: "sample",
    });
  });

  it("drops a curated sample whose keyword this check does not have", () => {
    // Same command name, another implementation: fall back to its defaults.
    expect(sampleThresholds("check_load", vocabularyOf(help("check_load", "", "", ["queue"])))).toBeUndefined();
    expect(sampleThresholds("check_load", vocabularyOf(help("check_load", "queue > 5", "", ["queue"])))).toEqual({
      warning: ["queue > 5"],
      critical: [],
      source: "defaults",
    });
  });

  it("matches the command name without regard to case", () => {
    expect(sampleThresholds("Check_Load", vocabularyOf(help("check_load", "", "", ["load5"])))?.source).toBe("sample");
  });
});

describe("splitArguments and quoteArgument", () => {
  it("splits on spaces outside quotes", () => {
    expect(splitArguments('filter=free < 10% "warning=free < 20%" show-all=true')).toEqual([
      "filter=free",
      "<",
      "10%",
      "warning=free < 20%",
      "show-all=true",
    ]);
  });

  it("takes an escaped character literally", () => {
    expect(splitArguments('"filter=name = \\"a b\\""')).toEqual(['filter=name = "a b"']);
    expect(splitArguments("path=C:\\\\temp")).toEqual(["path=C:\\temp"]);
  });

  it("quotes only what needs it, and round-trips", () => {
    expect(quoteArgument("time=5m")).toBe("time=5m");
    expect(quoteArgument("warning=load > 80")).toBe('"warning=load > 80"');
    for (const argument of ["warning=load > 80", "critical=state = 'stopped'", 'filter=name = "a b"', "path=C:\\temp\\x y", "plain"]) {
      expect(splitArguments(quoteArgument(argument))).toEqual([argument]);
    }
  });
});

describe("addSamples and removeSamples", () => {
  const sample = { warning: ["load > 80"], critical: ["load > 90"], source: "defaults" as const };

  it("adds both levels to an empty line", () => {
    const { line, added } = addSamples("", sample);
    expect(line).toBe('"warning=load > 80" "critical=load > 90"');
    expect(added).toEqual(['"warning=load > 80"', '"critical=load > 90"']);
    expect(splitArguments(line)).toEqual(["warning=load > 80", "critical=load > 90"]);
  });

  it("appends after what is already there", () => {
    expect(addSamples("time=5m ", sample).line).toBe('time=5m "warning=load > 80" "critical=load > 90"');
  });

  it("leaves a level the line already sets, by either spelling", () => {
    expect(addSamples('"warning=load > 50"', sample)).toEqual({
      line: '"warning=load > 50" "critical=load > 90"',
      added: ['"critical=load > 90"'],
    });
    expect(addSamples('"warn=load > 50" "crit=load > 60"', sample)).toEqual({ line: '"warn=load > 50" "crit=load > 60"', added: [] });
    expect(addSamples("critical=none", sample).added).toEqual(['"warning=load > 80"']);
  });

  it("writes one argument per expression", () => {
    const two = { warning: [], critical: ["state = 'stopped'", "count = 0"], source: "sample" as const };
    const { line } = addSamples("process=nscp", two);
    expect(splitArguments(line)).toEqual(["process=nscp", "critical=state = 'stopped'", "critical=count = 0"]);
  });

  it("takes out exactly what it added and nothing else", () => {
    const start = 'time=5m  "filter=core = \'total\'"';
    const { line, added } = addSamples(start, sample);
    expect(removeSamples(line, added)).toBe(start);
    expect(removeSamples(addSamples("", sample).line, addSamples("", sample).added)).toBe("");
  });

  it("keeps a sample the user has edited since", () => {
    const { added } = addSamples("", sample);
    const edited = '"warning=load > 70" "critical=load > 90"';
    expect(removeSamples(edited, added)).toBe('"warning=load > 70"');
  });

  it("does not cut the same text out of a longer argument", () => {
    const added = ["critical=none"];
    expect(removeSamples("xcritical=none critical=none", added)).toBe("xcritical=none");
    expect(removeSamples("critical=nonesuch", added)).toBe("critical=nonesuch");
  });
});

describe("simple mode preference", () => {
  it("is off until switched on, and remembered", () => {
    expect(readSimpleMode()).toBe(false);
    writeSimpleMode(true);
    expect(readSimpleMode()).toBe(true);
    writeSimpleMode(false);
    expect(readSimpleMode()).toBe(false);
  });
});
