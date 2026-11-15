import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import DiagnosticsSettingsWidget, { groupDiagnosticsSections } from "./DiagnosticsSettingsWidget";
import { SettingsDescription } from "../api/api";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const description = (over: Partial<SettingsDescription> = {}): SettingsDescription => ({
  path: "/settings/system/windows/diagnostics",
  key: "processes",
  type: "bool",
  title: "TOP PROCESSES",
  description: "Produce the processes.top_cpu and processes.top_memory diagnostics tables.",
  value: "false",
  default_value: "false",
  icon: "",
  is_advanced_key: false,
  is_object: false,
  is_sample_key: false,
  is_template_key: false,
  plugins: ["CheckSystem"],
  sample_usage: "",
  ...over,
});

const capture = (value: string) =>
  description({
    path: "/settings/core/failed checks",
    key: "capture diagnostics",
    title: "Collect diagnostics and record them with each failure",
    description: "Collect diagnostics - small tables of what is going on right now.",
    value,
    is_advanced_key: true,
    plugins: [],
  });

const tables: SettingsDescription[] = [
  description(),
  description({ key: "services", title: "SERVICE PROBLEMS", value: "true" }),
  description({
    path: "/settings/eventlog/diagnostics",
    key: "errors",
    title: "EVENT LOG ERRORS",
    description: "Produce the eventlog.errors diagnostics table.",
    plugins: ["CheckEventLog"],
  }),
  description({
    path: "/settings/eventlog/diagnostics",
    key: "services",
    title: "SERVICE EVENTS",
    plugins: ["CheckEventLog"],
  }),
  description({
    path: "/settings/disk/diagnostics",
    key: "growth spots",
    title: "GROWTH SPOTS",
    plugins: ["CheckDisk"],
  }),
  // A setting about a table, not a table.
  description({
    path: "/settings/disk/diagnostics",
    key: "growth spots interval",
    type: "string",
    title: "GROWTH SPOTS INTERVAL",
    value: "10m",
    default_value: "10m",
    plugins: ["CheckDisk"],
  }),
  // The capture switch's neighbours are not diagnostics switches.
  description({ path: "/settings/core/failed checks", key: "capture metrics", plugins: [] }),
];

const descriptions = (captureValue = "true") => [...tables, capture(captureValue)];

describe("groupDiagnosticsSections", () => {
  it("puts the capture switch first, then every diagnostics section", () => {
    const sections = groupDiagnosticsSections(descriptions());
    expect(sections.map((s) => [s.owner, s.path])).toEqual([
      ["Core", "/settings/core/failed checks"],
      ["CheckDisk", "/settings/disk/diagnostics"],
      ["CheckEventLog", "/settings/eventlog/diagnostics"],
      ["CheckSystem", "/settings/system/windows/diagnostics"],
    ]);
  });

  it("keeps the switches and drops everything else", () => {
    const sections = groupDiagnosticsSections(descriptions());
    expect(sections[0].keys.map((k) => k.key)).toEqual(["capture diagnostics"]);
    expect(sections[1].keys.map((k) => k.key)).toEqual(["growth spots"]);
    expect(sections[3].keys.map((k) => k.key)).toEqual(["processes", "services"]);
  });
});

describe("DiagnosticsSettingsWidget", () => {
  it("lists the capture switch and one switch per table, showing which are on", async () => {
    installFetchMock({ "/api/v2/settings/descriptions": jsonResponse(descriptions()) });
    renderWithProviders(<DiagnosticsSettingsWidget />, { withRouter: false });

    await waitFor(() => expect(screen.getByLabelText("Core: capture diagnostics")).toBeChecked());
    expect(screen.getByText("/settings/eventlog/diagnostics")).toBeInTheDocument();
    // Two modules have a `services` table, so the switch names its module.
    expect(screen.getByLabelText("CheckSystem: services")).toBeChecked();
    expect(screen.getByLabelText("CheckEventLog: services")).not.toBeChecked();
    expect(screen.getByLabelText("CheckDisk: growth spots")).not.toBeChecked();
    expect(screen.queryByText("growth spots interval")).not.toBeInTheDocument();
    expect(screen.queryByText("capture metrics")).not.toBeInTheDocument();
    expect(screen.queryByText(/none of them is collected/)).not.toBeInTheDocument();
  });

  it("says that tables are not collected while capture is off", async () => {
    installFetchMock({ "/api/v2/settings/descriptions": jsonResponse(descriptions("false")) });
    renderWithProviders(<DiagnosticsSettingsWidget />, { withRouter: false });

    await waitFor(() => expect(screen.getByLabelText("Core: capture diagnostics")).not.toBeChecked());
    expect(screen.getByText(/none of them is collected/)).toBeInTheDocument();
  });

  it("writes the setting when capture is switched on", async () => {
    const fetchMock = installFetchMock({
      "/api/v2/settings/descriptions": jsonResponse(descriptions("false")),
      "/api/v2/settings": jsonResponse("saved"),
    });
    renderWithProviders(<DiagnosticsSettingsWidget />, { withRouter: false });

    await waitFor(() => expect(screen.getByLabelText("Core: capture diagnostics")).toBeInTheDocument());
    await userEvent.click(screen.getByLabelText("Core: capture diagnostics"));

    await waitFor(() => {
      const put = fetchMock.mock.calls.find(([input]) => (input as Request).method === "PUT");
      expect(put).toBeDefined();
    });
    const [request] = fetchMock.mock.calls.find(([input]) => (input as Request).method === "PUT") as [Request];
    expect(await request.clone().json()).toEqual({
      path: "/settings/core/failed checks",
      key: "capture diagnostics",
      value: "true",
    });
  });

  it("writes a table's switch to its module's section", async () => {
    const fetchMock = installFetchMock({
      "/api/v2/settings/descriptions": jsonResponse(descriptions()),
      "/api/v2/settings": jsonResponse("saved"),
    });
    renderWithProviders(<DiagnosticsSettingsWidget />, { withRouter: false });

    await waitFor(() => expect(screen.getByLabelText("CheckEventLog: errors")).toBeInTheDocument());
    await userEvent.click(screen.getByLabelText("CheckEventLog: errors"));

    await waitFor(() => {
      const put = fetchMock.mock.calls.find(([input]) => (input as Request).method === "PUT");
      expect(put).toBeDefined();
    });
    const [request] = fetchMock.mock.calls.find(([input]) => (input as Request).method === "PUT") as [Request];
    expect(await request.clone().json()).toEqual({
      path: "/settings/eventlog/diagnostics",
      key: "errors",
      value: "true",
    });
  });

  it("renders nothing when no diagnostics setting is described", async () => {
    // Not an empty card: there is no switch to offer.
    installFetchMock({
      "/api/v2/settings/descriptions": jsonResponse([description({ path: "/settings/system/windows" })]),
    });
    const { container } = renderWithProviders(<DiagnosticsSettingsWidget />, { withRouter: false });

    await waitFor(() => expect(container).toBeEmptyDOMElement());
  });
});
