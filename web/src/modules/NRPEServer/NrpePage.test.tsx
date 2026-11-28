import { describe, expect, it, vi } from "vitest";
import { screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import NrpePage from "./NrpePage";
import { Settings } from "../../api/api";
import {
  authenticatedState,
  installFetchMock,
  jsonResponse,
  renderWithProviders,
} from "../../test/test-utils";
import { INSECURE_CIPHERS, NRPE_PATH } from "./nrpe";

const nrpe = (key: string, value: string): Settings => ({ path: NRPE_PATH, key, value });

// Serves the stored settings and records every write.
function serve(stored: Settings[]) {
  const writes: string[] = [];
  installFetchMock({
    "/api/v2/settings": async (req: Request) => {
      if (req.method === "PUT") {
        const body = (await req.json()) as Settings;
        writes.push(`PUT [${body.path}] ${body.key}=${body.value}`);
        return jsonResponse("");
      }
      return jsonResponse(stored);
    },
  });
  return writes;
}

function render(stored: Settings[], onExpertMode = () => {}) {
  const writes = serve(stored);
  renderWithProviders(<NrpePage onExpertMode={onExpertMode} />, {
    preloadedState: authenticatedState,
  });
  return writes;
}

const radio = (name: RegExp) => screen.getByRole("radio", { name });

describe("NRPE page", () => {
  it("reads a fresh install as one-way TLS, says what that lacks, and has nothing to save", async () => {
    render([]);
    await waitFor(() => expect(radio(/^One-way TLS/)).toBeChecked());
    expect(screen.getByText("Clients are not authenticated")).toBeInTheDocument();
    expect(radio(/^No arguments/)).toBeChecked();
    // Nothing to save: no save bar at all.
    expect(screen.queryByRole("region", { name: "Unsaved changes" })).not.toBeInTheDocument();
    expect(screen.queryByRole("button", { name: "Save" })).not.toBeInTheDocument();
  });

  it("shows insecure as insecure", async () => {
    render([nrpe("insecure", "true"), nrpe("allowed ciphers", INSECURE_CIPHERS)]);
    await waitFor(() => expect(radio(/^Insecure/)).toBeChecked());
    expect(screen.getByText("NRPE is insecure")).toBeInTheDocument();
    // No certificate fields in insecure mode.
    expect(screen.queryByLabelText("Server certificate")).not.toBeInTheDocument();
  });

  it("keeps a configuration it does not recognise, and points to expert mode", async () => {
    const onExpertMode = vi.fn();
    render([nrpe("verify mode", "peer")], onExpertMode);
    await waitFor(() => expect(radio(/^Custom/)).toBeChecked());
    expect(screen.queryByRole("region", { name: "Unsaved changes" })).not.toBeInTheDocument();
    await userEvent.click(screen.getByRole("button", { name: "Edit them in expert mode" }));
    expect(onExpertMode).toHaveBeenCalled();
  });

  it("lists and writes the changes for two-way TLS", async () => {
    const writes = render([nrpe("certificate", "server.pem")]);
    await waitFor(() => expect(radio(/^One-way TLS/)).toBeChecked());

    await userEvent.click(radio(/^Two-way TLS/));
    expect(screen.queryByText("Clients are not authenticated")).not.toBeInTheDocument();
    const pending = within(screen.getByRole("list", { name: "Pending changes" }));
    expect(pending.getByText(`[${NRPE_PATH}] verify mode = peer-cert`)).toBeInTheDocument();
    expect(pending.getByText("[/modules] NRPEServer = enabled")).toBeInTheDocument();
    // The CA is the agent's default, so it is not written.
    expect(pending.queryByText(/\] ca = /)).not.toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "Save" }));
    await waitFor(() =>
      expect(writes).toEqual([
        `PUT [${NRPE_PATH}] verify mode=peer-cert`,
        "PUT [/modules] NRPEServer=enabled",
      ]),
    );
  });

  it("will not save two-way TLS without a CA", async () => {
    render([]);
    await waitFor(() => expect(radio(/^One-way TLS/)).toBeChecked());
    await userEvent.click(radio(/^Two-way TLS/));
    await userEvent.clear(screen.getByLabelText("Client CA"));
    expect(screen.getByText(/needs the CA/)).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Save" })).toBeDisabled();
  });

  it("explains the safe way to vary checks, and flags arguments to scripts", async () => {
    render([]);
    await waitFor(() => expect(radio(/^No arguments/)).toBeChecked());
    expect(screen.getByRole("link", { name: "Open Check helpers" })).toHaveAttribute(
      "href",
      "/modules/CheckHelpers",
    );
    await userEvent.click(radio(/^Arguments for built-in checks and external scripts/));
    expect(screen.getByText("Requests can pass arguments to your scripts")).toBeInTheDocument();
    const pending = within(screen.getByRole("list", { name: "Pending changes" }));
    expect(pending.getByText(`[${NRPE_PATH}] allow arguments = true`)).toBeInTheDocument();
    expect(
      pending.getByText("[/settings/external scripts] allow arguments = true"),
    ).toBeInTheDocument();
  });

  it("keeps a shared value in the shared section, and pins it locally on request", async () => {
    const writes = render([{ path: "/settings/default", key: "timeout", value: "60" }]);
    await waitFor(() => expect(radio(/^One-way TLS/)).toBeChecked());
    const timeout = screen.getByLabelText("Timeout (seconds)");
    expect(timeout).toHaveValue("60");
    const scope = screen.getByRole("group", { name: "Where Timeout (seconds) is kept" });
    expect(within(scope).getByRole("button", { name: "Default" })).toHaveAttribute(
      "aria-pressed",
      "true",
    );

    // Edited in the default scope: the shared section changes.
    await userEvent.clear(timeout);
    await userEvent.type(timeout, "15");
    const pending = () => within(screen.getByRole("list", { name: "Pending changes" }));
    expect(pending().getByText("[/settings/default] timeout = 15")).toBeInTheDocument();

    // Switched to local: NRPE gets its own value and the shared one is left alone.
    await userEvent.click(within(scope).getByRole("button", { name: "Local" }));
    await userEvent.clear(timeout);
    await userEvent.type(timeout, "20");
    expect(pending().getByText(`[${NRPE_PATH}] timeout = 20`)).toBeInTheDocument();
    expect(pending().queryByText(/\[\/settings\/default\] timeout/)).not.toBeInTheDocument();

    await userEvent.click(screen.getByText("Advanced"));
    await userEvent.click(screen.getByRole("switch", { name: "Performance data" }));
    await userEvent.click(screen.getByRole("button", { name: "Save" }));
    await waitFor(() =>
      expect(writes).toEqual([
        `PUT [${NRPE_PATH}] timeout=20`,
        `PUT [${NRPE_PATH}] performance data=false`,
        "PUT [/modules] NRPEServer=enabled",
      ]),
    );
  });

  it("keeps bind to and the host cache under Advanced", async () => {
    render([]);
    await waitFor(() => expect(radio(/^One-way TLS/)).toBeChecked());
    expect(screen.getByLabelText("Bind to address")).not.toBeVisible();
    await userEvent.click(screen.getByText("Advanced"));
    await waitFor(() => expect(screen.getByLabelText("Bind to address")).toBeVisible());
    expect(screen.getByText("Cache allowed hosts")).toBeVisible();
    // And not in Connection.
    const connection = screen.getByText("Connection").parentElement as HTMLElement;
    expect(within(connection).queryByLabelText("Bind to address")).not.toBeInTheDocument();
  });

  it("offers the client identity only with two-way TLS", async () => {
    render([]);
    await waitFor(() => expect(radio(/^One-way TLS/)).toBeChecked());
    expect(screen.queryByLabelText("Client identity")).not.toBeInTheDocument();
    await userEvent.click(radio(/^Two-way TLS/));
    expect(screen.getByLabelText("Client identity")).toBeInTheDocument();
  });

  it("shows the configuration as an INI fragment, unsaved changes included", async () => {
    render([nrpe("certificate", "server.pem")]);
    await waitFor(() => expect(radio(/^One-way TLS/)).toBeChecked());
    await userEvent.click(radio(/^Two-way TLS/));
    await userEvent.click(screen.getByRole("button", { name: "Show config" }));
    const ini = screen.getByLabelText("INI fragment");
    expect(ini).toHaveTextContent("[/modules] NRPEServer = enabled");
    expect(ini).toHaveTextContent("verify mode = peer-cert");
    expect(ini).toHaveTextContent("certificate = server.pem");
    expect(ini).toHaveTextContent("port = 5666");
    expect(screen.getByText(/Includes the changes not saved yet/)).toBeInTheDocument();
    await userEvent.click(
      screen.getByRole("switch", { name: "Include values left at their default" }),
    );
    expect(screen.getByLabelText("INI fragment")).not.toHaveTextContent("port = 5666");
  });

  it("will not save a client identity from the certificate without two-way TLS", async () => {
    render([nrpe("client identity source", "cn")]);
    await waitFor(() => expect(radio(/^One-way TLS/)).toBeChecked());
    await userEvent.type(screen.getByLabelText("Port"), "7");
    expect(screen.getByText(/needs two-way TLS; the agent refuses/)).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Save" })).toBeDisabled();
  });
});
