import { describe, expect, it } from "vitest";
import { screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import Modules from "./Modules";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const moduleFixture = (
  id: string,
  loaded: boolean,
  enabled: boolean,
  description: string,
  experimental = false,
) => ({
  id,
  name: id,
  title: id,
  description,
  enabled,
  loaded,
  experimental,
  metadata: { alias: "", plugin_id: "0" },
  load_url: "",
  unload_url: "",
  module_url: "",
});

const MODULES = [
  moduleFixture("CheckDisk", true, true, "Monitors disk usage"),
  moduleFixture("CheckSystem", true, false, "Monitors cpu and memory"),
  moduleFixture("WEBServer", false, false, "Serves the web interface"),
  moduleFixture("CheckSecurity", true, true, "Checks host security posture", true),
];

function setup() {
  installFetchMock({
    "/api/v2/modules": jsonResponse(MODULES),
  });
  return renderWithProviders(<Modules />);
}

describe("Modules page", () => {
  it("renders the module list with names and descriptions", async () => {
    setup();

    expect(screen.getByText("Modules")).toBeInTheDocument();
    expect(await screen.findByText("CheckDisk")).toBeInTheDocument();
    expect(screen.getByText("CheckSystem")).toBeInTheDocument();
    expect(screen.getByText("WEBServer")).toBeInTheDocument();
    expect(screen.getByText("Monitors disk usage")).toBeInTheDocument();
  });

  it("filters modules by name and description and shows the hit count", async () => {
    setup();
    await screen.findByText("CheckDisk");

    await userEvent.type(screen.getByPlaceholderText("Filter modules"), "disk");

    expect(screen.getByText("CheckDisk")).toBeInTheDocument();
    expect(screen.queryByText("CheckSystem")).not.toBeInTheDocument();
    expect(screen.queryByText("WEBServer")).not.toBeInTheDocument();
    expect(screen.getByText("1/4")).toBeInTheDocument();
  });

  it("marks an experimental module and leaves the others unmarked", async () => {
    setup();
    await screen.findByText("CheckSecurity");

    // One marker, on the one module that declared itself experimental.
    const markers = screen.getAllByText("Experimental");
    expect(markers).toHaveLength(1);
    expect(markers[0].closest("li")).toHaveTextContent("CheckSecurity");
  });

  it("filters on the experimental marker", async () => {
    setup();
    await screen.findByText("CheckSecurity");

    await userEvent.type(screen.getByPlaceholderText("Filter modules"), "experimental");

    expect(screen.getByText("CheckSecurity")).toBeInTheDocument();
    expect(screen.queryByText("CheckDisk")).not.toBeInTheDocument();
    expect(screen.getByText("1/4")).toBeInTheDocument();
  });

  it("shows an empty state when the filter matches nothing", async () => {
    setup();
    await screen.findByText("CheckDisk");

    await userEvent.type(screen.getByPlaceholderText("Filter modules"), "nonexistent");

    expect(screen.getByText(/No modules match/)).toBeInTheDocument();
    expect(screen.getByText("0/4")).toBeInTheDocument();
  });
});
