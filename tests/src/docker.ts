import { GenericContainer, Network, StartedTestContainer, Wait, type WaitStrategy, type StartedNetwork } from "testcontainers";
import execaForProbe from "execa";

/**
 * Comma-separated `allowed hosts` string covering loopback plus the
 * three RFC1918 ranges Docker bridges live in. Set this on any nscp
 * server module a container will talk to via `host.docker.internal` —
 * without it, packets arriving from the docker bridge gateway
 * (typically 172.17.0.1) get rejected with the TCP socket closing right
 * after accept, which surfaces in the container as "Connection
 * refused".
 */
export const DOCKER_HOST_ALLOWED_HOSTS =
  "127.0.0.1,::1,172.16.0.0/12,192.168.0.0/16,10.0.0.0/8";

/**
 * `true` when the suite was asked to skip docker-dependent scenarios
 * (set `NSCP_SKIP_DOCKER=1` in the environment). Used by CI pipelines
 * that can run the rest of the suite — including the rest-* scenarios
 * which only need an nscp binary — without a docker daemon available.
 *
 * Reads the env var on every call rather than caching, so test files
 * loaded by Jest pick up the current value even if the env was set
 * after import.
 */
export function skipDocker(): boolean {
  return process.env.NSCP_SKIP_DOCKER === "1";
}

/**
 * Returns `describe` when docker is available, `describe.skip` when
 * `NSCP_SKIP_DOCKER=1`. Use at the top of any scenario that calls
 * `GenericContainer.fromDockerfile`, `dockerRunOnce`, or otherwise
 * needs a docker daemon:
 *
 *   dockerOrSkip()("Icinga integration", () => { ... });
 *
 * Match the existing `maybeDescribe` shape in check_mk-site.test.ts so
 * the two gates can compose:
 *
 *   const enabled = !skipDocker() && process.env.RUN_CMK_SITE_TEST === "1";
 *   const maybeDescribe = enabled ? describe : describe.skip;
 */
export function dockerOrSkip(): jest.Describe {
  return skipDocker() ? describe.skip : describe;
}

/**
 * Returns the extra-hosts list needed so containers can resolve
 * `host.docker.internal` to the actual docker host:
 *
 *   - Native Docker / Docker Desktop: `host-gateway` resolves to the
 *     docker bridge gateway (typically 172.17.0.1) which routes to the
 *     host. Works out of the box.
 *   - Rancher Desktop (and other lima/gvisor-backed engines on WSL):
 *     containers run in a *different* WSL distro than the user's, so
 *     `host-gateway` lands on the container-distro's bridge — NOT the
 *     user's host. Routing back to the host services is provided by
 *     gvisor on a fixed magic IP, 192.168.127.254.
 *
 * Detection happens once per process and is cached.
 */
let cachedHostGateway: string[] | undefined;
export function hostGatewayExtraHosts(): string[] {
  if (cachedHostGateway) return cachedHostGateway;
  let mapping = "host.docker.internal:host-gateway";
  try {
    const r = execaForProbe.sync(
      "docker", ["info", "--format", "{{.OperatingSystem}}"],
      { timeout: 5_000, reject: false },
    );
    if (/rancher desktop/i.test(r.stdout ?? "")) {
      // gvisor's host-reach gateway on Rancher Desktop. See
      // https://rancherdesktop.io/ docs ("Connecting to Host Services
      // from Containers").
      mapping = "host.docker.internal:192.168.127.254";
    }
  } catch { /* probe failure: fall back to host-gateway */ }
  cachedHostGateway = [mapping];
  return cachedHostGateway;
}

export { GenericContainer, Network, Wait };
export type { StartedTestContainer, WaitStrategy, StartedNetwork };

/**
 * Run a thin docker exec wrapper (image must already exist or be referenced
 * by tag) — convenience for the cases where we use `docker run --rm <image>`
 * as a *client* and just want the exit code / stdout, not a long-lived
 * container.
 *
 * Uses `docker run --rm` directly instead of testcontainers to keep
 * one-shot client semantics (testcontainers is designed around
 * long-running containers with wait strategies).
 */
import execa from "execa";
type ExecaReturnValue = execa.ExecaReturnValue;

/**
 * Read every regular file under `dir` inside the container and return the
 * concatenated contents. Use when the host can't read the files directly —
 * e.g. NRDP/PHP writes spool files as www-data with mode 0660, which the
 * test process (running as the developer's uid) cannot open from the host
 * bind mount.
 */
export async function containerReadAll(
  container: StartedTestContainer,
  dir: string,
): Promise<string> {
  const r = await container.exec([
    "sh", "-c",
    `find ${dir} -type f -exec cat {} +`,
  ]);
  return r.output;
}

/**
 * Chmod a path inside a running container so the host can read it from
 * the bind mount. Cheaper than shelling files through `exec` when there
 * are many files (NSCA's results.txt, NRDP's spool dir).
 */
export async function containerChmodReadable(
  container: StartedTestContainer,
  ...paths: string[]
): Promise<void> {
  await container.exec(["chmod", "-R", "a+rX", ...paths]);
}

export async function dockerRunOnce(
  image: string,
  args: string[],
  opts: {
    extraHosts?: string[];
    network?: string;
    env?: Record<string, string>;
    /** `[{ source: hostPath, target: containerPath, ro?: boolean }, …]` */
    bindMounts?: Array<{ source: string; target: string; ro?: boolean }>;
    timeout?: number;
    allowFailure?: boolean;
  } = {},
): Promise<ExecaReturnValue> {
  const dockerArgs: string[] = ["run", "--rm"];
  for (const h of opts.extraHosts ?? []) {
    dockerArgs.push("--add-host", h);
  }
  if (opts.network) dockerArgs.push("--network", opts.network);
  for (const [k, v] of Object.entries(opts.env ?? {})) {
    dockerArgs.push("-e", `${k}=${v}`);
  }
  for (const m of opts.bindMounts ?? []) {
    dockerArgs.push("-v", `${m.source}:${m.target}${m.ro ? ":ro" : ""}`);
  }
  dockerArgs.push(image, ...args);
  return execa("docker", dockerArgs, {
    timeout: opts.timeout ?? 60_000,
    reject: !opts.allowFailure,
    all: true,
  });
}
