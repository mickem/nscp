// The small check and helper pages against a fake settings API.
import { describe, expect, it } from "vitest";
import { ComponentType } from "react";
import { screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { Settings } from "../api/api";
import { authenticatedState, installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";
import DiskPage from "./CheckDisk/DiskPage";
import DiskFiles from "./CheckDisk/DiskFiles";
import { accessRisk, disabledCollectors, disableValue, fileAccessOf } from "./CheckDisk/disk";
import { getModulePage } from "./registry";

function render(Page: ComponentType, stored: Settings[]) {
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
  renderWithProviders(<Page />, { preloadedState: authenticatedState });
  return writes;
}
const save = () => userEvent.click(screen.getByRole("button", { name: "Save" }));
const P = "/settings/disk";

describe("CheckDisk", () => {
  it("warns that any path may be named, and asks for paths once restricted", async () => {
    const writes = render(DiskPage, []);
    expect(await screen.findByText("Callers can describe any path")).toBeInTheDocument();
    expect(screen.queryByLabelText("Allowed paths")).not.toBeInTheDocument();

    await userEvent.click(screen.getByRole("radio", { name: /Paths under the ones listed/ }));
    expect(screen.getByText("No path is allowed")).toBeInTheDocument();
    await userEvent.type(screen.getByLabelText("Allowed paths"), "/var/log");
    expect(screen.queryByText("No path is allowed")).not.toBeInTheDocument();
    await save();
    await waitFor(() =>
      expect(writes).toEqual([
        `[${P}] file access=allowed`,
        `[${P}] allowed files=/var/log`,
        "[/modules] CheckDisk=enabled",
      ]),
    );
  });

  it("reads the agent's other spellings, and flags one it does not read", () => {
    expect(fileAccessOf("Named")).toBe("predefined");
    expect(fileAccessOf("list")).toBe("allowed");
    expect(fileAccessOf("")).toBe("any");
    expect(fileAccessOf("nope")).toBeNull();
    expect(accessRisk(null, "")?.title).toMatch(/not one the agent reads/);
    expect(accessRisk("predefined", "")).toBeNull();
  });

  it("asks for the growth-spots interval only while they are measured", async () => {
    const writes = render(DiskPage, []);
    const growth = await screen.findByRole("switch", { name: "Measure the growth spots" });
    expect(screen.queryByLabelText("Measured every")).not.toBeInTheDocument();
    await userEvent.click(growth);
    expect(screen.getByLabelText("Measured every")).toBeInTheDocument();
    await userEvent.click(screen.getByRole("switch", { name: "Report the volumes" }));
    await save();
    await waitFor(() =>
      expect(writes).toEqual([
        `[${P}/diagnostics] growth spots=true`,
        `[${P}/facts] storage.volumes=true`,
        "[/modules] CheckDisk=enabled",
      ]),
    );
  });

  it("switches collectors off as the list the agent reads", async () => {
    const writes = render(DiskPage, [{ path: P, key: "disable", value: "trend" }]);
    await userEvent.click(await screen.findByText("Advanced"));
    const trend = screen.getByRole("checkbox", { name: /Used-space trend/ });
    expect(trend).toBeChecked();
    await userEvent.click(screen.getByRole("checkbox", { name: /Disk I\/O/ }));
    await save();
    await waitFor(() => expect(writes).toEqual([`[${P}] disable=disk_io,trend`, "[/modules] CheckDisk=enabled"]));
    expect(disabledCollectors("disk_free, disk_io")).toEqual(new Set(["disk_io", "disk_free"]));
    expect(disableValue(new Set())).toBe("");
  });

  it("lists the named paths and runs one by name", async () => {
    let file: string | null = null;
    installFetchMock({
      "/api/v2/settings": jsonResponse([{ path: `${P}/files`, key: "applog", value: "/var/log/app.log" }]),
      "/api/v2/queries/check_single_file/commands/execute": async (req: Request) => {
        file = new URL(req.url, "http://localhost").searchParams.get("file");
        return jsonResponse({
          command: "check_single_file",
          result: 0,
          lines: [{ message: "app.log: 12 KB", perf: {} }],
        });
      },
    });
    renderWithProviders(<DiskFiles />, { preloadedState: authenticatedState });
    const table = await screen.findByRole("table", { name: "Named paths" });
    expect(within(table).getByText("/var/log/app.log")).toBeInTheDocument();
    await userEvent.click(within(table).getByRole("button", { name: "Run" }));
    expect(await screen.findByText(/OK: app\.log: 12 KB/)).toBeInTheDocument();
    expect(file).toBe("applog");
  });
});

describe("small pages", () => {
  const panel = (id: string) => {
    const Panel = getModulePage(id)?.Panel;
    if (!Panel) throw new Error(`${id} has no page`);
    return Panel as ComponentType;
  };

  it.each([
    ["CheckHyperV", "Report the virtual machines", "[/settings/hyperv/facts] hyperv.vms=true"],
    ["CheckTaskSched", "Report the scheduled tasks", "[/settings/task schedule/facts] tasks.scheduled=true"],
  ])("%s switches its fact set on", async (id, label, write) => {
    const writes = render(panel(id), []);
    await userEvent.click(await screen.findByRole("switch", { name: label }));
    await save();
    await waitFor(() => expect(writes).toEqual([write, `[/modules] ${id}=enabled`]));
  });

  it("shows the file writer's defaults and writes a changed file", async () => {
    const writes = render(panel("SimpleFileWriter"), []);
    expect(await screen.findByLabelText("Channel")).toHaveValue("FILE");
    expect(screen.getByLabelText("Line")).toHaveValue("${alias-or-command} ${result} ${message}");
    const file = screen.getByLabelText("File");
    await userEvent.clear(file);
    await userEvent.type(file, "results.log");
    await save();
    await waitFor(() =>
      expect(writes).toEqual(["[/settings/writers/file] file=results.log", "[/modules] SimpleFileWriter=enabled"]),
    );
  });

  it("shows the cache's channel and key", async () => {
    render(panel("SimpleCache"), []);
    expect(await screen.findByLabelText("Channel")).toHaveValue("CACHE");
    expect(screen.getByLabelText("Cache key")).toHaveValue("${alias-or-command}");
  });

  it("turns the prompt's history off", async () => {
    const writes = render(panel("CommandClient"), []);
    const size = await screen.findByLabelText("Commands kept in the history");
    await userEvent.clear(size);
    await userEvent.type(size, "0");
    await save();
    await waitFor(() => expect(writes).toEqual(["[/settings/cli] history size=0", "[/modules] CommandClient=enabled"]));
  });
});
