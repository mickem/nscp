// The scripts and scheduling pages against a fake API.
import { describe, expect, it } from "vitest";
import { screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { Settings } from "../api/api";
import {
  authenticatedState,
  installFetchMock,
  jsonResponse,
  renderWithProviders,
} from "../test/test-utils";
import CheckHelpersAliases from "./CheckHelpers/CheckHelpersAliases";
import { CHECK_HELPERS_ALIAS_PATH as A } from "./CheckHelpers/paths";
import ExtScriptsPage from "./CheckExternalScripts/ExtScriptsPage";
import ScriptsEditor from "./CheckExternalScripts/ScriptsEditor";
import {
  argumentsRisk,
  EXT_SCRIPTS,
  readScripts,
  Script,
  scriptWrites,
} from "./CheckExternalScripts/ext";

function api(stored: Settings[]) {
  const writes: string[] = [];
  const runs: string[] = [];
  installFetchMock({
    "/api/v2/settings": async (req: Request) => {
      if (req.method === "PUT") {
        const body = (await req.json()) as Settings;
        writes.push(`[${body.path}] ${body.key}=${body.value}`);
        return jsonResponse("");
      }
      return jsonResponse(stored);
    },
    [encodeURI(`/api/v2/settings${A}`)]: async (req: Request) => {
      if (req.method === "DELETE") {
        writes.push(
          `delete [${A}] ${new URL(req.url, "http://localhost").searchParams.get("key")}`,
        );
        return jsonResponse("");
      }
      return jsonResponse(stored);
    },
    "/api/v2/queries": jsonResponse([
      {
        name: "check_cpu",
        title: "",
        plugin: "CheckSystem",
        description: "Check CPU load",
        experimental: false,
        query_url: "",
      },
      {
        name: "check_ok",
        title: "",
        plugin: "CheckHelpers",
        description: "Just return OK",
        experimental: false,
        query_url: "",
      },
    ]),
    "/api/v2/aliases": jsonResponse([]),
    "/api/v2/queries/check_cpu/help": jsonResponse({
      name: "check_cpu",
      keyword_source: "check_cpu",
      parameters: [
        {
          name: "time",
          default_value: "",
          required: false,
          repeatable: true,
          content_type: "",
          short_description: "The time to check",
          long_description: "",
        },
      ],
      fields: [],
    }),
    "/api/v2/queries/check_cpu/commands/execute": async (req: Request) => {
      runs.push(new URL(req.url, "http://localhost").search);
      return jsonResponse({
        command: "check_cpu",
        result: 1,
        lines: [{ message: "CPU load is high", perf: {} }],
      });
    },
  });
  return { writes, runs };
}

const dialog = () => within(screen.getByRole("dialog"));

describe("check helpers aliases", () => {
  const render = (stored: Settings[]) => {
    const r = api(stored);
    renderWithProviders(<CheckHelpersAliases collectionPath={A} descriptions={[]} />, {
      preloadedState: authenticatedState,
    });
    return r;
  };

  it("lists each alias with what it runs", async () => {
    render([
      { path: A, key: "cpu", value: 'check_cpu "warn=load > 80%"' },
      { path: A, key: "files", value: 'check_files "path=$ARG1$"' },
    ]);
    const table = within(await screen.findByRole("table", { name: "Aliases" }));
    expect(table.getByText('check_cpu "warn=load > 80%"')).toBeInTheDocument();
    expect(table.getAllByText("takes arguments")).toHaveLength(1);
  });

  it("builds an alias with the check picker and an option chip, and runs it", async () => {
    const { writes, runs } = render([]);
    await userEvent.click(await screen.findByRole("button", { name: "Add alias" }));
    await userEvent.click(screen.getByText("Empty alias"));
    await userEvent.type(dialog().getByLabelText("Name"), "cpu_5m");
    await userEvent.type(dialog().getByLabelText("Command"), "check_cpu");
    await userEvent.click(await dialog().findByText("time"));
    await userEvent.type(dialog().getByLabelText("Arguments"), "5m");
    await userEvent.click(dialog().getByRole("button", { name: "Run now" }));
    expect(await dialog().findByText("WARNING: CPU load is high")).toBeInTheDocument();
    expect(runs).toEqual(["?time=5m"]);
    await userEvent.click(dialog().getByRole("button", { name: "Save" }));
    await waitFor(() => expect(writes).toEqual([`[${A}] cpu_5m=check_cpu time=5m`]));
  });

  it("starts from a preset, and cannot run one that takes arguments", async () => {
    const { writes } = render([]);
    await userEvent.click(await screen.findByRole("button", { name: "Add alias" }));
    await userEvent.click(screen.getByText("alias_process_stopped"));
    expect(dialog().getByLabelText("Name")).toHaveValue("alias_process_stopped");
    expect(dialog().getByRole("button", { name: "Run now" })).toBeDisabled();
    expect(dialog().getByText(/Takes \$ARG1\$ from its caller/)).toBeInTheDocument();
    await userEvent.click(dialog().getByRole("button", { name: "Save" }));
    await waitFor(() => expect(writes[0]).toMatch(/^\[.*\] alias_process_stopped=check_process /));
  });

  it("renames by writing the new name and dropping the old", async () => {
    const { writes } = render([{ path: A, key: "cpu", value: "check_cpu" }]);
    const row = (await screen.findByText("cpu", { selector: "td" })).closest("tr") as HTMLElement;
    await userEvent.click(within(row).getByRole("button", { name: "Edit" }));
    await userEvent.clear(dialog().getByLabelText("Name"));
    await userEvent.type(dialog().getByLabelText("Name"), "cpu_load");
    await userEvent.click(dialog().getByRole("button", { name: "Save" }));
    await waitFor(() => expect(writes).toEqual([`[${A}] cpu_load=check_cpu`, `delete [${A}] cpu`]));
  });
});

describe("external scripts", () => {
  const S = EXT_SCRIPTS;

  it("writes a script with only a command as one line, and one with options as a section", () => {
    expect(scriptWrites(null, "foo", { command: "scripts\\foo.bat" })).toEqual([
      { kind: "put", path: S, key: "foo", value: "scripts\\foo.bat" },
    ]);
    const line: Script = { name: "foo", form: "line", values: { command: "x" } };
    expect(scriptWrites(line, "foo", { command: "x", user: "svc" })).toEqual([
      { kind: "delete", path: S, key: "foo" },
      { kind: "put", path: `${S}/foo`, key: "command", value: "x" },
      { kind: "put", path: `${S}/foo`, key: "user", value: "svc" },
    ]);
    const section: Script = { name: "foo", form: "section", values: { command: "x", user: "svc" } };
    expect(scriptWrites(section, "foo", { command: "y", user: "" })).toEqual([
      { kind: "put", path: `${S}/foo`, key: "command", value: "y" },
      { kind: "delete", path: `${S}/foo`, key: "user" },
    ]);
    expect(scriptWrites(section, "bar", { command: "x", user: "svc" })[0]).toEqual({
      kind: "delete-path",
      path: `${S}/foo`,
    });
  });

  it("reads both forms", () => {
    expect(
      readScripts([
        { path: S, key: "a", value: "a.bat" },
        { path: `${S}/b`, key: "command", value: "b.ps1" },
        { path: `${S}/b`, key: "user", value: "svc" },
        { path: `${S}/default`, key: "user", value: "x" },
      ]),
    ).toEqual([
      { name: "a", form: "line", values: { command: "a.bat" } },
      { name: "b", form: "section", values: { command: "b.ps1", user: "svc" } },
    ]);
  });

  it("warns once arguments are allowed, and more with shell characters", () => {
    expect(argumentsRisk("false", "true")).toBeNull();
    expect(argumentsRisk("true", "false")?.severity).toBe("warning");
    expect(argumentsRisk("true", "true")?.severity).toBe("error");
  });

  it("offers shell characters only once arguments are allowed", async () => {
    api([]);
    renderWithProviders(<ExtScriptsPage />, { preloadedState: authenticatedState });
    const allow = await screen.findByRole("switch", {
      name: "Let callers pass arguments to scripts",
    });
    expect(
      screen.queryByRole("switch", { name: "Allow shell characters in arguments" }),
    ).toBeNull();
    await userEvent.click(allow);
    expect(screen.getByText("Callers can pass arguments to scripts")).toBeInTheDocument();
    expect(
      screen.getByRole("switch", { name: "Allow shell characters in arguments" }),
    ).toBeInTheDocument();
  });

  it("adds a script as one line", async () => {
    const { writes } = api([]);
    renderWithProviders(<ScriptsEditor />, { preloadedState: authenticatedState });
    await userEvent.click(await screen.findByRole("button", { name: "Add script" }));
    await userEvent.type(dialog().getByLabelText("Name"), "check_foo");
    await userEvent.type(dialog().getByLabelText("Command line"), "scripts/foo.sh");
    await userEvent.click(dialog().getByRole("button", { name: "Save" }));
    await waitFor(() => expect(writes).toEqual([`[${S}] check_foo=scripts/foo.sh`]));
  });
});
