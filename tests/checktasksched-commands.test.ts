/**
 * Exercises CheckTaskSched's check_tasksched end-to-end (Windows only —
 * CheckTaskSched is a Windows module). Focus: the `uri` (task path) and
 * `hidden` keywords, and the interplay with the `hidden=true` enumeration
 * option (hidden tasks are excluded unless it is passed).
 *
 * A per-user hidden task is registered as a deterministic fixture (schtasks,
 * no elevation needed) so `hidden=1` and `uri` can be asserted on real data,
 * then removed in afterAll.
 */
import { execFileSync, execSync } from "node:child_process";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

import {
  NscpInstance,
  OK,
  WARNING,
  executeQuery,
  messageOf,
  setupQueryNscp,
  describeOnWindows,
} from "@fixtures/index";

jest.setTimeout(300_000);

const TASK = "nscp_parity_hidden_test";

// A minimal per-user task with the Hidden flag set. InteractiveToken + Author
// context means it registers without elevation.
const HIDDEN_TASK_XML = `<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo><Description>nscp integration test - hidden</Description></RegistrationInfo>
  <Triggers />
  <Principals><Principal id="Author"><LogonType>InteractiveToken</LogonType></Principal></Principals>
  <Settings><Hidden>true</Hidden><Enabled>true</Enabled><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy></Settings>
  <Actions Context="Author"><Exec><Command>cmd.exe</Command><Arguments>/c exit</Arguments></Exec></Actions>
</Task>`;

describeOnWindows("CheckTaskSched check_tasksched", () => {
  let nscp: NscpInstance;
  let key: string;

  beforeAll(async () => {
    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckTaskSched");

    // Register the hidden fixture task. Task Scheduler stores XML as UTF-16.
    const dir = mkdtempSync(join(tmpdir(), "nscp-task-"));
    try {
      const xmlPath = join(dir, "hidden.xml");
      writeFileSync(xmlPath, `﻿${HIDDEN_TASK_XML}`, "utf16le");
      execSync(`schtasks /create /tn "${TASK}" /xml "${xmlPath}" /f`, {
        stdio: "ignore",
      });
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  afterAll(async () => {
    try {
      execSync(`schtasks /delete /tn "${TASK}" /f`, { stdio: "ignore" });
    } catch {
      /* best-effort cleanup */
    }
    await nscp?.stop();
  });

  it("hidden=true enumerates the hidden task and reports uri + hidden=1", async () => {
    const q = await executeQuery(key, "check_tasksched", {
      hidden: "true", // valued boolean over REST (must be accepted, not bool_switch)
      filter: `title = '${TASK}'`,
      warning: "none",
      critical: "none",
      "top-syntax": "${list}",
      "detail-syntax": "${title} uri=${uri} hidden=${hidden}",
    });
    expect(q.result).toBe(OK);
    const msg = messageOf(q);
    // uri is the task's full path; the fixture lives in the root folder.
    expect(msg).toContain(`uri=\\${TASK}`);
    expect(msg).toContain("hidden=1");
  });

  it("excludes hidden tasks from enumeration without hidden=true", async () => {
    const q = await executeQuery(key, "check_tasksched", {
      filter: `title = '${TASK}'`,
      warning: "none",
      critical: "none",
      "top-syntax": "count=${count}",
      "detail-syntax": "${title}",
    });
    // The default enumeration omits hidden tasks, so the fixture never matches —
    // the check falls through to its empty-state "No tasks found" message rather
    // than listing the task.
    const msg = messageOf(q);
    expect(msg).toMatch(/No tasks found/i);
    expect(msg).not.toContain(TASK);
  });

  it("a filter that matches nothing takes the empty state (WARNING by default)", async () => {
    // The #1499 shape: the default warn/crit on exit_code are re-evaluated
    // with no task bound once nothing matched. check_tasksched defaults to
    // empty-state=warning (a task list that comes back empty deserves a look);
    // the option still relaxes it.
    const args = { filter: "title = 'nosuchtask-1499'" };
    const q = await executeQuery(key, "check_tasksched", args);
    expect(q.result).toBe(WARNING);
    expect(messageOf(q)).toMatch(/No tasks found/i);
    const relaxed = await executeQuery(key, "check_tasksched", { ...args, "empty-state": "ok" });
    expect(relaxed.result).toBe(OK);
  });

  /**
   * Tasks that have really run, ported from the legacy
   * scripts/python/test_w32_schetask.py: four per-user tasks whose action
   * exits 0, 1 and 2, plus one that prints a long output before exiting 0.
   * Each is run once through schtasks, and exit_code must then read back
   * exactly what the action returned.
   */
  describe("exit_code of tasks that ran", () => {
    const suffix = `${Date.now()}_${process.pid}`;
    const expected: Record<string, number> = { OK: 0, WARN: 1, CRIT: 2, LONG: 0 };
    const taskName = (state: string) => `nscp_exit_${state}_${suffix}`;
    let dir: string;

    function schtasks(args: string[]): void {
      execFileSync("schtasks.exe", args, { stdio: "ignore" });
    }

    /** exit_code = `code` as the warning, so WARNING means it matched. */
    function exitCodeIs(state: string, code: number) {
      return executeQuery(key, "check_tasksched", {
        filter: `title = '${taskName(state)}'`,
        warning: `exit_code = ${code}`,
      });
    }

    beforeAll(async () => {
      dir = mkdtempSync(join(tmpdir(), "nscp-task-exit-"));
      for (const state of Object.keys(expected)) {
        const lines = state === "LONG" ? Array(11).fill(`echo ${"0123456789".repeat(10)}`) : [];
        const bat = join(dir, `${state.toLowerCase()}.bat`);
        writeFileSync(bat, ["@echo off", ...lines, `exit /b ${expected[state]}`, ""].join("\r\n"));
        schtasks([
          "/Create",
          "/SC",
          "DAILY",
          "/TN",
          taskName(state),
          "/TR",
          bat,
          "/ST",
          "00:00",
          "/F",
        ]);
        schtasks(["/Run", "/TN", taskName(state)]);
      }
      // A task that has never run reads 267011 (SCHED_S_TASK_HAS_NOT_RUN), so
      // this waits for the run to finish even for the tasks that exit 0.
      for (const state of Object.keys(expected)) {
        const deadline = Date.now() + 120_000;
        while ((await exitCodeIs(state, expected[state])).result !== WARNING) {
          if (Date.now() > deadline)
            throw new Error(`task ${taskName(state)} never reported exit code ${expected[state]}`);
          await new Promise((r) => setTimeout(r, 2_000));
        }
      }
    });

    afterAll(() => {
      for (const state of Object.keys(expected)) {
        try {
          schtasks(["/Delete", "/TN", taskName(state), "/F"]);
        } catch {
          /* best-effort cleanup */
        }
      }
      if (dir) rmSync(dir, { recursive: true, force: true });
    });

    it.each(Object.entries(expected))(
      "the %s task reads exit_code %i and nothing else",
      async (state, code) => {
        for (const probe of [0, 1, 2, 3, 4]) {
          const q = await exitCodeIs(state, probe);
          expect({ probe, result: q.result, message: messageOf(q) }).toEqual({
            probe,
            result: probe === code ? WARNING : OK,
            message: expect.any(String),
          });
        }
      },
    );
  });
});
