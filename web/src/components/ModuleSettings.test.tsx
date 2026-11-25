import { describe, expect, it } from "vitest";
import { screen, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import ModuleSettings from "./ModuleSettings";
import { Settings, SettingsDescription } from "../api/api";
import {
  authenticatedState,
  installFetchMock,
  jsonResponse,
  renderWithProviders,
} from "../test/test-utils";

const description = (over: Partial<SettingsDescription>): SettingsDescription => ({
  path: "",
  key: "",
  type: "",
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

// A real-time collection root, as /api/v2/settings/descriptions lists it.
const collection = (path: string, title: string, plugin: string) =>
  description({ path, title, is_object: true, plugins: [plugin] });

function serve(descriptions: SettingsDescription[], stored: Settings[] = []) {
  installFetchMock({
    "/api/v2/settings/descriptions": jsonResponse(descriptions),
    "/api/v2/settings": jsonResponse(stored),
  });
}

describe("ModuleSettings", () => {
  it("gives the Linux real-time collections the Windows layout and presets", async () => {
    serve(
      [collection("/settings/system/unix/real-time/cpu", "Realtime cpu filters", "CheckSystemUnix")],
      [{ path: "/settings/system/unix/real-time/cpu/cpu_high", key: "time", value: "5m" }],
    );
    renderWithProviders(<ModuleSettings settings={[]} moduleId="CheckSystemUnix" />, {
      preloadedState: authenticatedState,
    });

    const card = (await screen.findByText("Realtime cpu filters")).closest(".MuiCard-root");
    expect(card).not.toBeNull();
    // The preview column.
    expect(await within(card as HTMLElement).findByText("Time")).toBeInTheDocument();
    // The presets on the Add button.
    await userEvent.click(within(card as HTMLElement).getByLabelText("add from template"));
    expect(screen.getByText("Cpu above 80%")).toBeInTheDocument();
  });

  it("renders the wizard a module page declares", async () => {
    serve([]);
    renderWithProviders(<ModuleSettings settings={[]} moduleId="NRPEServer" />, {
      preloadedState: authenticatedState,
    });
    expect(await screen.findByText(/insecure/i)).toBeInTheDocument();
  });

  it("hides a collection the module page declares superseded", async () => {
    serve([
      collection("/settings/system/windows/real-time/checks", "Legacy checks", "CheckSystem"),
      collection("/settings/system/windows/real-time/cpu", "Realtime cpu filters", "CheckSystem"),
    ]);
    renderWithProviders(<ModuleSettings settings={[]} moduleId="CheckSystem" />, {
      preloadedState: authenticatedState,
    });
    expect(await screen.findByText("Realtime cpu filters")).toBeInTheDocument();
    expect(screen.queryByText("Legacy checks")).not.toBeInTheDocument();
  });

  it("shows a gated collection only while its gate is on", async () => {
    const filters = collection(
      "/settings/eventlog/real-time/filters",
      "Real-time filters",
      "CheckEventLog",
    );
    const gate = (value: string) =>
      description({ path: "/settings/eventlog/real-time", key: "enabled", type: "bool", value });

    serve([filters, gate("false")]);
    const off = renderWithProviders(<ModuleSettings settings={[]} moduleId="CheckEventLog" />, {
      preloadedState: authenticatedState,
    });
    // Nothing else to render: the component renders nothing at all.
    await new Promise((r) => setTimeout(r, 50));
    expect(screen.queryByText("Real-time filters")).not.toBeInTheDocument();
    off.unmount();

    serve([filters, gate("true")]);
    renderWithProviders(<ModuleSettings settings={[]} moduleId="CheckEventLog" />, {
      preloadedState: authenticatedState,
    });
    expect(await screen.findByText("Real-time filters")).toBeInTheDocument();
  });
});
