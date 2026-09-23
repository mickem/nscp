import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import Facts from "./Facts";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const envelope = (over: Record<string, unknown> = {}) =>
  jsonResponse({
    revision: 7,
    collected: "2026-09-23T10:00:00Z",
    path: "",
    found: true,
    enabled: ["hardware", "os"],
    errors: {},
    facts: {
      os: { family: "windows", name: "Windows 11 24H2", version: "10.0.26200" },
      hardware: { manufacturer: "Dell Inc.", cpu_cores: 20, memory_gb: 32 },
    },
    ...over,
  });

describe("Facts", () => {
  it("renders one card per fact set, with the fields sorted", async () => {
    installFetchMock({ "/api/v2/facts": envelope() });
    renderWithProviders(<Facts />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("hardware")).toBeInTheDocument());
    expect(screen.getByText("os")).toBeInTheDocument();
    expect(screen.getByText("Windows 11 24H2")).toBeInTheDocument();
    expect(screen.getByText("Dell Inc.")).toBeInTheDocument();
    // Numbers survive as numbers rather than being dropped as falsy.
    expect(screen.getByText("20")).toBeInTheDocument();
    expect(screen.getByText("32")).toBeInTheDocument();
  });

  it("shows the revision and when the inventory was last collected", async () => {
    installFetchMock({ "/api/v2/facts": envelope() });
    renderWithProviders(<Facts />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("revision 7")).toBeInTheDocument());
    expect(screen.getByText(/^collected /)).toBeInTheDocument();
  });

  it("explains how to turn a set on when nothing is collected", async () => {
    // A fresh install: facts are opt-in, so an empty document is the normal
    // state and has to read as a next step rather than as a failure.
    installFetchMock({
      "/api/v2/facts": envelope({ revision: 0, collected: "", found: false, enabled: [], facts: {} }),
    });
    renderWithProviders(<Facts />, { withRouter: false });

    await waitFor(() => expect(screen.getByText(/No facts collected/)).toBeInTheDocument());
    expect(screen.getByText(/\[\/settings\/system\/windows\/facts\] os = true/)).toBeInTheDocument();
  });

  it("flags a set that failed to collect as stale, and keeps showing its values", async () => {
    installFetchMock({
      "/api/v2/facts": envelope({ errors: { hardware: "WMI query timed out" } }),
    });
    renderWithProviders(<Facts />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("WMI query timed out")).toBeInTheDocument());
    expect(screen.getByText("stale")).toBeInTheDocument();
    // The last good values are still there - a failed round keeps them.
    expect(screen.getByText("Dell Inc.")).toBeInTheDocument();
  });
});
