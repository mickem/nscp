import { describe, expect, it } from "vitest";
import { render, screen } from "@testing-library/react";
import SystemInfoWidget from "./SystemInfoWidget";
import { parseMetrics } from "../metric_parser";

describe("SystemInfoWidget", () => {
  it("renders nothing when no known metric is present", () => {
    const { container } = render(<SystemInfoWidget metrics={[]} />);
    expect(container).toBeEmptyDOMElement();
  });

  it("renders one row per available metric", () => {
    const { metrics } = parseMetrics({
      "system.uptime.uptime": "3d 2:01",
      "system.metrics.procs.procs": 123,
      "workers.jobs": 4,
    });
    render(<SystemInfoWidget metrics={metrics} />);

    expect(screen.getByText("System Info")).toBeInTheDocument();
    expect(screen.getByRole("row", { name: /Uptime 3d 2:01/ })).toBeInTheDocument();
    expect(screen.getByRole("row", { name: /Processes 123/ })).toBeInTheDocument();
    expect(screen.getByRole("row", { name: /Worker jobs 4/ })).toBeInTheDocument();
    // Rows whose metric is missing must not render at all.
    expect(screen.queryByText("Threads")).not.toBeInTheDocument();
    expect(screen.queryByText("Handles")).not.toBeInTheDocument();
  });

  it("formats large numbers with locale separators and rounds floats", () => {
    const { metrics } = parseMetrics({
      "system.metrics.procs.threads": 1234567,
      "system.metrics.procs.handles": 98.7,
    });
    render(<SystemInfoWidget metrics={metrics} />);
    expect(screen.getByText((1234567).toLocaleString())).toBeInTheDocument();
    expect(screen.getByRole("row", { name: /Handles 99/ })).toBeInTheDocument();
  });
});
