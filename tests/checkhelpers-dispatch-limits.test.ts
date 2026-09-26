/**
 * Bounds on what one caller-supplied request may cost the agent.
 *
 * A check that runs other checks - `check_multi`, `check_and_forward`,
 * `check_timeout` - calls back into the core on the same OS thread, so nesting
 * is recursion the caller controls. Nothing used to bound it: every level
 * pushes a full handler frame (protobuf messages, an options_description, the
 * filter machinery), so a deep enough nesting exhausted the thread stack, which
 * on Windows is not a catchable failure - the process died.
 *
 * `check_and_forward` and `check_timeout` are the cheap vectors, because their
 * `arguments=` option passes each inner token through as its own argument: one
 * more level costs one more `arguments=` prefix per token, not a doubling of
 * the whole string the way a quoted `check_multi command="..."` does.
 *
 * Run over the one-shot client-query path (`nscp client --module ... --boot
 * --query`), which needs no web server and still passes `k=v` as single tokens,
 * exactly as REST does. That path prints the raw Nagios message with no status
 * word, so the verdict is read from the exit code (0 OK / 3 UNKNOWN).
 */
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

const OK = 0;

describe("CheckHelpers dispatch limits", () => {
  let nscp: NscpInstance;

  /** Run a CheckHelpers query and return its output and Nagios exit code. */
  async function query(command: string, args: string[] = []): Promise<{ out: string; code: number }> {
    const r = await nscp.run(["client", "--module", "CheckHelpers", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  /**
   * `command=<wrapper>` nested `depth` levels deep, ending in check_ok. Each
   * level prefixes every token of the level below with `arguments=`, which is
   * how the wrapped command receives them - so the whole thing grows with the
   * square of the depth rather than exponentially.
   */
  function nested(wrapper: string, depth: number): string[] {
    let tokens = ["command=check_ok"];
    for (let i = 0; i < depth; i++) {
      tokens = [`command=${wrapper}`, ...tokens.map((t) => `arguments=${t}`)];
    }
    return tokens;
  }

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  it("runs an ordinary check_multi", async () => {
    // The limits must not get in the way of what check_multi is for.
    const { code } = await query("check_multi", ["command=check_ok", "command=check_ok"]);
    expect(code).toBe(OK);
  });

  it("still allows nesting a few levels deep", async () => {
    // A wrapper around a wrapper is depth 2-3, far below the limit. The forward
    // itself has no channel configured in this one-shot process, so the verdict
    // is not OK - what matters is that nothing was refused for nesting and the
    // process came back at all.
    const { out, code } = await query("check_and_forward", nested("check_and_forward", 3));
    expect(out).not.toMatch(/nested/i);
    expect(code).not.toBe(139); // SIGSEGV
  });

  it("refuses a request nested past the dispatch depth limit, and survives it", async () => {
    const { out, code } = await query("check_and_forward", nested("check_and_forward", 30));
    expect(out).toMatch(/nested more than 16 deep/i);
    expect(code).not.toBe(139);

    // Still usable afterwards: the depth counter is per request, not a latch.
    const after = await query("check_multi", ["command=check_ok"]);
    expect(after.code).toBe(OK);
  });

  it("caps how deep check_timeout may nest", async () => {
    // check_timeout runs the wrapped command on a NEW thread, which starts at
    // dispatch depth zero, so the core's guard does not bound it: nesting
    // through check_timeout trades stack frames for threads, one per level.
    const { out } = await query("check_timeout", nested("check_timeout", 15));
    expect(out).toMatch(/nested too deep \(limit 8\)/i);
  });

  it("still allows an ordinary check_timeout", async () => {
    const { code } = await query("check_timeout", ["command=check_ok", "timeout=30"]);
    expect(code).toBe(OK);
  });

  it("caps how many commands one check_multi may fan out to", async () => {
    const args = Array.from({ length: 200 }, () => "command=check_ok");
    const { out } = await query("check_multi", args);
    expect(out).toMatch(/Too many commands/i);
    expect(out).toMatch(/128/);
  });

  it("accepts a fan-out at the limit", async () => {
    const args = Array.from({ length: 128 }, () => "command=check_ok");
    const { out, code } = await query("check_multi", args);
    expect(out).not.toMatch(/Too many commands/i);
    expect(code).toBe(OK);
  });
});
