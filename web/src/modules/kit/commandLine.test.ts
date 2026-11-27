import { describe, expect, it } from "vitest";
import {
  commandLineProblem,
  joinCommand,
  placeholders,
  quote,
  splitCommand,
  tokenize,
} from "./commandLine";

const raw = String.raw;

describe("command lines", () => {
  it("splits on spaces, keeping double-quoted tokens together", () => {
    expect(tokenize(`check_cpu "warn=load > 80%" "crit=state != 'x'" time=5m`)).toEqual([
      "check_cpu",
      "warn=load > 80%",
      "crit=state != 'x'",
      "time=5m",
    ]);
    // Single quotes are plain characters; empty tokens are dropped.
    expect(tokenize(`a 'b c' "" d`)).toEqual(["a", "'b", "c'", "d"]);
    expect(tokenize(raw`path=C:\\nscp a\ b \"q\"`)).toEqual([raw`path=C:\nscp`, "a b", '"q"']);
    expect(tokenize("  ")).toEqual([]);
  });

  it("says what the agent would refuse", () => {
    expect(commandLineProblem(raw`check_files "path=C:\\temp"`)).toBeNull();
    expect(commandLineProblem(raw`check_files "path=C:\temp"`)).toMatch(/Only/);
    expect(commandLineProblem(raw`check_files "path=C:\nscp"`)).toMatch(/line break/);
  });

  it("separates the command and keeps the rest as typed", () => {
    expect(splitCommand(`check_cpu  "warn=load > 80%"`)).toEqual({
      command: "check_cpu",
      args: `"warn=load > 80%"`,
    });
    expect(splitCommand("check_ok")).toEqual({ command: "check_ok", args: "" });
    // A space typed after the last argument stays while typing.
    expect(splitCommand(joinCommand("check_cpu", "time=5m ")).args).toBe("time=5m ");
  });

  it("quotes what needs it, so it comes back as itself", () => {
    expect(quote("time=5m")).toBe("time=5m");
    expect(quote("warn=load > 80%")).toBe(`"warn=load > 80%"`);
    for (const t of [`crit=state != "x"`, raw`C:\temp dir`, "a b"])
      expect(tokenize(quote(t))).toEqual([t]);
  });

  it("finds the placeholders a caller fills", () => {
    expect(placeholders(`check_files "path=$ARG1$" "crit=size > $ARG2$" $ARG1$`)).toEqual([
      "$ARG1$",
      "$ARG2$",
    ]);
    expect(placeholders("check_ok")).toEqual([]);
    expect(placeholders("x %ARG1%")).toEqual(["%ARG1%"]);
  });
});
