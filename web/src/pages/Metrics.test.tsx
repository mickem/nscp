import { describe, expect, it } from "vitest";
import { fireEvent, screen, waitFor, within } from "@testing-library/react";
import Metrics from "./Metrics";
import { formatMetricValue } from "../common/format";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const described = () =>
  jsonResponse({
    metrics: {
      "disk.free.C:.free": 688604987392,
      "system.cpu.core 0.idle": 93,
      "system.cpu.core 0.user": 4.123456,
      "workers.jobs": 5,
      // A script module publishes in a bundle with no name.
      ".world": "safe",
    },
    metadata: {
      "disk.free.C:.free": { type: "gauge", help: "Free space on the volume", unit: "bytes" },
      "system.cpu.core 0.idle": {
        type: "gauge",
        help: "Idle time",
        unit: "percent",
        labels: { core: "0" },
      },
      "system.cpu.core 0.user": {
        type: "gauge",
        help: "User time",
        unit: "percent",
        labels: { core: "0", mode: "user" },
      },
      "workers.jobs": { type: "counter" },
      ".world": { type: "info" },
    },
  });

// One row per metric; the rows that head a group are not counted.
const dataRows = () => [...document.querySelectorAll<HTMLElement>("tr[data-metric]")];
const groupRow = (path: string) =>
  document.querySelector<HTMLElement>(`tr[data-group="${path}"]`) as HTMLElement;

describe("Metrics", () => {
  it("shows what each metric means next to its value", async () => {
    installFetchMock({ "/api/v2/metrics": described() });
    renderWithProviders(<Metrics />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("Free space on the volume")).toBeInTheDocument());
    const row = screen.getByText("Idle time").closest("tr") as HTMLElement;
    expect(within(row).getByText("93 %")).toBeInTheDocument();
    expect(within(row).getByText("gauge")).toBeInTheDocument();
    expect(
      within(screen.getByText("jobs").closest("tr") as HTMLElement).getByText("counter"),
    ).toBeInTheDocument();
  });

  it("says the module, type and instance once per group, with the labels the group shares", async () => {
    installFetchMock({ "/api/v2/metrics": described() });
    renderWithProviders(<Metrics />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("Idle time")).toBeInTheDocument());
    const group = groupRow("system.cpu.core 0");
    expect(within(group).getByText("system › cpu › core 0")).toBeInTheDocument();
    expect(within(group).getByText("core=0")).toBeInTheDocument();
    // A label only one metric carries stays on that metric's row.
    expect(within(group).queryByText("mode=user")).toBeNull();
    const user = screen.getByText("User time").closest("tr") as HTMLElement;
    expect(within(user).getByText("mode=user")).toBeInTheDocument();
    expect(within(user).queryByText("core=0")).toBeNull();
    expect(groupRow("(unnamed)")).not.toBeNull();
  });

  it("scales a byte count and keeps the exact value in the tooltip", async () => {
    installFetchMock({ "/api/v2/metrics": described() });
    renderWithProviders(<Metrics />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("641.3 GiB")).toBeInTheDocument());
    expect(screen.getByText("641.3 GiB")).toHaveAttribute("title", "688604987392 bytes");
  });

  it("formats a value by its unit", () => {
    expect(formatMetricValue(1024, "bytes")).toBe("1.0 KiB");
    expect(formatMetricValue(4.123456, "percent")).toBe("4.123 %");
    expect(formatMetricValue(12, "milliseconds")).toBe("12 ms");
    expect(formatMetricValue(7, "furlongs")).toBe("7 furlongs");
    expect(formatMetricValue(1.2768852774977912)).toBe("1.277");
    expect(formatMetricValue("safe")).toBe("safe");
  });

  it("filters on the module that has no name", async () => {
    installFetchMock({ "/api/v2/metrics": described() });
    renderWithProviders(<Metrics />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("world")).toBeInTheDocument());
    expect(dataRows()).toHaveLength(5);

    fireEvent.click(screen.getByRole("button", { name: "(unnamed)" }));
    expect(dataRows()).toHaveLength(1);
    expect(screen.getByText("safe")).toBeInTheDocument();

    fireEvent.click(screen.getByRole("button", { name: "all modules" }));
    expect(dataRows()).toHaveLength(5);
  });

  it("selects a module exactly and narrows it by type", async () => {
    installFetchMock({ "/api/v2/metrics": described() });
    renderWithProviders(<Metrics />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("world")).toBeInTheDocument());
    fireEvent.click(screen.getByRole("button", { name: "system" }));
    expect(dataRows()).toHaveLength(2);
    fireEvent.click(screen.getByRole("button", { name: "cpu" }));
    expect(dataRows()).toHaveLength(2);
    expect(screen.getByText("idle")).toBeInTheDocument();
  });

  it("searches the help text", async () => {
    installFetchMock({ "/api/v2/metrics": described() });
    renderWithProviders(<Metrics />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("world")).toBeInTheDocument());
    fireEvent.change(screen.getByPlaceholderText("Filter metrics…"), {
      target: { value: "free space" },
    });
    await waitFor(() => expect(dataRows()).toHaveLength(1));
    expect(screen.getByText("1/5")).toBeInTheDocument();
  });

  it("renders an empty table before the first snapshot", async () => {
    installFetchMock({ "/api/v2/metrics": new Response("", { status: 200 }) });
    renderWithProviders(<Metrics />, { withRouter: false });

    await waitFor(() =>
      expect(screen.getByRole("button", { name: "all modules" })).toBeInTheDocument(),
    );
    expect(dataRows()).toHaveLength(0);
  });
});
