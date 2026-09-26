import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import FactSetsWidget, { groupFactSections } from "./FactSetsWidget";
import { SettingsDescription } from "../api/api";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const description = (over: Partial<SettingsDescription> = {}): SettingsDescription => ({
  path: "/settings/system/windows/facts",
  key: "os",
  type: "bool",
  title: "OS FACTS",
  description: "Collect the `os` fact set: the OS family, product name and kernel version.",
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

const descriptions: SettingsDescription[] = [
  description(),
  description({ key: "hardware", title: "HARDWARE FACTS", description: "Collect the `hardware` fact set." }),
  description({
    key: "software.installed",
    title: "INSTALLED SOFTWARE FACTS",
    description: "Collect the `software.installed` fact set.",
    value: "true",
  }),
  description({
    path: "/settings/disk/facts",
    key: "storage.volumes",
    title: "STORAGE VOLUMES FACTS",
    description: "Collect the `storage.volumes` fact set.",
    plugins: ["CheckDisk"],
  }),
  // The core's own section: it registers no plugin, and the two keys next to
  // its set are settings about facts rather than sets of them.
  description({ path: "/settings/facts", key: "agent", title: "AGENT FACTS", plugins: [] }),
  description({
    path: "/settings/facts",
    key: "interval",
    type: "string",
    title: "Refresh interval",
    value: "1h",
    default_value: "1h",
    is_advanced_key: true,
    plugins: [],
  }),
  // Not a fact section at all.
  description({ path: "/settings/system/windows", key: "default buffer length", type: "string", title: "LENGTH" }),
];

describe("groupFactSections", () => {
  it("takes every facts section, whichever module registered it", () => {
    const sections = groupFactSections(descriptions);
    expect(sections.map((s) => [s.owner, s.path])).toEqual([
      ["CheckDisk", "/settings/disk/facts"],
      ["CheckSystem", "/settings/system/windows/facts"],
      // The core registers no plugin, so it has no module name to report.
      ["Core", "/settings/facts"],
    ]);
  });

  it("keeps the switches and drops everything else in the section", () => {
    const sections = groupFactSections(descriptions);
    // `interval` lives in the same section and is not a set: a string, not a
    // switch.
    expect(sections[2].keys.map((k) => k.key)).toEqual(["agent"]);
    expect(sections[1].keys.map((k) => k.key)).toEqual(["hardware", "os", "software.installed"]);
  });
});

describe("FactSetsWidget", () => {
  it("lists one switch per set, showing which are on", async () => {
    installFetchMock({ "/api/v2/settings/descriptions": jsonResponse(descriptions) });
    renderWithProviders(<FactSetsWidget />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("os")).toBeInTheDocument());
    // The set's id, which is what it is called in the configuration, in the
    // facts document and in the documentation.
    expect(screen.getByText("storage.volumes")).toBeInTheDocument();
    expect(screen.getByText("agent")).toBeInTheDocument();
    // The module that produces each set, so an operator knows what enabling
    // one will cost and who to blame when it fails.
    expect(screen.getByText("CheckDisk")).toBeInTheDocument();
    expect(screen.getByText("/settings/facts")).toBeInTheDocument();

    expect(screen.getByLabelText("os")).not.toBeChecked();
    expect(screen.getByLabelText("software.installed")).toBeChecked();
  });

  it("leaves out the keys in a facts section that are not sets", async () => {
    installFetchMock({ "/api/v2/settings/descriptions": jsonResponse(descriptions) });
    renderWithProviders(<FactSetsWidget />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("agent")).toBeInTheDocument());
    expect(screen.queryByText("interval")).not.toBeInTheDocument();
    expect(screen.queryByText("default buffer length")).not.toBeInTheDocument();
  });

  it("writes the setting when a set is switched on", async () => {
    const fetchMock = installFetchMock({
      "/api/v2/settings/descriptions": jsonResponse(descriptions),
      "/api/v2/settings": jsonResponse("saved"),
    });
    renderWithProviders(<FactSetsWidget />, { withRouter: false });

    await waitFor(() => expect(screen.getByLabelText("os")).toBeInTheDocument());
    await userEvent.click(screen.getByLabelText("os"));

    await waitFor(() => {
      const put = fetchMock.mock.calls.find(([input]) => (input as Request).method === "PUT");
      expect(put).toBeDefined();
    });
    const [request] = fetchMock.mock.calls.find(([input]) => (input as Request).method === "PUT") as [Request];
    expect(await request.clone().json()).toEqual({
      path: "/settings/system/windows/facts",
      key: "os",
      value: "true",
    });
  });

  it("shows the description of a set as its tooltip", async () => {
    installFetchMock({ "/api/v2/settings/descriptions": jsonResponse(descriptions) });
    renderWithProviders(<FactSetsWidget />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("os")).toBeInTheDocument());
    await userEvent.hover(screen.getByText("os"));

    // The registered title heads the tooltip: it shouts in a list of rows,
    // and reads as a heading here.
    await waitFor(() => expect(screen.getByRole("tooltip")).toHaveTextContent("OS FACTS"));
    expect(screen.getByRole("tooltip")).toHaveTextContent("the OS family, product name and kernel version");
  });

  it("renders nothing when no loaded module produces facts", async () => {
    // Not an empty card: there is no switch to offer, and a heading with
    // nothing under it reads as a list that failed to load.
    installFetchMock({
      "/api/v2/settings/descriptions": jsonResponse([description({ path: "/settings/system/windows" })]),
    });
    const { container } = renderWithProviders(<FactSetsWidget />, { withRouter: false });

    await waitFor(() => expect(container).toBeEmptyDOMElement());
  });
});
