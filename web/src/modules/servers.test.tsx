// The server pages against a fake settings API: what each one reads, warns
// about and refuses, and exactly what Save writes.
import { describe, expect, it } from "vitest";
import { ComponentType } from "react";
import { screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { Settings } from "../api/api";
import {
  authenticatedState,
  installFetchMock,
  jsonResponse,
  renderWithProviders,
} from "../test/test-utils";
import type { ModulePanelProps } from "./types";
import NscaPage from "./NSCAServer/NscaPage";
import NsclientPage from "./NSClientServer/NsclientPage";
import CheckmkPage from "./CheckMKServer/CheckmkPage";
import NcpaPage from "./NCPAServer/NcpaPage";
import WebPage from "./WEBServer/WebPage";
import IcingaPage from "./IcingaAgent/IcingaPage";

function render(Page: ComponentType<ModulePanelProps>, stored: Settings[]) {
  const writes: string[] = [];
  installFetchMock({
    "/api/v2/settings": async (req: Request) => {
      if (req.method === "PUT") {
        const body = (await req.json()) as Settings;
        writes.push(`[${body.path}] ${body.key}=${body.value}`);
        return jsonResponse("");
      }
      return jsonResponse(stored);
    },
  });
  renderWithProviders(<Page onExpertMode={() => {}} />, { preloadedState: authenticatedState });
  return writes;
}

const radio = (name: RegExp) => screen.getByRole("radio", { name });
const save = () => userEvent.click(screen.getByRole("button", { name: "Save" }));
const pending = () => within(screen.getByRole("list", { name: "Pending changes" }));

describe("NSCA server page", () => {
  const P = "/settings/NSCA/server";

  it("reads a fresh install as plain TCP with aes256, and needs the key before saving", async () => {
    render(NscaPage, []);
    await waitFor(() => expect(radio(/^No TLS/)).toBeChecked());
    expect(screen.getByLabelText("Encryption")).toHaveTextContent("aes256");
    // Nothing to save yet, but the missing key is already said.
    expect(screen.getByText(/NSCA encryption needs a password/)).toBeInTheDocument();
  });

  it("writes the key and lets the save through", async () => {
    const writes = render(NscaPage, []);
    await waitFor(() => expect(radio(/^No TLS/)).toBeChecked());
    await userEvent.type(screen.getByLabelText("Password"), "s3cret");
    expect(screen.getByText(/Will be replaced when saved/)).toBeInTheDocument();
    await save();
    await waitFor(() =>
      expect(writes).toEqual([`[${P}] password=s3cret`, "[/modules] NSCAServer=enabled"]),
    );
  });

  it("says plainly when results are accepted unencrypted", async () => {
    render(NscaPage, [{ path: P, key: "encryption", value: "none" }]);
    expect(await screen.findByText("Results are accepted unencrypted")).toBeInTheDocument();
  });

  it("calls out xor and old ciphers", async () => {
    render(NscaPage, [
      { path: P, key: "encryption", value: "xor" },
      { path: P, key: "password", value: "k" },
    ]);
    expect(await screen.findByText("xor is not encryption")).toBeInTheDocument();
  });
});

describe("NSClient server page", () => {
  const P = "/settings/NSClient/server";

  it("reads a fresh install as TLS with no password, and says what that means", async () => {
    render(NsclientPage, []);
    await waitFor(() => expect(radio(/^One-way TLS/)).toBeChecked());
    expect(screen.getByText("check_nt is a legacy protocol")).toBeInTheDocument();
    expect(screen.getByText("No password: every request is refused")).toBeInTheDocument();
  });

  it("flags plain TCP and writes it", async () => {
    const writes = render(NsclientPage, [
      { path: "/settings/default", key: "password", value: "pw" },
    ]);
    await waitFor(() => expect(radio(/^One-way TLS/)).toBeChecked());
    expect(screen.queryByText(/No password/)).not.toBeInTheDocument();
    await userEvent.click(radio(/^No TLS/));
    expect(screen.getByText("The password travels in clear")).toBeInTheDocument();
    await save();
    await waitFor(() =>
      expect(writes).toEqual([`[${P}] use ssl=false`, "[/modules] NSClientServer=enabled"]),
    );
  });
});

describe("check_mk server page", () => {
  const P = "/settings/check_mk/server";

  it("writes two-way TLS with its files", async () => {
    const writes = render(CheckmkPage, []);
    await waitFor(() => expect(radio(/^No TLS/)).toBeChecked());
    expect(screen.getByText("The agent output travels in clear")).toBeInTheDocument();
    await userEvent.click(radio(/^Two-way TLS/));
    expect(pending().getByText(`[${P}] verify mode = peer-cert`)).toBeInTheDocument();
    await save();
    await waitFor(() =>
      expect(writes).toEqual([
        `[${P}] use ssl=true`,
        `[${P}] verify mode=peer-cert`,
        "[/modules] CheckMKServer=enabled",
      ]),
    );
  });
});

describe("NCPA server page", () => {
  const P = "/settings/NCPA/server";

  it("warns that a missing token refuses everything", async () => {
    render(NcpaPage, []);
    expect(await screen.findByText("No token: every request is refused")).toBeInTheDocument();
  });

  it("calls out arguments to every exposed query, scripts included", async () => {
    const writes = render(NcpaPage, [{ path: P, key: "token", value: "t" }]);
    await userEvent.click(await screen.findByRole("switch", { name: "Allow arguments" }));
    expect(screen.getByText("Pollers can pass arguments")).toBeInTheDocument();
    expect(screen.getByText(/external scripts included, with arguments/)).toBeInTheDocument();
    await save();
    await waitFor(() =>
      expect(writes).toEqual([`[${P}] allow arguments=true`, "[/modules] NCPAServer=enabled"]),
    );
  });

  it("warns about serving in clear", async () => {
    render(NcpaPage, [{ path: P, key: "allow insecure", value: "true" }]);
    expect(
      await screen.findByText("A missing certificate means serving in clear"),
    ).toBeInTheDocument();
  });
});

describe("web server page", () => {
  it("warns that changing it can end the session, and about anonymous access", async () => {
    const writes = render(WebPage, []);
    expect(await screen.findByText(/This is the server you are using now/)).toBeInTheDocument();
    await userEvent.click(screen.getByRole("switch", { name: "Allow anonymous access" }));
    expect(screen.getByText("Requests without credentials are answered")).toBeInTheDocument();
    await userEvent.click(screen.getByRole("switch", { name: "Keep passive results for polling" }));
    await save();
    await waitFor(() =>
      expect(writes).toEqual([
        "[/settings/WEB/server] allow anonymous access=true",
        "[/settings/WEB/server/results] enabled=true",
        "[/modules] WEBServer=enabled",
      ]),
    );
  });

  it("keeps the admin password in the shared section unless made local", async () => {
    const writes = render(WebPage, [
      { path: "/settings/default", key: "password", value: "pbkdf2-sha256$x" },
    ]);
    expect(await screen.findByText(/Set \(stored hashed\)/)).toBeInTheDocument();
    await userEvent.type(screen.getByLabelText("Admin password"), "new");
    await save();
    await waitFor(() =>
      expect(writes).toEqual(["[/settings/default] password=new", "[/modules] WEBServer=enabled"]),
    );
  });
});

describe("Icinga agent page", () => {
  const P = "/settings/Icinga/agent";

  it("says the agent is off until a parent is set", async () => {
    render(IcingaPage, []);
    expect(await screen.findByText("The agent is off")).toBeInTheDocument();
  });

  it("needs the parent endpoint with a parent host, and warns about an unpinned first connection", async () => {
    const writes = render(IcingaPage, []);
    await userEvent.type(await screen.findByLabelText("Parent host"), "icinga.example");
    expect(
      screen.getByText(/parent endpoint \(the CN of the parent's certificate\) is required/),
    ).toBeInTheDocument();
    expect(screen.getByText("The first connection is not verified")).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Save" })).toBeDisabled();

    await userEvent.type(screen.getByLabelText("Parent endpoint"), "master1");
    await userEvent.type(screen.getByLabelText("Parent fingerprint"), "AB:CD");
    expect(screen.queryByText("The first connection is not verified")).not.toBeInTheDocument();
    await save();
    await waitFor(() =>
      expect(writes).toEqual([
        `[${P}] parent host=icinga.example`,
        `[${P}] parent endpoint=master1`,
        `[${P}] parent fingerprint=AB:CD`,
        "[/modules] IcingaAgent=enabled",
      ]),
    );
  });

  it("leaves the computed endpoint name out of the INI fragment", async () => {
    render(IcingaPage, []);
    await userEvent.click(await screen.findByRole("button", { name: "Show config" }));
    const ini = screen.getByLabelText("INI fragment");
    expect(ini).toHaveTextContent("parent port = 5665");
    expect(ini).not.toHaveTextContent("endpoint name =");
  });
});
