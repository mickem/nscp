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
import { execSync } from "node:child_process";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

import {
  NscpInstance,
  OK,
  executeQuery,
  messageOf,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(300_000);

const onWindows = process.platform === "win32";
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

(onWindows ? describe : describe.skip)("CheckTaskSched check_tasksched", () => {
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
});
