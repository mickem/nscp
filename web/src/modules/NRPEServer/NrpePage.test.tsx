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
    expect(screen.getByText("No changes.")).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Save" })).toBeDisabled();
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
    expect(screen.getByText("No changes.")).toBeInTheDocument();
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
    expect(screen.getByRole("link", { name: "Open External scripts" })).toHaveAttribute(
      "href",
      "/modules/CheckExternalScripts",
    );
    await userEvent.click(radio(/^Arguments for built-in checks and external scripts/));
    expect(screen.getByText("Requests can pass arguments to your scripts")).toBeInTheDocument();
    const pending = within(screen.getByRole("list", { name: "Pending changes" }));
    expect(pending.getByText(`[${NRPE_PATH}] allow arguments = true`)).toBeInTheDocument();
    expect(
      pending.getByText("[/settings/external scripts] allow arguments = true"),
    ).toBeInTheDocument();
  });
});
