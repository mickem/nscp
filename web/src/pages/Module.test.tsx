import { describe, expect, it } from "vitest";
import { screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { Route, Routes } from "react-router";
import Module from "./Module";
import { SettingsDescription } from "../api/api";
import {
  authenticatedState,
  installFetchMock,
  jsonResponse,
  renderWithProviders,
} from "../test/test-utils";

const description = (over: Partial<SettingsDescription>): SettingsDescription => ({
  path: "",
  key: "",
  type: "string",
  title: "",
  description: "",
  value: "",
  default_value: "",
  icon: "",
  is_advanced_key: false,
  is_object: false,
  is_sample_key: false,
  is_template_key: false,
  plugins: [],
  sample_usage: "",
  ...over,
});

// One ordinary and one advanced key for the module under test.
function serve(moduleId: string, path: string, loaded = true) {
  installFetchMock({
    [`/api/v2/modules/${moduleId}`]: jsonResponse({
      id: moduleId,
      name: moduleId,
      title: moduleId,
      description: "",
      enabled: loaded,
      loaded,
      metadata: { alias: "", plugin_id: "0" },
      load_url: "",
      unload_url: "",
      module_url: "",
    }),
    "/api/v2/queries": jsonResponse([]),
    "/api/v2/aliases": jsonResponse([]),
    "/api/v2/settings": jsonResponse([]),
    "/api/v2/settings/descriptions": jsonResponse([
      description({ path, title: "Section", plugins: [moduleId] }),
      description({ path, key: "plain", title: "PLAIN KEY", plugins: [moduleId] }),
      description({
        path,
        key: "deep",
        title: "ADVANCED KEY",
        is_advanced_key: true,
        plugins: [moduleId],
      }),
    ]),
  });
}

function renderModule(moduleId: string) {
  return renderWithProviders(
    <Routes>
      <Route path="/modules/:id" element={<Module />} />
    </Routes>,
    { preloadedState: authenticatedState, route: `/modules/${moduleId}` },
  );
}

const expertSwitch = () => screen.getByRole("switch", { name: "Expert mode" });
const hideDefaultsSwitch = () => screen.getByRole("switch", { name: "Hide defaults" });

describe("Module page expert mode", () => {
  it("starts off for a module with a tailored page", async () => {
    serve("CheckNSCP", "/settings/nscp/check/update");
    renderModule("CheckNSCP");

    expect(await screen.findByText("PLAIN KEY")).toBeInTheDocument();
    expect(expertSwitch()).not.toBeChecked();
    expect(screen.queryByText("ADVANCED KEY")).not.toBeInTheDocument();

    await userEvent.click(expertSwitch());
    expect(await screen.findByText("ADVANCED KEY")).toBeInTheDocument();
    expect(screen.getByText(/^Expert mode: every setting/)).toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "Back to the tailored page" }));
    expect(expertSwitch()).not.toBeChecked();
    expect(screen.queryByText("ADVANCED KEY")).not.toBeInTheDocument();
  });

  it("starts on for a module without a tailored page", async () => {
    serve("CheckDisk", "/settings/disk");
    renderModule("CheckDisk");

    expect(await screen.findByText("ADVANCED KEY")).toBeInTheDocument();
    expect(expertSwitch()).toBeChecked();
    expect(screen.getByText(/has no tailored settings page yet/)).toBeInTheDocument();

    // It can still be turned off.
    await userEvent.click(expertSwitch());
    expect(screen.queryByText("ADVANCED KEY")).not.toBeInTheDocument();
    expect(screen.getByText("PLAIN KEY")).toBeInTheDocument();
    // The generic form is still there, so its filter is too.
    expect(hideDefaultsSwitch()).toBeInTheDocument();
  });

  it("offers Hide defaults only where the generic settings form shows", async () => {
    serve("CheckNSCP", "/settings/nscp/check/update");
    renderModule("CheckNSCP");

    expect(await screen.findByText("PLAIN KEY")).toBeInTheDocument();
    expect(screen.queryByRole("switch", { name: "Hide defaults" })).not.toBeInTheDocument();

    await userEvent.click(expertSwitch());
    expect(hideDefaultsSwitch()).toBeInTheDocument();
  });

  it("shows no settings for a module that is not loaded", async () => {
    serve("NRPEServer", "/settings/NRPE/server", false);
    renderModule("NRPEServer");
    expect(await screen.findByText(/Cannot show queries and settings/)).toBeInTheDocument();
    expect(screen.queryByText("PLAIN KEY")).not.toBeInTheDocument();
    expect(screen.queryByText("NRPE server")).not.toBeInTheDocument();
  });
});
