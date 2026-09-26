import { describe, expect, it } from "vitest";
import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
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

  it("folds a long list of volumes between whole bars, and offers the rest", async () => {
    // A file server, in one cell of a dashboard: without a fold the widget is
    // as tall as the machine has volumes. The fold lands between two bars, so
    // no volume is ever shown as half a row.
    const many: Record<string, number> = {};
    for (let i = 0; i < 11; i++) {
      const drive = String.fromCharCode(65 + i) + ":";
      Object.assign(many, {
        [`disk.free.${drive}.total`]: 100 * GB,
        [`disk.free.${drive}.free`]: 40 * GB,
        [`disk.free.${drive}.used`]: 60 * GB,
        [`disk.free.${drive}.used_pct`]: 60,
      });
    }
    const { metrics } = parseMetrics(many);
    render(<DiskFreeWidget metrics={metrics} />);

    expect(screen.getAllByRole("progressbar")).toHaveLength(8);
    expect(screen.queryByText("I:")).not.toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "… 3 more" }));
    expect(screen.getAllByRole("progressbar")).toHaveLength(11);
    expect(screen.getByText("K:")).toBeInTheDocument();
  });

  it("carries no control when every volume already fits", () => {
    const { metrics } = parseMetrics({
      "disk.free.C:.total": 100 * GB,
      "disk.free.C:.free": 40 * GB,
      "disk.free.C:.used": 60 * GB,
      "disk.free.C:.used_pct": 60,
    });
    render(<DiskFreeWidget metrics={metrics} />);

    expect(screen.queryByRole("button")).not.toBeInTheDocument();
  });
});
