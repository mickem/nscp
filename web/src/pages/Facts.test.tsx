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
    gathered: { os: "2026-09-23T08:00:00Z", hardware: "2026-09-23T08:00:00Z" },
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

  it("separates when the agent last asked from when the values were read", async () => {
    // The round happened at 10:00 but the producer read the machine at 08:00
    // and has been handing back that snapshot since. Showing the round time
    // against the values would claim they are two hours fresher than they are.
    installFetchMock({ "/api/v2/facts": envelope() });
    renderWithProviders(<Facts />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("revision 7")).toBeInTheDocument());
    const checked = screen.getByText(/^checked /);
    expect(checked).toBeInTheDocument();
    expect(checked.textContent).toContain(new Date("2026-09-23T10:00:00Z").toLocaleString());

    const gathered = screen.getAllByText(/^gathered /);
    expect(gathered).toHaveLength(2);
    gathered.forEach((node) => expect(node.textContent).toContain(new Date("2026-09-23T08:00:00Z").toLocaleString()));
  });

  it("falls back to the round time for a set that did not say when it read", async () => {
    // Right for a producer that collects every round: it read them just now.
    installFetchMock({ "/api/v2/facts": envelope({ gathered: {} }) });
    renderWithProviders(<Facts />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("os")).toBeInTheDocument());
    const gathered = screen.getAllByText(/^gathered /);
    gathered.forEach((node) => expect(node.textContent).toContain(new Date("2026-09-23T10:00:00Z").toLocaleString()));
  });

  it("offers the switches when nothing is collected", async () => {
    // A fresh install: facts are opt-in, so an empty document is the normal
    // state and has to read as a next step rather than as a failure. The next
    // step is on the page, which is why the empty state points at it rather
    // than quoting an ini section to go and edit.
    installFetchMock({
      "/api/v2/facts": envelope({ revision: 0, collected: "", found: false, enabled: [], facts: {} }),
      "/api/v2/settings/descriptions": jsonResponse([
        {
          path: "/settings/system/windows/facts",
          key: "os",
          type: "bool",
          title: "OS FACTS",
          description: "Collect the `os` fact set.",
          value: "false",
          default_value: "false",
          icon: "",
          is_advanced_key: false,
          is_object: false,
          is_sample_key: false,
          is_template_key: false,
          plugins: ["CheckSystem"],
          sample_usage: "",
        },
      ]),
    });
    renderWithProviders(<Facts />, { withRouter: false });

    await waitFor(() => expect(screen.getByText(/No facts collected/)).toBeInTheDocument());
    expect(await screen.findByText("Fact sets")).toBeInTheDocument();
    expect(await screen.findByLabelText("os")).not.toBeChecked();
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
