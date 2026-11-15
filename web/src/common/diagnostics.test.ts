import { describe, expect, it } from "vitest";
import { formatCell, orderSectionNames } from "./diagnostics";
import { formatBytes } from "./format";

describe("orderSectionNames", () => {
  it("puts failures.recent first and the rest alphabetically", () => {
    expect(
      orderSectionNames(["services.problems", "processes.top_cpu", "failures.recent", "disk.growth_spots"]),
    ).toEqual(["failures.recent", "disk.growth_spots", "processes.top_cpu", "services.problems"]);
  });
});

describe("formatBytes", () => {
  it("scales without locale separators", () => {
    expect(formatBytes(0)).toBe("0 B");
    expect(formatBytes(1023)).toBe("1023 B");
    expect(formatBytes(2048)).toBe("2.0 KiB");
    expect(formatBytes(4 * 1024 ** 2)).toBe("4.0 MiB");
    expect(formatBytes(36507222016)).toBe("34.0 GiB");
    expect(formatBytes(3 * 1024 ** 4)).toBe("3.0 TiB");
  });
});

describe("formatCell", () => {
  it("formats by type and unit", () => {
    expect(formatCell({ name: "s", type: "int", unit: "bytes" }, 412000256)).toBe("392.9 MiB");
    expect(formatCell({ name: "s", type: "int", unit: "bytes" }, 512)).toBe("512 B");
    expect(formatCell({ name: "s", type: "int", unit: "bytes" }, -1536)).toBe("-1.5 KiB");
    expect(formatCell({ name: "c", type: "float", unit: "percent" }, 2.125)).toBe("2.1 %");
    expect(formatCell({ name: "f", type: "float" }, 1.23456)).toBe("1.23");
    expect(formatCell({ name: "d", type: "int", unit: "seconds" }, 30)).toBe("30 seconds");
    expect(formatCell({ name: "i", type: "int" }, 1234567)).toBe("1234567");
    expect(formatCell({ name: "b", type: "bool" }, false)).toBe("no");
    expect(formatCell({ name: "t", type: "timestamp" }, "2026-11-15T10:41:52Z")).toBe(
      new Date("2026-11-15T10:41:52Z").toLocaleString(),
    );
    expect(formatCell({ name: "t", type: "timestamp" }, "not a time")).toBe("not a time");
    expect(formatCell({ name: "n", type: "string" }, null)).toBe("-");
    expect(formatCell({ name: "n", type: "int" }, undefined)).toBe("-");
  });
});
