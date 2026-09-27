import fs from "node:fs";
import net from "node:net";
import path from "node:path";
import { randomUUID } from "node:crypto";
import { execFileSync } from "node:child_process";
import request from "supertest";
import { NscpInstance, describeWithModules, hasModule, onWindows } from "@fixtures/index";

jest.setTimeout(180_000);

const serviceSection = `/settings/system/${onWindows ? "windows" : "unix"}/facts`;
function serviceAndTaskFacts(taskAlias: string) {
  const taskSection = `/settings/${taskAlias || "task schedule"}/facts`;
  let nscp: NscpInstance;
  let key: string;
  let withTasks: boolean;
  let REST_URL: string;
  let fixtureCreated = false;
  const taskName = `nscp_facts_${randomUUID()}`;

  async function facts(refresh = false) {
    const req = refresh
      ? request(REST_URL).post("/api/v2/facts/commands/refresh")
      : request(REST_URL).get("/api/v2/facts");
    return (await req.set("Authorization", `Bearer ${key}`).trustLocalhost(true).expect(200)).body;
  }

  async function toggle(enabled: boolean) {
    const settings = [[serviceSection, "services.installed"]];
    if (withTasks) settings.push([taskSection, "tasks.scheduled"]);
    for (const [section, name] of settings) {
      await request(REST_URL)
        .put("/api/v2/settings")
        .set("Authorization", `Bearer ${key}`)
        .send([{ path: section, key: name, value: String(enabled) }])
        .trustLocalhost(true)
        .expect(200);
    }
    for (const command of ["save", "reload"]) {
      await request(REST_URL)
        .post("/api/v2/settings/command")
        .set("Authorization", `Bearer ${key}`)
        .send({ command })
        .trustLocalhost(true)
        .expect(200);
    }
    const deadline = Date.now() + 60_000;
    let lastEnabled: string[] = [];
    while (Date.now() < deadline) {
      await new Promise((resolve) => setTimeout(resolve, 250));
      try {
        const response = await request(REST_URL)
          .get("/api/v2/facts")
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true);
        // Saving freshly configured credentials can invalidate a session on
        // reload. This test concerns facts; obtain a new session when needed.
        if (response.status === 403) {
          const login = await request(REST_URL)
            .get("/api/v2/login")
            .auth("admin", "facts-password")
            .trustLocalhost(true)
            .expect(200);
          key = login.body.key;
          continue;
        }
        expect(response.status).toEqual(200);
        const body = response.body;
        lastEnabled = body.enabled;
        if (
          body.enabled.includes("services") === enabled &&
          (!withTasks || body.enabled.includes("tasks") === enabled)
        )
          return body;
      } catch {
        // A reload briefly recreates the web listener.
      }
    }
    throw new Error(`Facts did not reflect the settings reload: enabled=${lastEnabled.join(",")}`);
  }

  beforeAll(async () => {
    // Use a private port so a developer's running agent is never contacted.
    const listener = net.createServer();
    await new Promise<void>((resolve) => listener.listen(0, "127.0.0.1", resolve));
    const port = (listener.address() as net.AddressInfo).port;
    await new Promise<void>((resolve, reject) =>
      listener.close((error) => (error ? reject(error) : resolve())),
    );
    REST_URL = `https://127.0.0.1:${port}`;
    withTasks = onWindows && hasModule("CheckTaskSched");
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        WEBServer: "enabled",
        CheckSystem: "enabled",
        ...(withTasks
          ? taskAlias
            ? { [taskAlias]: "CheckTaskSched" }
            : { CheckTaskSched: "enabled" }
          : {}),
      },
      "/settings/default": { "allowed hosts": "127.0.0.1,::1" },
      "/settings/WEB/server": { port: String(port) },
      "/settings/WEB/server/users/admin": { role: "full", password: "facts-password" },
      // A custom alias must ignore the default section, even if it is enabled.
      ...(withTasks && taskAlias
        ? { "/settings/task schedule/facts": { "tasks.scheduled": "true" } }
        : {}),
    });
    nscp.start();
    await nscp.waitForPort(port, { timeoutMs: 30_000 });
    const login = await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "facts-password")
      .trustLocalhost(true)
      .expect(200);
    key = login.body.key;
    if (withTasks) {
      const file = path.join(nscp.scratch("tasks"), "disabled-hidden.xml");
      fs.writeFileSync(
        file,
        `\uFEFF<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <Triggers />
  <Principals><Principal id="Author"><LogonType>InteractiveToken</LogonType></Principal></Principals>
  <Settings><Hidden>true</Hidden><Enabled>false</Enabled><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy></Settings>
  <Actions Context="Author"><Exec><Command>cmd.exe</Command><Arguments>/c exit</Arguments></Exec></Actions>
</Task>`,
        "utf16le",
      );
      execFileSync("schtasks.exe", ["/create", "/tn", taskName, "/xml", file], { stdio: "pipe" });
      fixtureCreated = true;
    }
  });

  afterAll(async () => {
    try {
      if (fixtureCreated)
        execFileSync("schtasks.exe", ["/delete", "/tn", taskName, "/f"], { stdio: "pipe" });
    } finally {
      await nscp?.stop();
    }
  });

  it("collects neither inventory by default", async () => {
    const body = await facts(true);
    expect(body.enabled).toEqual([]);
    expect(body.facts).toEqual({});
    expect(body.errors).toEqual({});
  });

  it("enables the inventories on reload and publishes only inventory fields", async () => {
    await toggle(true);
    const body = await facts(true);
    expect(body.enabled.sort()).toEqual(withTasks ? ["services", "tasks"] : ["services"]);
    if (!onWindows && !fs.existsSync("/run/systemd/system")) {
      expect(body.errors.services).toBeTruthy();
      expect(body.facts.services).toBeUndefined();
    } else {
      const services = body.facts.services.installed;
      expect(services.length).toBeGreaterThan(0);
      const ids = services.map((s: any) => s.id);
      expect(new Set(ids).size).toEqual(ids.length);
      expect(ids).toEqual([...ids].sort());
      for (const service of services) {
        expect(service.id).toEqual(service.name);
        expect(
          Object.keys(service).every((k) =>
            ["id", "name", "display_name", "start_type"].includes(k),
          ),
        ).toBe(true);
      }
      expect(body.gathered.services).toMatch(/^\d{4}-\d{2}-\d{2}T/);
    }
    if (withTasks) {
      const tasks = body.facts.tasks.scheduled;
      expect(Array.isArray(tasks)).toBe(true);
      const ids = tasks.map((t: any) => t.id);
      expect(new Set(ids).size).toEqual(ids.length);
      expect(ids).toEqual([...ids].sort());
      for (const task of tasks) {
        expect(task.id.startsWith("\\")).toBe(true);
        expect(typeof task.enabled).toEqual("boolean");
        expect(
          Object.keys(task).every((k) => ["id", "name", "folder", "enabled", "hidden"].includes(k)),
        ).toBe(true);
      }
      expect(body.gathered.tasks).toMatch(/^\d{4}-\d{2}-\d{2}T/);
      expect(tasks.find((task: any) => task.id === `\\${taskName}`)).toEqual({
        id: `\\${taskName}`,
        name: taskName,
        folder: "\\",
        enabled: false,
        hidden: true,
      });
    }
  });

  it("keeps check_tasksched filtering independent of the facts inventory", async () => {
    if (!withTasks) return;
    const response = await request(REST_URL)
      .get("/api/v1/queries/check_tasksched/commands/execute")
      .query({
        hidden: "true",
        filter: `title = '${taskName}'`,
        warning: "none",
        critical: "none",
        "top-syntax": "${list}",
        "detail-syntax": "${uri} enabled=${enabled} hidden=${hidden}",
      })
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    expect(response.body.result).toEqual(0);
    expect(response.body.lines[0].message).toContain(`\\${taskName} enabled=0 hidden=1`);
  });

  it("removes disabled sets and their errors on reload", async () => {
    const body = await toggle(false);
    expect(body.enabled).toEqual([]);
    expect(body.facts).toEqual({});
    expect(body.errors).toEqual({});
    expect(body.gathered).toEqual({});
  });
}

describeWithModules("CheckSystem", "WEBServer").each(onWindows ? ["", "custom tasks"] : [""])(
  "REST service and task facts (alias=%s)",
  serviceAndTaskFacts,
);
