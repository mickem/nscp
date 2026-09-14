/**
 * The client host-override guard, seen from the NRDP module over the one-shot
 * client-query path (no server, no docker, nothing on the wire): a request may
 * not hand a credentialed target's token to a proxy of its own choosing.
 *
 * `proxy=` never moves the destination, so the address comparison the guard
 * was built on could not see it; the request went out through the caller's
 * proxy with the configured token inside. The cases below pin the refusal, the
 * REST-style `key=value` spelling, and the three documented ways around it.
 */
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const TARGET_PATH = "/settings/NRDP/client/targets/default";
// Port 1 on localhost refuses immediately, so a request that is *not* refused
// by the guard fails fast on the connection instead of hanging on a timeout.
const CONFIGURED_ADDRESS = "https://127.0.0.1:1/nrdp/";
const CONFIGURED_PROXY = "http://127.0.0.1:1/";
const CALLER_PROXY = "http://127.0.0.2:1/";

describe("NRDPClient — a request-chosen proxy is guarded like a request-chosen host", () => {
  let nscp: NscpInstance;

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  /** Run one submit_nrdp through the client-query path and return its output. */
  async function submit(...args: string[]) {
    const r = await nscp.run(
      [
        "client",
        "--module",
        "NRDPClient",
        "--boot",
        "--query",
        "submit_nrdp",
        ...args,
        "command=x",
        "result=0",
        "message=x",
      ],
      { allowFailure: true, timeout: 60_000 },
    );
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  it("refuses proxy= against a target that carries a token", async () => {
    await nscp.configure({
      "/modules": { NRDPClient: "enabled" },
      [TARGET_PATH]: { address: CONFIGURED_ADDRESS, token: "s3cret", timeout: "2" },
    });

    const out = await submit(`proxy=${CALLER_PROXY}`);

    expect(out).toMatch(/'default' carries credentials/);
    expect(out).toMatch(/--proxy/);
    expect(out).toMatch(/caller-chosen proxy/);
  });

  it("refuses no-proxy= the same way, since it decides whether the configured proxy is used", async () => {
    await nscp.configure({
      "/modules": { NRDPClient: "enabled" },
      [TARGET_PATH]: {
        address: CONFIGURED_ADDRESS,
        token: "s3cret",
        proxy: CONFIGURED_PROXY,
        timeout: "2",
      },
    });

    const out = await submit("no-proxy=*");

    expect(out).toMatch(/carries credentials/);
    expect(out).toMatch(/--no-proxy/);
  });

  it("lets a request repeat the configured proxy", async () => {
    await nscp.configure({
      "/modules": { NRDPClient: "enabled" },
      [TARGET_PATH]: {
        address: CONFIGURED_ADDRESS,
        token: "s3cret",
        proxy: CONFIGURED_PROXY,
        timeout: "2",
      },
    });

    // Not refused by the guard: the request reaches the (unreachable) proxy
    // and fails on the connection instead.
    const out = await submit(`proxy=${CONFIGURED_PROXY}`);

    expect(out).not.toMatch(/carries credentials/);
    // Past the guard: the submission was attempted and failed on the socket,
    // so a module that never reached the request cannot pass this vacuously.
    expect(out).toMatch(/Socket error/);
  });

  it("lets a request that brings its own token choose a proxy", async () => {
    await nscp.configure({
      "/modules": { NRDPClient: "enabled" },
      [TARGET_PATH]: { address: CONFIGURED_ADDRESS, token: "s3cret", timeout: "2" },
    });

    const out = await submit(`proxy=${CALLER_PROXY}`, "token=mine");

    expect(out).not.toMatch(/carries credentials/);
    // Past the guard: the submission was attempted and failed on the socket,
    // so a module that never reached the request cannot pass this vacuously.
    expect(out).toMatch(/Socket error/);
  });

  it("lets a request choose a proxy when the target allows host override", async () => {
    await nscp.configure({
      "/modules": { NRDPClient: "enabled" },
      [TARGET_PATH]: {
        address: CONFIGURED_ADDRESS,
        token: "s3cret",
        "allow host override": "true",
        timeout: "2",
      },
    });

    const out = await submit(`proxy=${CALLER_PROXY}`);

    expect(out).not.toMatch(/carries credentials/);
    // Past the guard: the submission was attempted and failed on the socket,
    // so a module that never reached the request cannot pass this vacuously.
    expect(out).toMatch(/Socket error/);
  });
});
