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
import RolesEditor from "./WEBServer/RolesEditor";
import ModuleSettings from "../components/ModuleSettings";
import { SettingsDescription } from "../api/api";

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

  it("reads a fresh install as aes256 with no TLS, and needs the key before saving", async () => {
    render(NscaPage, []);
    await waitFor(() => expect(screen.getByLabelText("Encryption")).toHaveTextContent("aes256"));
    // NSCA has its own encryption: no TLS on the page.
    expect(screen.queryByRole("radio", { name: /TLS/ })).not.toBeInTheDocument();
    expect(screen.queryByLabelText("Server certificate")).not.toBeInTheDocument();
    expect(screen.queryByText("TLS is on for this server")).not.toBeInTheDocument();
    // Nothing to save yet, but the missing key is already said.
    expect(screen.getByText(/NSCA encryption needs a password/)).toBeInTheDocument();
  });

  it("writes the key and lets the save through", async () => {
    const writes = render(NscaPage, []);
    await screen.findByLabelText("Password");
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

  it("offers only the secure ciphers, keeping another stored cipher on show", async () => {
    const writes = render(NscaPage, [
      { path: P, key: "encryption", value: "blowfish" },
      { path: P, key: "password", value: "k" },
    ]);
    const field = await screen.findByLabelText("Encryption");
    expect(field).toHaveTextContent("blowfish");
    expect(screen.getByText(/offers only the secure ciphers/)).toBeInTheDocument();
    await userEvent.click(field);
    const options = screen.getAllByRole("option").map((o) => o.textContent);
    expect(options).toEqual(["aes256", "aes192", "aes128", "twofish", "serpent", "blowfish"]);
    await userEvent.click(screen.getByRole("option", { name: "twofish" }));
    await save();
    await waitFor(() =>
      expect(writes).toEqual([`[${P}] encryption=twofish`, "[/modules] NSCAServer=enabled"]),
    );
  });

  it("points to expert mode when TLS was turned on there", async () => {
    render(NscaPage, [{ path: P, key: "use ssl", value: "true" }]);
    expect(await screen.findByText("TLS is on for this server")).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Open expert mode" })).toBeInTheDocument();
  });
});

describe("NSClient server page", () => {
  const P = "/settings/NSClient/server";

  it("offers no TLS, and says check_nt cannot reach the agent's default TLS", async () => {
    render(NsclientPage, []);
    expect(await screen.findByText("check_nt is a legacy protocol")).toBeInTheDocument();
    expect(screen.getByText("TLS is not supported by check_nt")).toBeInTheDocument();
    expect(screen.getByText(/check_nt clients cannot connect to/)).toBeInTheDocument();
    expect(screen.queryByRole("radio", { name: /TLS/ })).not.toBeInTheDocument();
    expect(screen.queryByLabelText("Server certificate")).not.toBeInTheDocument();
    expect(screen.getByText("No password: every request is refused")).toBeInTheDocument();
  });

  it("says the connection is in clear once the agent's TLS is off", async () => {
    render(NsclientPage, [{ path: P, key: "use ssl", value: "false" }]);
    expect(await screen.findByText(/Connections are plain TCP/)).toBeInTheDocument();
    expect(screen.queryByRole("button", { name: "Open expert mode" })).not.toBeInTheDocument();
  });

  it("picks allowed commands and writes them in the shortest spelling", async () => {
    const writes = render(NsclientPage, [
      { path: "/settings/default", key: "password", value: "pw" },
    ]);
    const all = await screen.findByRole("switch", { name: /All commands/ });
    expect(all).toBeChecked();
    // Everything allowed includes the arbitrary reads.
    expect(screen.getByText("check_nt can read more than metrics")).toBeInTheDocument();

    await userEvent.click(all);
    expect(screen.getByText("No command is allowed")).toBeInTheDocument();
    await userEvent.click(screen.getByRole("checkbox", { name: /System metrics/ }));
    await userEvent.click(screen.getByRole("checkbox", { name: /Agent version/ }));
    await userEvent.click(screen.getByRole("checkbox", { name: "counter" }));
    expect(screen.getByText(/any performance counter, by name/)).toBeInTheDocument();
    await userEvent.click(screen.getByRole("checkbox", { name: "counter" }));
    expect(screen.queryByText("check_nt can read more than metrics")).not.toBeInTheDocument();

    await save();
    await waitFor(() =>
      expect(writes).toEqual([`[${P}] allow=metrics, info`, "[/modules] NSClientServer=enabled"]),
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

describe("web server advanced options", () => {
  it("keeps worker threads and the login limiter under Advanced", async () => {
    render(WebPage, []);
    expect(await screen.findByLabelText("Worker threads")).not.toBeVisible();
    expect(screen.getByLabelText("Failed logins before blocking")).not.toBeVisible();
    expect(screen.getByLabelText("Block for (seconds)")).not.toBeVisible();
    await userEvent.click(screen.getByText("Advanced"));
    await waitFor(() => expect(screen.getByLabelText("Worker threads")).toBeVisible());
    expect(screen.getByLabelText("Failed logins before blocking")).toBeVisible();
    expect(screen.getByLabelText("Block for (seconds)")).toBeVisible();
  });

  it("still warns where logins are set up when the limiter is off", async () => {
    render(WebPage, [
      { path: "/settings/WEB/server", key: "auth rate limit max failures", value: "0" },
    ]);
    expect(await screen.findByText("Failed logins are not limited")).toBeVisible();
    expect(screen.getByText(/The limit is under Advanced/)).toBeVisible();
  });
});

describe("web server passive results", () => {
  it("shows the cache's options only while it is on", async () => {
    render(WebPage, []);
    await screen.findByRole("switch", { name: "Keep passive results for polling" });
    expect(screen.queryByLabelText("Channel")).not.toBeInTheDocument();
    await userEvent.click(screen.getByRole("switch", { name: "Keep passive results for polling" }));
    expect(screen.getByLabelText("Channel")).toBeInTheDocument();
    expect(screen.getByRole("switch", { name: "Drop results once polled" })).toBeInTheDocument();
  });
});

describe("web server roles", () => {
  const R = "/settings/WEB/server/roles";
  const stored: Settings[] = [
    { path: R, key: "full", value: "*" },
    { path: R, key: "metrics", value: "public,metrics.list,openmetrics.list,login.get" },
    { path: "/settings/WEB/server/users/admin", key: "role", value: "full" },
  ];
  const renderRoles = (s: Settings[]) => {
    const writes: string[] = [];
    installFetchMock({
      "/api/v2/settings": async (req: Request) => {
        if (req.method === "PUT") {
          const body = (await req.json()) as Settings;
          writes.push(`[${body.path}] ${body.key}=${body.value}`);
          return jsonResponse("");
        }
        return jsonResponse(s);
      },
    });
    renderWithProviders(<RolesEditor collectionPath={R} descriptions={[]} />, {
      preloadedState: authenticatedState,
    });
    return writes;
  };

  it("lists roles with what they grant and who holds them", async () => {
    renderRoles(stored);
    const table = within(await screen.findByRole("table", { name: "Roles" }));
    expect(table.getByText("Everything")).toBeInTheDocument();
    expect(table.getByText("admin")).toBeInTheDocument();
    expect(table.getByText("powerful grants")).toBeInTheDocument();
    expect(
      table.getByText(/public, metrics.list, openmetrics.list and 1 more/),
    ).toBeInTheDocument();
  });

  it("edits a role's grants with the picker", async () => {
    const writes = renderRoles(stored);
    const row = (await screen.findByText("metrics")).closest("tr") as HTMLElement;
    await userEvent.click(within(row).getByRole("button", { name: "Edit" }));
    const dialog = within(screen.getByRole("dialog"));
    expect(dialog.getByRole("checkbox", { name: "Metrics" })).toBeChecked();
    await userEvent.click(dialog.getByRole("checkbox", { name: "Change settings" }));
    expect(dialog.getByText("This role holds powerful grants")).toBeInTheDocument();
    await userEvent.click(dialog.getByRole("button", { name: "Save" }));
    await waitFor(() =>
      expect(writes).toEqual([
        `[${R}] metrics=public,metrics.list,openmetrics.list,login.get,settings.put`,
      ]),
    );
  });

  it("covers a whole area with its wildcard, and nothing outside it", async () => {
    const writes = renderRoles([]);
    await userEvent.click(await screen.findByRole("button", { name: "Add role" }));
    await userEvent.click(screen.getByText("Empty role"));
    const dialog = within(screen.getByRole("dialog"));
    await userEvent.type(dialog.getByLabelText("Name"), "checker");
    await userEvent.click(dialog.getByRole("checkbox", { name: "List aliases" }));
    await userEvent.click(dialog.getByRole("checkbox", { name: /queries\.\*/ }));
    expect(dialog.getByRole("checkbox", { name: "List checks" })).toBeDisabled();
    expect(dialog.getByRole("checkbox", { name: "List aliases" })).toBeEnabled();
    await userEvent.click(dialog.getByRole("button", { name: "Save" }));
    await waitFor(() =>
      expect(writes).toEqual([`[${R}] checker=public,login.get,aliases.list,queries.*`]),
    );
  });

  it("starts a role from a built-in one", async () => {
    const writes = renderRoles(stored);
    await userEvent.click(await screen.findByRole("button", { name: "Add role" }));
    await userEvent.click(screen.getByText("Like metrics"));
    const dialog = within(screen.getByRole("dialog"));
    // The name is taken, so it gets a suffix.
    expect(dialog.getByLabelText("Name")).toHaveValue("metrics_2");
    await userEvent.click(dialog.getByRole("button", { name: "Save" }));
    await waitFor(() =>
      expect(writes).toEqual([`[${R}] metrics_2=public,metrics.list,openmetrics.list,login.get`]),
    );
  });

  it("warns before deleting a role someone holds", async () => {
    renderRoles(stored);
    const row = (await screen.findByText("full")).closest("tr") as HTMLElement;
    await userEvent.click(within(row).getByRole("button", { name: "Delete" }));
    expect(screen.getByText(/admin hold this role/)).toBeInTheDocument();
  });
});

describe("web server users", () => {
  const U = "/settings/WEB/server/users";
  const d = (
    path: string,
    key: string,
    over: Partial<SettingsDescription> = {},
  ): SettingsDescription => ({
    path,
    key,
    type: "string",
    title: key.toUpperCase(),
    description: "",
    value: "",
    default_value: "",
    icon: "",
    is_advanced_key: false,
    is_object: key === "",
    is_sample_key: false,
    is_template_key: false,
    plugins: ["WEBServer"],
    sample_usage: "",
    ...over,
  });

  it("has no templates: no Add template, no default row", async () => {
    installFetchMock({
      "/api/v2/settings/descriptions": jsonResponse([
        d(U, "", { title: "Web server users" }),
        d(`${U}/default`, "is template", { default_value: "false" }),
        d(`${U}/default`, "parent", { default_value: "default" }),
        d(`${U}/default`, "password"),
        d(`${U}/default`, "role"),
      ]),
      "/api/v2/settings": jsonResponse([{ path: `${U}/admin`, key: "role", value: "full" }]),
    });
    renderWithProviders(<ModuleSettings settings={[]} moduleId="WEBServer" />, {
      preloadedState: authenticatedState,
    });
    const card = (await screen.findByText("Web server users")).closest(
      ".MuiCard-root",
    ) as HTMLElement;
    expect(await within(card).findByText("admin")).toBeInTheDocument();
    expect(within(card).queryByText("default")).not.toBeInTheDocument();
    expect(within(card).queryByRole("button", { name: "Add template" })).not.toBeInTheDocument();
  });
});
