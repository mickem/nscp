import { describe, expect, it } from "vitest";
import { screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import FailedChecks from "./FailedChecks";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const ENTRIES = [
  {
    index: 2,
    timestamp: 1760000060,
    date: "2025-Oct-09 10:01:00",
    age: 30,
    subject: "WEBServer:admin",
    depth: 0,
    command: "check_drivesize",
    alias: "",
    status: 2,
    result: "CRITICAL",
    message: "CRITICAL C: 92% used",
    lines: [
      {
        message: "CRITICAL C: 92% used",
        perf: {
          "C: used %": {
            value: 92,
            unit: "%",
            warning: 80,
            critical: "@90:100",
            minimum: 0,
            maximum: 100,
          },
        },
      },
    ],
    arguments: ["drive=C:", "password=***"],
    context: {
      state: "captured",
      snapshot_id: 7,
      captured_at: 1760000055,
      age_at_failure: 5,
      size: 2048,
    },
  },
  {
    index: 1,
    timestamp: 1760000000,
    date: "2025-Oct-09 10:00:00",
    age: 90,
    subject: "NRPEServer",
    depth: 1,
    command: "check_cpu",
    alias: "cpu",
    status: 1,
    result: "WARNING",
    message: "WARNING: 5m: 85%",
    lines: [{ message: "WARNING: 5m: 85%", perf: {} }],
    arguments: [],
    context: {
      state: "evicted",
      snapshot_id: 6,
      captured_at: 1759999995,
      age_at_failure: 5,
      size: 2048,
    },
  },
];

const CONTEXT = {
  index: 2,
  timestamp: 1760000060,
  state: "captured",
  snapshot_id: 7,
  captured_at: 1760000055,
  age_at_failure: 5,
  size: 2048,
  metrics: { "system.cpu.total.idle": 3, "system.mem.physical.used": 15032385536 },
  metadata: {
    "system.cpu.total.idle": { type: "gauge", unit: "percent", help: "Idle time" },
    "system.mem.physical.used": { type: "gauge", unit: "bytes" },
  },
};

const DIAGNOSTICS_REF = {
  state: "captured",
  snapshot_id: 8,
  captured_at: 1760000057,
  age_at_failure: 3,
  size: 900,
};

const DIAGNOSTICS_CONTEXT = {
  ...DIAGNOSTICS_REF,
  // Deliberately not in reading order: the page sorts them.
  sections: {
    "services.problems": {
      source: "CheckSystem",
      captured_at: "2025-10-09T10:00:50Z",
      truncated: false,
      error: "Access is denied.",
      columns: [{ name: "name", type: "string" }],
      rows: [],
    },
    "processes.top_cpu": {
      source: "CheckSystem",
      description: "The processes using the most CPU over the last second",
      captured_at: "2025-10-09T10:00:56Z",
      truncated: true,
      columns: [
        { name: "name", type: "string" },
        { name: "pid", type: "int" },
        { name: "cpu", type: "float", unit: "percent" },
        { name: "working_set", type: "int", unit: "bytes" },
      ],
      rows: [{ name: "acme-indexer.exe", pid: 4242, cpu: 87.5, working_set: 412000256 }],
    },
    "failures.recent": {
      source: "core",
      captured_at: "2025-10-09T10:00:57Z",
      truncated: false,
      columns: [
        { name: "time", type: "timestamp" },
        { name: "command", type: "string" },
      ],
      rows: [{ time: "2025-10-09T10:00:00Z", command: "check_cpu" }],
    },
  },
};

const envelope = (over: Record<string, unknown> = {}) =>
  jsonResponse({
    enabled: true,
    max_entries: 50,
    arguments_recorded: true,
    statuses: "warning,critical,unknown",
    last_index: 2,
    dropped: 3,
    capture_metrics: true,
    context_budget: 4194304,
    max_context_size: 1048576,
    context_bytes_used: 2048,
    count: ENTRIES.length,
    entries: ENTRIES,
    ...over,
  });

describe("FailedChecks", () => {
  it("lists the log newest first with status, command, message and source", async () => {
    installFetchMock({ "/api/v2/failures": envelope() });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    const rows = screen.getAllByRole("row");
    // Header, then the two entries (each followed by its collapsed detail row).
    expect(within(rows[1]).getByText("Critical")).toBeInTheDocument();
    expect(within(rows[1]).getByText("CRITICAL C: 92% used")).toBeInTheDocument();
    expect(within(rows[1]).getByText("WEBServer:admin")).toBeInTheDocument();
    expect(screen.getByText("check_cpu")).toBeInTheDocument();
    expect(screen.getByText("Warning")).toBeInTheDocument();
    expect(screen.getByText("NRPEServer")).toBeInTheDocument();
    // A check run by another check is marked as such.
    expect(screen.getByText("nested")).toBeInTheDocument();
    // The configuration line: cap, evictions and whether arguments are kept.
    expect(screen.getByText(/Keeping the last 50 checks/)).toBeInTheDocument();
    expect(screen.getByText(/3 older entries evicted/)).toBeInTheDocument();
    expect(screen.getByText(/· arguments recorded/)).toBeInTheDocument();
  });

  it("expands an entry to its lines, perf data and arguments", async () => {
    installFetchMock({ "/api/v2/failures": envelope() });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[0]);

    const perf = screen.getByRole("table", { name: "performance data" });
    expect(within(perf).getByText("C: used %")).toBeInTheDocument();
    expect(within(perf).getByText("92")).toBeInTheDocument();
    // A Nagios range threshold is shown as the string it was.
    expect(within(perf).getByText("@90:100")).toBeInTheDocument();
    expect(screen.getByText("drive=C:")).toBeInTheDocument();
    // Redacted by the agent; the UI shows what it got.
    expect(screen.getByText("password=***")).toBeInTheDocument();
  });

  it("shows the metrics the failure was recorded with once the row is opened", async () => {
    const fetchMock = installFetchMock({
      "/api/v2/failures": envelope(),
      "/api/v2/failures/2/context": jsonResponse(CONTEXT),
    });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    expect(screen.getByText(/metrics captured \(2\.0 KiB of 4\.0 MiB\)/)).toBeInTheDocument();
    // Nothing is fetched for a row nobody opened.
    const contextCalls = () =>
      fetchMock.mock.calls.filter((c) =>
        new URL((c[0] as Request).url).pathname.endsWith("/context"),
      );
    expect(contextCalls()).toHaveLength(0);

    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[0]);
    const table = await screen.findByRole("table", { name: "metrics at the time" });
    expect(within(table).getByText("system.cpu.total.idle")).toBeInTheDocument();
    expect(within(table).getByText("percent")).toBeInTheDocument();
    expect(within(table).getByText("15032385536")).toBeInTheDocument();
    expect(screen.getByText(/snapshot #7, 5s before the failure/)).toBeInTheDocument();
    expect(contextCalls()).toHaveLength(1);

    await userEvent.type(screen.getByPlaceholderText("Filter metrics"), "mem");
    expect(within(table).queryByText("system.cpu.total.idle")).not.toBeInTheDocument();
    expect(within(table).getByText("system.mem.physical.used")).toBeInTheDocument();
  });

  it("says what pruning left out of the snapshot", async () => {
    installFetchMock({
      "/api/v2/failures": envelope(),
      "/api/v2/failures/2/context": jsonResponse({
        ...CONTEXT,
        pruned: {
          rules: "zeros,duplicates",
          zeros: 12,
          empty: 0,
          duplicates: 11,
          patterns: ["virtualbox"],
          excluded: 4,
          collapsed: { "disk.io.C:": "disk.io._Total" },
        },
      }),
    });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[0]);
    expect(
      await screen.findByText(
        /Pruned \(zeros,duplicates, "virtualbox"\): 12 zero, 11 duplicate, 4 excluded values left out/,
      ),
    ).toBeInTheDocument();
    const collapsed = screen.getByRole("list", { name: "collapsed instances" });
    expect(within(collapsed).getByText("disk.io.C:")).toBeInTheDocument();
    expect(within(collapsed).getByText("disk.io._Total")).toBeInTheDocument();
  });

  it("says why an entry has no metrics instead of fetching them", async () => {
    installFetchMock({ "/api/v2/failures": envelope() });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_cpu")).toBeInTheDocument());
    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[1]);
    expect(screen.getByText("evicted")).toBeInTheDocument();
    expect(
      screen.getByText(/released to stay within the metrics context budget/),
    ).toBeInTheDocument();
  });

  it("points at the setting when metrics are not captured", async () => {
    const off = ENTRIES.map((e) => ({ ...e, context: { state: "disabled" } }));
    installFetchMock({ "/api/v2/failures": envelope({ capture_metrics: false, entries: off }) });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    expect(screen.getByText(/metrics not captured \(capture metrics = false/)).toBeInTheDocument();
    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[0]);
    expect(screen.getByText("disabled")).toBeInTheDocument();
  });

  it("shows the diagnostics the failure was recorded with, failures.recent first", async () => {
    const withDiagnostics = ENTRIES.map((e) => ({ ...e, diagnostics: DIAGNOSTICS_REF }));
    const fetchMock = installFetchMock({
      "/api/v2/failures": envelope({
        capture_diagnostics: true,
        diagnostics_rows: 10,
        entries: withDiagnostics,
      }),
      "/api/v2/failures/2/context": jsonResponse({ ...CONTEXT, diagnostics: DIAGNOSTICS_CONTEXT }),
    });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    expect(
      screen.getByText(/diagnostics captured \(up to 10 rows per table\)/),
    ).toBeInTheDocument();
    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[0]);

    expect(await screen.findByText("Diagnostics at the time")).toBeInTheDocument();
    expect(screen.getByText(/snapshot #8, 3s before the failure/)).toBeInTheDocument();
    const headings = (await screen.findAllByRole("heading", { level: 3 })).map(
      (h) => h.textContent,
    );
    expect(headings).toEqual(["failures.recent", "processes.top_cpu", "services.problems"]);
    const cpu = screen.getByRole("table", { name: "processes.top_cpu rows" });
    expect(within(cpu).getByText("acme-indexer.exe")).toBeInTheDocument();
    expect(within(cpu).getByText("87.5 %")).toBeInTheDocument();
    expect(within(cpu).getByText("392.9 MiB")).toBeInTheDocument();
    expect(screen.getByText("showing the first 1")).toBeInTheDocument();
    expect(screen.getByText("Could not be collected: Access is denied.")).toBeInTheDocument();
    // The metrics view is still there, and both came from one request.
    expect(screen.getByRole("table", { name: "metrics at the time" })).toBeInTheDocument();
    const contextCalls = fetchMock.mock.calls.filter((c) =>
      new URL((c[0] as Request).url).pathname.endsWith("/context"),
    );
    expect(contextCalls).toHaveLength(1);
  });

  it("fetches the diagnostics even when the metrics were not captured", async () => {
    const entries = [
      { ...ENTRIES[0], context: { state: "disabled" }, diagnostics: DIAGNOSTICS_REF },
    ];
    installFetchMock({
      "/api/v2/failures": envelope({ capture_metrics: false, capture_diagnostics: true, entries }),
      "/api/v2/failures/2/context": jsonResponse({
        ...CONTEXT,
        metrics: undefined,
        diagnostics: DIAGNOSTICS_CONTEXT,
      }),
    });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[0]);
    expect(
      await screen.findByRole("table", { name: "processes.top_cpu rows" }),
    ).toBeInTheDocument();
    expect(screen.queryByRole("table", { name: "metrics at the time" })).not.toBeInTheDocument();
  });

  it.each([
    ["evicted", /Diagnostics released to stay within the metrics context budget/],
    ["too_large", /Diagnostics larger than max context size/],
    ["none", /Diagnostics no diagnostics had been collected yet/],
  ])("says why an entry has no diagnostics (%s) instead of fetching them", async (state, text) => {
    const entries = [{ ...ENTRIES[1], diagnostics: { state } }];
    const fetchMock = installFetchMock({
      "/api/v2/failures": envelope({ capture_diagnostics: true, entries }),
    });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_cpu")).toBeInTheDocument());
    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[0]);
    expect(screen.getByText(text)).toBeInTheDocument();
    // The chip beside the heading (the metrics context of this entry may carry the same one).
    expect(screen.getAllByText(state.replace("_", " ")).length).toBeGreaterThan(0);
    expect(
      fetchMock.mock.calls.some((c) =>
        new URL((c[0] as Request).url).pathname.endsWith("/context"),
      ),
    ).toBe(false);
  });

  it("points at the setting when diagnostics are not captured", async () => {
    const entries = ENTRIES.map((e) => ({
      ...e,
      context: { state: "disabled" },
      diagnostics: { state: "disabled" },
    }));
    installFetchMock({
      "/api/v2/failures": envelope({ capture_metrics: false, capture_diagnostics: false, entries }),
    });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    expect(
      screen.getByText(/diagnostics not captured \(capture diagnostics = false/),
    ).toBeInTheDocument();
    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[0]);
    expect(
      screen.getByText(/Diagnostics not captured \(capture diagnostics = false/),
    ).toBeInTheDocument();
  });

  it("shows no diagnostics section for an agent that predates them", async () => {
    installFetchMock({
      "/api/v2/failures": envelope(),
      "/api/v2/failures/2/context": jsonResponse(CONTEXT),
    });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    expect(screen.queryByText(/diagnostics/)).not.toBeInTheDocument();
    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[0]);
    await screen.findByRole("table", { name: "metrics at the time" });
    expect(screen.queryByText("Diagnostics at the time")).not.toBeInTheDocument();
  });

  it("says when only the allow-listed arguments are recorded", async () => {
    installFetchMock({ "/api/v2/failures": envelope({ arguments_mode: "safe" }) });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    expect(screen.getByText(/allow-listed arguments recorded/)).toBeInTheDocument();
  });

  it("says when arguments were not recorded rather than showing an empty list", async () => {
    const stripped = ENTRIES.map((e) => {
      const copy: Record<string, unknown> = { ...e };
      delete copy.arguments;
      return copy;
    });
    installFetchMock({
      "/api/v2/failures": envelope({
        arguments_recorded: false,
        arguments_mode: "none",
        entries: stripped,
      }),
    });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    expect(screen.getByText(/arguments not recorded/)).toBeInTheDocument();
    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[0]);
    expect(screen.getByText("not recorded")).toBeInTheDocument();
  });

  it("filters by status and by text", async () => {
    installFetchMock({ "/api/v2/failures": envelope() });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    await userEvent.click(screen.getByLabelText("Status"));
    await userEvent.click(screen.getByRole("option", { name: "Warning" }));
    expect(screen.queryByText("check_drivesize")).not.toBeInTheDocument();
    expect(screen.getByText("check_cpu")).toBeInTheDocument();
    expect(screen.getByText("1/2")).toBeInTheDocument();

    await userEvent.click(screen.getByLabelText("Status"));
    await userEvent.click(screen.getByRole("option", { name: "All" }));
    await userEvent.type(screen.getByPlaceholderText("Filter failed checks"), "nrpe");
    expect(screen.queryByText("check_drivesize")).not.toBeInTheDocument();
    expect(screen.getByText("check_cpu")).toBeInTheDocument();
  });

  it("explains a switched-off log", async () => {
    installFetchMock({
      "/api/v2/failures": envelope({
        enabled: false,
        count: 0,
        entries: [],
        max_entries: 0,
        statuses: "",
      }),
    });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText(/switched off/)).toBeInTheDocument());
    expect(screen.getByText("No failed checks recorded.")).toBeInTheDocument();
    expect(screen.getByRole("button", { name: /Clear all/ })).toBeDisabled();
  });

  it("clears the log over DELETE", async () => {
    let cleared = false;
    const fetchMock = installFetchMock({
      "/api/v2/failures": (request: Request) => {
        if (request.method === "DELETE") {
          cleared = true;
          return jsonResponse({ enabled: true, removed: 2 });
        }
        return cleared ? envelope({ count: 0, entries: [] }) : envelope();
      },
    });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    await userEvent.click(screen.getByRole("button", { name: /Clear all/ }));

    await waitFor(() => expect(screen.getByText("No failed checks recorded.")).toBeInTheDocument());
    const deletes = fetchMock.mock.calls.filter((c) => (c[0] as Request).method === "DELETE");
    expect(deletes).toHaveLength(1);
  });
});
