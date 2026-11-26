import { describe, expect, it } from "vitest";
import { ALL_COMMANDS, allowRisk, allowValue, formatAllow, parseAllow } from "./nsclient";

describe("check_nt allowed commands", () => {
  it("reads the spellings the agent takes", () => {
    expect([...parseAllow("any").commands].sort()).toEqual([...ALL_COMMANDS].sort());
    expect([...parseAllow(" ALL ").commands].length).toBe(ALL_COMMANDS.length);
    expect([...parseAllow("metrics, info").commands].sort()).toEqual(
      ["clientversion", "cpuload", "memuse", "uptime", "useddiskspace"].sort(),
    );
    expect(parseAllow("cpuload,Counter").commands).toEqual(new Set(["cpuload", "counter"]));
    expect(parseAllow("cpuload, bogus").unknown).toEqual(["bogus"]);
    expect(parseAllow("").commands.size).toBe(0);
  });

  it("writes the shortest spelling, keeping names it does not know", () => {
    expect(formatAllow(parseAllow("metrics,info,service,process,counters,files"))).toBe("any");
    expect(formatAllow(parseAllow("cpuload,uptime,useddiskspace,memuse,counter"))).toBe(
      "metrics, counter",
    );
    expect(formatAllow(parseAllow("info, bogus"))).toBe("info, bogus");
  });

  it("keeps a stored spelling that means the same", () => {
    expect(allowValue("all", parseAllow("any"))).toBe("all");
    expect(allowValue("all", parseAllow("metrics"))).toBe("metrics");
  });

  it("flags the commands that read more than metrics", () => {
    expect(allowRisk(parseAllow("metrics, info").commands)).toBeNull();
    expect(allowRisk(parseAllow("fileage").commands)?.reason).toMatch(/whether any file exists/);
  });
});
