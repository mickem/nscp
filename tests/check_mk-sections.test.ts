/**
 * The system sections of the Checkmk agent dump (default_check_mk.lua plus
 * the `fetch-only` modes it wraps), read straight off CheckMKServer's TCP
 * port without Docker. Checkmk drops a section it cannot parse without a
 * word, so these assert the line shapes its parsers need, not just that the
 * section headers are there: check_mk-agent.test.ts covers the local/MRPE
 * bridge through a real netcat container.
 */
import * as net from "net";
import { NscpInstance, describeIf, moduleBuiltHere, onDarwin, onWindows } from "@fixtures/index";

jest.setTimeout(300_000);

// Not 6556: check_mk-agent.test.ts binds that one, and the suites run back to back.
const PORT = 16556;

function fetchDump(port: number): Promise<string> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    const socket = net.connect(port, "127.0.0.1");
    socket.setTimeout(60_000, () => socket.destroy(new Error("timed out reading the agent dump")));
    socket.on("data", (c) => chunks.push(c));
    socket.on("end", () => resolve(Buffer.concat(chunks).toString("utf8")));
    socket.on("error", reject);
  });
}

/** Section title (without a `:sep(..)` suffix) -> its lines. */
function parseSections(dump: string): Map<string, string[]> {
  const sections = new Map<string, string[]>();
  let current: string[] | null = null;
  for (const line of dump.split(/\r?\n/)) {
    const m = /^<<<([^:>]+)(:[^>]*)?>>>$/.exec(line);
    if (m) {
      current = sections.get(m[1]) ?? [];
      sections.set(m[1], current);
    } else if (current && line !== "") {
      current.push(line);
    }
  }
  return sections;
}

// macOS: no CheckDisk, and no /proc for the Linux counter sections.
describeIf(!onDarwin && moduleBuiltHere("CheckDisk"))("check_mk agent system sections", () => {
  let nscp: NscpInstance;
  let sections: Map<string, string[]>;
  let dump = "";

  const section = (name: string): string[] => {
    const lines = sections.get(name);
    if (!lines) throw new Error(`no <<<${name}>>> section in the dump:\n${dump}`);
    return lines;
  };

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        CheckMKServer: "enabled",
        LUAScript: "enabled",
        CheckSystem: "enabled",
        CheckDisk: "enabled",
      },
      "/settings/check_mk/server": { port: PORT, "allowed hosts": "127.0.0.1" },
    });
    await nscp.waitForPortFree(PORT);
    nscp.start();
    await nscp.waitForPort(PORT, { timeoutMs: 30_000 });
    // The Windows <<<mem>>> and <<<uptime>>> come from the metrics store,
    // which the collector fills on its first tick; the dump skips them until then.
    await new Promise((r) => setTimeout(r, 5_000));
    dump = await fetchDump(PORT);
    sections = parseSections(dump);
  });

  afterAll(async () => {
    if (nscp) await nscp.stop();
  });

  it("header carries the OS label lines", () => {
    const header = section("check_mk");
    expect(header).toContain(`OSType: ${onWindows ? "windows" : "linux"}`);
    expect(header.some((l) => /^OSName: \S/.test(l))).toBe(true);
    expect(header.some((l) => /^OSVersion: \S/.test(l))).toBe(true);
  });

  it("ps lines are (user,vsz,rss,cputime,pid) command and carry owners", () => {
    const lines = section("ps");
    expect(lines.length).toBeGreaterThan(0);
    const shape = /^\(([^,\s()]*),(\d+),(\d+),(\d+),(\d+)\) \S/;
    for (const l of lines) expect(l).toMatch(shape);
    // Our own process, at least, has an owner the agent can name.
    expect(lines.filter((l) => !l.startsWith("(,")).length).toBeGreaterThan(0);
  });

  it("df lines have the df -PT columns", () => {
    const lines = section("df");
    const start = lines.indexOf("[df_inodes_start]");
    const blocks = start === -1 ? lines : lines.slice(0, start);
    expect(blocks.length).toBeGreaterThan(0);
    for (const l of blocks) {
      const cols = l.split(/\s+/);
      expect(cols.length).toBeGreaterThanOrEqual(7);
      expect(cols.slice(2, 5).every((c) => /^\d+$/.test(c))).toBe(true);
      expect(cols[5]).toMatch(/^(\d+%|-)$/);
    }
  });

  if (onWindows) {
    it("mem carries the Windows commit and virtual lines", () => {
      const keys = section("mem").map((l) => l.split(":")[0]);
      for (const k of [
        "MemTotal",
        "MemFree",
        "SwapTotal",
        "SwapFree",
        "PageTotal",
        "PageFree",
        "VirtualTotal",
        "VirtualFree",
      ]) {
        expect(keys).toContain(k);
      }
    });
    return;
  }

  it("df carries an inode block for the same filesystems", () => {
    const lines = section("df");
    const start = lines.indexOf("[df_inodes_start]");
    const end = lines.indexOf("[df_inodes_end]");
    expect(start).toBeGreaterThan(0);
    expect(end).toBeGreaterThan(start);
    const mounts = (ls: string[]) => ls.map((l) => l.split(/\s+/).slice(6).join(" "));
    expect(mounts(lines.slice(start + 1, end))).toEqual(mounts(lines.slice(0, start)));
  });

  it("mem is /proc/meminfo as-is", () => {
    const keys = section("mem").map((l) => l.split(":")[0]);
    for (const k of [
      "MemTotal",
      "MemFree",
      "MemAvailable",
      "Buffers",
      "Cached",
      "SwapTotal",
      "SwapFree",
    ]) {
      expect(keys).toContain(k);
    }
  });

  it("cpu is the load averages with the CPU count, then the thread limit", () => {
    const lines = section("cpu");
    const cols = lines[0].split(" ");
    expect(cols).toHaveLength(6);
    expect(cols[3]).toMatch(/^\d+\/\d+$/);
    expect(Number(cols[5])).toBeGreaterThan(0);
    expect(lines[1]).toMatch(/^\d+$/);
  });

  it("kernel is an epoch, then /proc/vmstat and /proc/stat", () => {
    const lines = section("kernel");
    const epoch = Number(lines[0]);
    expect(Math.abs(epoch - Date.now() / 1000)).toBeLessThan(600);
    expect(lines.some((l) => l.startsWith("cpu "))).toBe(true);
    expect(lines.some((l) => l.startsWith("ctxt "))).toBe(true);
    expect(lines.some((l) => l.startsWith("pgmajfault "))).toBe(true);
  });
});
