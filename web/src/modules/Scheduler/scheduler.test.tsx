import { describe, expect, it } from "vitest";
import { screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { Settings } from "../../api/api";
import {
  authenticatedState,
  installFetchMock,
  jsonResponse,
  renderWithProviders,
} from "../../test/test-utils";
import SchedulesEditor from "./SchedulesEditor";
import {
  cronProblem,
  intervalSeconds,
  isFuzzy,
  reportSet,
  reportValue,
  scheduleProblem,
  SCHEDULES as S,
  scheduleValue,
} from "./scheduler";

describe("schedule formats", () => {
  it("reads intervals by the unit's first letter", () => {
    expect(intervalSeconds("30")).toBe(30);
    expect(intervalSeconds("5m")).toBe(300);
    expect(intervalSeconds("2 hours")).toBe(7200);
    expect(intervalSeconds("1w")).toBe(604800);
    expect(intervalSeconds("5x")).toBeNull();
    expect(intervalSeconds("1.5h")).toBeNull();
  });

  it("reads cron lines as the agent does: * or numbers, five fields", () => {
    expect(cronProblem("0,30 * * * 1,2,3")).toBeNull();
    expect(cronProblem("*/5 * * * *")).toMatch(/ranges, steps and names/);
    expect(cronProblem("0 24 * * *")).toMatch(/hour field runs from 0 to 23/);
    expect(cronProblem("0 * * *")).toMatch(/five fields/);
  });

  it("reads any randomness as fuzzy", () => {
    expect(isFuzzy("20%")).toBe(true);
    expect(isFuzzy("5")).toBe(true);
    expect(isFuzzy("0")).toBe(false);
    expect(isFuzzy("")).toBe(false);
  });

  it("reads and writes report", () => {
    expect([...reportSet("warn, CRIT")]).toEqual(["warning", "critical"]);
    expect(reportValue(reportSet("all"))).toBe("all");
    expect(reportValue(new Set(["critical", "ok"]))).toBe("ok,critical");
  });
});

describe("schedule values", () => {
  const stored: Settings[] = [
    { path: `${S}/default`, key: "interval", value: "5m" },
    { path: S, key: "cpu", value: "check_cpu" },
    { path: `${S}/slow`, key: "parent", value: "hourly" },
    { path: `${S}/hourly`, key: "interval", value: "1h" },
    { path: `${S}/hourly`, key: "channel", value: "NRDP" },
  ];

  it("takes what a schedule leaves out from its parent, then the default schedule", () => {
    expect(scheduleValue(stored, "cpu", "interval", { command: "check_cpu" })).toBe("5m");
    expect(scheduleValue(stored, "cpu", "channel", {})).toBe("NSCA");
    expect(scheduleValue(stored, "slow", "interval", { parent: "hourly" })).toBe("1h");
    expect(scheduleValue(stored, "slow", "channel", { parent: "hourly" })).toBe("NRDP");
  });

  it("says when the agent refuses a schedule", () => {
    const get = (m: Record<string, string>) => (k: string) => m[k] ?? "";
    expect(scheduleProblem(get({ command: "x", interval: "5m" }), false)).toBeNull();
    expect(
      scheduleProblem(get({ command: "x", interval: "5m", schedule: "0 * * * *" }), false),
    ).toMatch(/both/);
    expect(scheduleProblem(get({ command: "x" }), false)).toMatch(/interval or a schedule/);
    expect(scheduleProblem(get({ command: "x", interval: "0" }), false)).toMatch(/0 is refused/);
    expect(scheduleProblem(get({}), true)).toBeNull();
  });
});

describe("schedules editor", () => {
  const render = (stored: Settings[]) => {
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
      "/api/v2/queries": jsonResponse([]),
      "/api/v2/aliases": jsonResponse([]),
    });
    renderWithProviders(<SchedulesEditor />, { preloadedState: authenticatedState });
    return writes;
  };
  const dialog = () => within(screen.getByRole("dialog"));

  it("lists default first, and what each schedule takes from it", async () => {
    render([
      { path: `${S}/default`, key: "interval", value: "5m" },
      { path: S, key: "cpu", value: "check_cpu" },
    ]);
    const table = within(await screen.findByRole("table", { name: "Schedules" }));
    const rows = table.getAllByRole("row");
    expect(within(rows[1]).getByText("default")).toBeInTheDocument();
    expect(within(rows[2]).getByText("check_cpu")).toBeInTheDocument();
    expect(within(rows[2]).getByText("every 5m")).toBeInTheDocument();
    expect(within(rows[2]).getByText("NSCA")).toBeInTheDocument();
  });

  it("marks templates, and a schedule the agent refuses", async () => {
    render([
      { path: `${S}/default`, key: "interval", value: "5m" },
      { path: `${S}/cron`, key: "schedule", value: "0 2 * * *" },
      { path: `${S}/cron`, key: "is template", value: "true" },
      { path: `${S}/nightly`, key: "command", value: "check_ok" },
      { path: `${S}/nightly`, key: "parent", value: "cron" },
    ]);
    const row = async (n: string) =>
      within((await screen.findByText(n, { selector: "p" })).closest("tr") as HTMLElement);
    expect((await row("cron")).getByText(/Template: not run/)).toBeInTheDocument();
    // An inherited cron line and an inherited interval: both, refused.
    expect((await row("nightly")).getByText("refused by the agent")).toBeInTheDocument();
  });

  it("adds a one-line schedule when only its command differs from default", async () => {
    const writes = render([{ path: `${S}/default`, key: "interval", value: "5m" }]);
    await userEvent.click(await screen.findByRole("button", { name: "Add schedule" }));
    await userEvent.type(dialog().getByLabelText("Name"), "cpu");
    await userEvent.type(dialog().getByLabelText("Command"), "check_cpu");
    await userEvent.click(dialog().getByRole("button", { name: "Save" }));
    await waitFor(() => expect(writes).toEqual([`[${S}] cpu=check_cpu`]));
  });

  it("refuses a cron schedule under a default that has an interval", async () => {
    render([{ path: `${S}/default`, key: "interval", value: "5m" }]);
    await userEvent.click(await screen.findByRole("button", { name: "Add schedule" }));
    await userEvent.type(dialog().getByLabelText("Name"), "nightly");
    await userEvent.type(dialog().getByLabelText("Command"), "check_ok");
    await userEvent.click(dialog().getByRole("radio", { name: "At set times (cron)" }));
    expect(dialog().getByText(/both an interval and a schedule/)).toBeInTheDocument();
    expect(dialog().getByRole("button", { name: "Save" })).toBeDisabled();
  });

  it("writes a schedule with its own interval and results as a section", async () => {
    const writes = render([]);
    await userEvent.click(await screen.findByRole("button", { name: "Add schedule" }));
    await userEvent.type(dialog().getByLabelText("Name"), "cpu");
    await userEvent.type(dialog().getByLabelText("Command"), "check_cpu");
    await userEvent.type(dialog().getByLabelText("Interval"), "1m");
    await userEvent.click(dialog().getByRole("checkbox", { name: "OK" }));
    await userEvent.click(dialog().getByRole("button", { name: "Save" }));
    await waitFor(() =>
      expect(writes).toEqual([
        `[${S}/cpu] command=check_cpu`,
        `[${S}/cpu] interval=1m`,
        `[${S}/cpu] report=warning,critical,unknown`,
      ]),
    );
  });

  it("turns fuzzy on as 20%, shows any randomness as fuzzy, and renames behind a toggle", async () => {
    const writes = render([
      { path: `${S}/default`, key: "interval", value: "5m" },
      { path: `${S}/cpu`, key: "command", value: "check_cpu" },
      { path: `${S}/cpu`, key: "randomness", value: "7" },
    ]);
    expect(await screen.findByText("every 5m, fuzzy")).toBeInTheDocument();
    await userEvent.click(screen.getByRole("button", { name: "Add schedule" }));
    await userEvent.type(dialog().getByLabelText("Name"), "mem");
    await userEvent.type(dialog().getByLabelText("Command"), "check_memory");
    expect(dialog().queryByLabelText("Service name")).not.toBeInTheDocument();
    await userEvent.click(dialog().getByRole("switch", { name: "Rename" }));
    await userEvent.type(dialog().getByLabelText("Service name"), "Memory");
    await userEvent.click(dialog().getByRole("switch", { name: "Fuzzy" }));
    await userEvent.click(dialog().getByLabelText("Channel"));
    await userEvent.click(await screen.findByRole("option", { name: "NRDP" }));
    await userEvent.click(dialog().getByRole("button", { name: "Save" }));
    await waitFor(() =>
      expect(writes).toEqual([
        `[${S}/mem] command=check_memory`,
        `[${S}/mem] randomness=20%`,
        `[${S}/mem] channel=NRDP`,
        `[${S}/mem] alias=Memory`,
      ]),
    );
  });

  it("turns fuzzy off over an inherited randomness by writing 0", async () => {
    const writes = render([
      { path: `${S}/default`, key: "interval", value: "5m" },
      { path: `${S}/default`, key: "randomness", value: "10%" },
      { path: `${S}/cpu`, key: "command", value: "check_cpu" },
    ]);
    const row = (await screen.findByText("cpu", { selector: "p" })).closest("tr") as HTMLElement;
    await userEvent.click(within(row).getByRole("button", { name: "Edit" }));
    const fuzzy = dialog().getByRole("switch", { name: "Fuzzy" });
    expect(fuzzy).toBeChecked();
    await userEvent.click(fuzzy);
    await userEvent.click(dialog().getByRole("button", { name: "Save" }));
    await waitFor(() => expect(writes).toEqual([`[${S}/cpu] randomness=0`]));
  });
});
