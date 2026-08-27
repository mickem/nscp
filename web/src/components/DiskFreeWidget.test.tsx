import { describe, expect, it } from "vitest";
import { render, screen } from "@testing-library/react";
import DiskFreeWidget from "./DiskFreeWidget";
import { parseMetrics } from "../metric_parser";

const GB = 1024 ** 3;

describe("DiskFreeWidget", () => {
  it("renders nothing without disk metrics", () => {
    const { container } = render(<DiskFreeWidget metrics={[]} />);
    expect(container).toBeEmptyDOMElement();
  });

  it("renders a usage bar per disk with formatted sizes", () => {
    const { metrics } = parseMetrics({
      "disk.free.C:.total": 100 * GB,
      "disk.free.C:.free": 40 * GB,
      "disk.free.C:.used": 60 * GB,
      "disk.free.C:.used_pct": 60,
      "disk.free.D:.total": 2 * 1024 ** 4,
      "disk.free.D:.free": 1024 ** 4,
      "disk.free.D:.used": 1024 ** 4,
      "disk.free.D:.used_pct": 50,
    });
    render(<DiskFreeWidget metrics={metrics} />);

    expect(screen.getByText("Disk Space")).toBeInTheDocument();
    expect(screen.getByText("C:")).toBeInTheDocument();
    expect(screen.getByText("D:")).toBeInTheDocument();
    expect(screen.getByText("60%")).toBeInTheDocument();
    expect(screen.getByText("60.0 GB used · 40.0 GB free · 100.0 GB total")).toBeInTheDocument();
    expect(screen.getByText("1.0 TB used · 1.0 TB free · 2.0 TB total")).toBeInTheDocument();
    expect(screen.getAllByRole("progressbar")).toHaveLength(2);
  });

  it("skips disks reporting a zero total", () => {
    const { metrics } = parseMetrics({
      "disk.free.E:.total": 0,
      "disk.free.E:.used_pct": 0,
    });
    const { container } = render(<DiskFreeWidget metrics={metrics} />);
    expect(container).toBeEmptyDOMElement();
  });
});
