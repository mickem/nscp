import { describe, expect, it } from "vitest";
import { parseMetrics } from "./metric_parser";

describe("parseMetrics", () => {
  it("returns an empty result for undefined input", () => {
    const result = parseMetrics(undefined);
    expect(result.metrics).toEqual([]);
    expect(result.modules).toEqual([]);
  });

  it("parses a two-part key as module + metric", () => {
    const result = parseMetrics({ "workers.jobs": 5 });
    expect(result.metrics).toEqual([
      { module: "workers", key: "workers.jobs", metric: "jobs", value: 5 },
    ]);
    expect(result.modules).toEqual(["workers"]);
  });

  it("parses a three-part key as module + type + metric", () => {
    const result = parseMetrics({ "system.uptime.uptime": "3d 4h" });
    expect(result.metrics).toEqual([
      {
        module: "system",
        type: "uptime",
        key: "system.uptime.uptime",
        metric: "uptime",
        value: "3d 4h",
      },
    ]);
  });

  it("parses a four-part key with an instance", () => {
    const result = parseMetrics({ "disk.free.C:.total": 1024 });
    expect(result.metrics).toEqual([
      {
        module: "disk",
        type: "free",
        instance: "C:",
        key: "disk.free.C:.total",
        metric: "total",
        value: 1024,
      },
    ]);
  });

  it("joins multi-part instances with dots", () => {
    const result = parseMetrics({ "system.network.my.fancy.nic.NetConnectionID": "eth0" });
    expect(result.metrics[0].instance).toBe("my.fancy.nic");
    expect(result.metrics[0].metric).toBe("NetConnectionID");
  });

  it("collects unique modules across metrics", () => {
    const result = parseMetrics({
      "system.cpu.total.kernel": 10,
      "system.cpu.total.user": 20,
      "disk.free.C:.total": 100,
      "workers.jobs": 1,
    });
    expect(result.modules).toEqual(["system", "disk", "workers"]);
    expect(result.metrics).toHaveLength(4);
  });
});
