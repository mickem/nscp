import { describe, expect, it } from "vitest";
import { fireEvent, screen, waitFor, within } from "@testing-library/react";
import Metrics from "./Metrics";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const described = () =>
  jsonResponse({
    metrics: {
      "disk.free.C:.free": 688604987392,
      "system.cpu.core 0.idle": 93,
      "workers.jobs": 5,
      // A script module publishes in a bundle with no name.
      ".world": "safe",
    },
    metadata: {
      "disk.free.C:.free": { type: "gauge", help: "Free space on the volume", unit: "bytes" },
      "system.cpu.core 0.idle": { type: "gauge", help: "Idle time", unit: "percent", labels: { core: "0" } },
      "workers.jobs": { type: "counter" },
      ".world": { type: "info" },
    },
  });

const dataRows = () => screen.getAllByRole("row").slice(1);

describe("Metrics", () => {
  it("shows what each metric means next to its value", async () => {
    installFetchMock({ "/api/v2/metrics": described() });
    renderWithProviders(<Metrics />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("Free space on the volume")).toBeInTheDocument());
    const row = screen.getByText("Idle time").closest("tr") as HTMLElement;
    expect(within(row).getByText("percent")).toBeInTheDocument();
    expect(within(row).getByText("gauge")).toBeInTheDocument();
    expect(within(row).getByText("core=0")).toBeInTheDocument();
    expect(within(screen.getByText("jobs").closest("tr") as HTMLElement).getByText("counter")).toBeInTheDocument();
  });

  it("filters on the module that has no name", async () => {
    installFetchMock({ "/api/v2/metrics": described() });
    renderWithProviders(<Metrics />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("world")).toBeInTheDocument());
    expect(dataRows()).toHaveLength(4);

    fireEvent.click(screen.getByRole("button", { name: "(unnamed)" }));
    expect(dataRows()).toHaveLength(1);
    expect(screen.getByText("safe")).toBeInTheDocument();

    fireEvent.click(screen.getByRole("button", { name: "all modules" }));
    expect(dataRows()).toHaveLength(4);
  });

  it("selects a module exactly and narrows it by type", async () => {
    installFetchMock({ "/api/v2/metrics": described() });
    renderWithProviders(<Metrics />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("world")).toBeInTheDocument());
    fireEvent.click(screen.getByRole("button", { name: "system" }));
    expect(dataRows()).toHaveLength(1);
    fireEvent.click(screen.getByRole("button", { name: "cpu" }));
    expect(dataRows()).toHaveLength(1);
    expect(screen.getByText("idle")).toBeInTheDocument();
  });

  it("searches the help text", async () => {
    installFetchMock({ "/api/v2/metrics": described() });
    renderWithProviders(<Metrics />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("world")).toBeInTheDocument());
    fireEvent.change(screen.getByPlaceholderText("Filter metrics…"), { target: { value: "free space" } });
    await waitFor(() => expect(dataRows()).toHaveLength(1));
    expect(screen.getByText("1/4")).toBeInTheDocument();
  });

  it("renders an empty table before the first snapshot", async () => {
    installFetchMock({ "/api/v2/metrics": new Response("", { status: 200 }) });
    renderWithProviders(<Metrics />, { withRouter: false });

    await waitFor(() => expect(screen.getByRole("button", { name: "all modules" })).toBeInTheDocument());
    expect(dataRows()).toHaveLength(0);
  });
});
