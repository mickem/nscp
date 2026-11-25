import { describe, expect, it } from "vitest";
import { screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import Diagnostics from "./Diagnostics";
import {
  authenticatedState,
  installFetchMock,
  jsonResponse,
  renderWithProviders,
} from "../test/test-utils";

const SUMMARY = {
  enabled: true,
  snapshot_id: 812,
  captured_at: "2026-11-15T10:42:07Z",
  max_rows: 10,
  dropped: 0,
  excluded: [] as string[],
  count: 3,
  sections: [
    {
      name: "processes.top_cpu",
      source: "CheckSystem",
      description: "The processes using the most CPU over the last second",
      captured_at: "2026-11-15T10:42:06Z",
      truncated: true,
      rows: 1,
    },
    {
      name: "failures.recent",
      source: "core",
      description: "The last failed checks, newest first",
      captured_at: "2026-11-15T10:42:07Z",
      truncated: false,
      rows: 0,
    },
    {
      name: "eventlog.errors",
      source: "CheckEventLog",
      captured_at: "2026-11-15T10:42:07Z",
      truncated: false,
      error: "The subscription could not be created",
      rows: 0,
    },
  ],
};

const TOP_CPU = {
  name: "processes.top_cpu",
  snapshot_id: 812,
  source: "CheckSystem",
  description: "The processes using the most CPU over the last second",
  captured_at: "2026-11-15T10:42:06Z",
  truncated: true,
  columns: [
    { name: "name", type: "string" },
    { name: "pid", type: "int" },
    { name: "cpu", type: "float", unit: "percent" },
    { name: "working_set", type: "int", unit: "bytes" },
  ],
  rows: [{ name: "acme-indexer.exe", pid: 4242, cpu: 87.5, working_set: 412000256 }],
};

const RECENT = {
  name: "failures.recent",
  snapshot_id: 812,
  source: "core",
  description: "The last failed checks, newest first",
  captured_at: "2026-11-15T10:42:07Z",
  truncated: false,
  columns: [
    { name: "time", type: "timestamp" },
    { name: "command", type: "string" },
  ],
  rows: [],
};

const ERRORS = {
  name: "eventlog.errors",
  snapshot_id: 812,
  source: "CheckEventLog",
  captured_at: "2026-11-15T10:42:07Z",
  truncated: false,
  error: "The subscription could not be created",
  columns: [{ name: "message", type: "string" }],
  rows: [],
};

const routes = (list: unknown = SUMMARY) => ({
  "/api/v2/diagnostics": jsonResponse(list),
  "/api/v2/diagnostics/processes.top_cpu": jsonResponse(TOP_CPU),
  "/api/v2/diagnostics/failures.recent": jsonResponse(RECENT),
  "/api/v2/diagnostics/eventlog.errors": jsonResponse(ERRORS),
});

describe("Diagnostics", () => {
  it("shows every section of the latest snapshot as a table, failures.recent first", async () => {
    const fetchMock = installFetchMock(routes());
    renderWithProviders(<Diagnostics />, { preloadedState: authenticatedState });

    expect(
      await screen.findByRole("table", { name: "processes.top_cpu rows" }),
    ).toBeInTheDocument();
    expect(screen.getByText("snapshot #812")).toBeInTheDocument();
    const headings = screen.getAllByRole("heading", { level: 3 }).map((h) => h.textContent);
    expect(headings).toEqual(["failures.recent", "eventlog.errors", "processes.top_cpu"]);
    const cpu = screen.getByRole("table", { name: "processes.top_cpu rows" });
    expect(within(cpu).getByText("acme-indexer.exe")).toBeInTheDocument();
    expect(within(cpu).getByText("87.5 %")).toBeInTheDocument();
    expect(screen.getByText("showing the first 1")).toBeInTheDocument();
    await waitFor(() =>
      expect(
        screen.getByText("Could not be collected: The subscription could not be created"),
      ).toBeInTheDocument(),
    );
    await waitFor(() => expect(screen.getByText(/No rows/)).toBeInTheDocument());

    // One request for the list and one per section.
    const paths = fetchMock.mock.calls.map((c) => new URL((c[0] as Request).url).pathname);
    expect(paths.filter((p) => p === "/api/v2/diagnostics")).toHaveLength(1);
    expect(paths.filter((p) => p.startsWith("/api/v2/diagnostics/"))).toHaveLength(3);
  });

  it("explains how to turn diagnostics on when they are off", async () => {
    installFetchMock(
      routes({
        enabled: false,
        snapshot_id: 0,
        captured_at: "1970-01-01T00:00:00Z",
        max_rows: 0,
        dropped: 0,
        excluded: [],
        count: 0,
        sections: [],
      }),
    );
    renderWithProviders(<Diagnostics />, { preloadedState: authenticatedState });

    expect(await screen.findByText(/Diagnostics are switched off/)).toBeInTheDocument();
    expect(screen.getByText("capture diagnostics = true")).toBeInTheDocument();
    expect(screen.getByText(/\[\/settings\/system\/windows\/diagnostics\]/)).toBeInTheDocument();
    expect(screen.queryByRole("table")).not.toBeInTheDocument();
  });

  it("offers the switches instead of the configuration sample when the settings can be read", async () => {
    installFetchMock({
      ...routes({ ...SUMMARY, enabled: false, snapshot_id: 0, count: 0, sections: [] }),
      "/api/v2/settings/descriptions": jsonResponse([
        {
          path: "/settings/core/failed checks",
          key: "capture diagnostics",
          type: "bool",
          title: "Collect diagnostics and record them with each failure",
          description: "Collect diagnostics.",
          value: "",
          default_value: "false",
          icon: "",
          is_advanced_key: true,
          is_object: false,
          is_sample_key: false,
          is_template_key: false,
          plugins: [],
          sample_usage: "",
        },
      ]),
    });
    renderWithProviders(<Diagnostics />, { preloadedState: authenticatedState });

    expect(await screen.findByLabelText("Core: capture diagnostics")).not.toBeChecked();
    expect(screen.getByText(/Diagnostics are switched off/)).toBeInTheDocument();
    expect(screen.getByText(/with the switches below/)).toBeInTheDocument();
    expect(
      screen.queryByText(/\[\/settings\/system\/windows\/diagnostics\]/),
    ).not.toBeInTheDocument();
  });

  it("does not offer the switches to a role that may not read diagnostics", async () => {
    installFetchMock({
      "/api/v2/diagnostics": new Response("403 You're not allowed", { status: 403 }),
      "/api/v2/settings/descriptions": jsonResponse([]),
    });
    renderWithProviders(<Diagnostics />, { preloadedState: authenticatedState });

    expect(await screen.findByText(/needs the/)).toBeInTheDocument();
    expect(screen.queryByText("Diagnostics tables")).not.toBeInTheDocument();
  });

  it("says when no round has run yet", async () => {
    installFetchMock(routes({ ...SUMMARY, snapshot_id: 0, count: 0, sections: [] }));
    renderWithProviders(<Diagnostics />, { preloadedState: authenticatedState });

    expect(await screen.findByText(/No diagnostics collected yet/)).toBeInTheDocument();
  });

  it("lists the sections prune pattern excluded and the ones dropped", async () => {
    installFetchMock(routes({ ...SUMMARY, excluded: ["processes.top_memory"], dropped: 2 }));
    renderWithProviders(<Diagnostics />, { preloadedState: authenticatedState });

    expect(await screen.findByText("processes.top_memory")).toBeInTheDocument();
    expect(screen.getByText(/Left out by/)).toBeInTheDocument();
    expect(screen.getByText(/2 tables were dropped/)).toBeInTheDocument();
  });

  it("says a section left the snapshot when it 404s between the list and the read", async () => {
    installFetchMock({
      ...routes(),
      "/api/v2/diagnostics/processes.top_cpu": new Response("Diagnostics section not found", {
        status: 404,
      }),
    });
    renderWithProviders(<Diagnostics />, { preloadedState: authenticatedState });

    expect(
      await screen.findByText("Could not be collected: no longer in the latest snapshot"),
    ).toBeInTheDocument();
  });

  it("explains a missing grant instead of logging the user out", async () => {
    installFetchMock({
      "/api/v2/diagnostics": new Response("403 You're not allowed", { status: 403 }),
    });
    const { store } = renderWithProviders(<Diagnostics />, { preloadedState: authenticatedState });

    expect(await screen.findByText(/needs the/)).toBeInTheDocument();
    expect(screen.getByText("diagnostics.list")).toBeInTheDocument();
    // diagnostics.list is in no built-in role but full: a 403 here is a grant,
    // and the session stays (an expired token is caught by the shell's polling).
    expect(store.getState().auth.token).toBe("test-token");
  });

  it("refetches on demand", async () => {
    const fetchMock = installFetchMock(routes());
    renderWithProviders(<Diagnostics />, { preloadedState: authenticatedState });

    await screen.findByRole("table", { name: "processes.top_cpu rows" });
    const lists = () =>
      fetchMock.mock.calls.filter(
        (c) => new URL((c[0] as Request).url).pathname === "/api/v2/diagnostics",
      ).length;
    expect(lists()).toBe(1);
    // The refresh button is the toolbar's only icon button.
    const buttons = screen.getAllByRole("button");
    await userEvent.click(buttons[buttons.length - 1]);
    await waitFor(() => expect(lists()).toBe(2));
  });
});
