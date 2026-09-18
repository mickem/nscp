/**
 * A real nsclient-fleet server in a container, plus the slice of its operator
 * API a test needs to provision work for an agent.
 *
 * Everything else in the fleet suites stands up a *fake* server in node: fast,
 * deterministic, and able to serve deliberately malformed responses. What they
 * cannot do is notice when the real server changes its mind about the protocol
 * — the fake happily keeps speaking the old one. That is not hypothetical:
 * the bundle signature moved from covering the bare SHA-256 digest to covering
 * a descriptor of the bundle's identity, every fake in this repo kept passing,
 * and the break only showed up on a live agent.
 *
 * So this fixture runs the genuine article: real enrollment, a real tenant CA,
 * a real Ed25519 bundle signature. It is deliberately thin — the fake-server
 * suites remain where edge cases belong.
 */
import * as crypto from "crypto";
import * as fs from "fs";
import * as https from "https";
import * as os from "os";
import * as path from "path";

import { GenericContainer, Wait, type StartedTestContainer } from "testcontainers";

const FLEET_REPO = "mickem/nsclient-fleet-server";

/**
 * The container's port, published on the host at a fixed number.
 *
 * Fixed rather than ephemeral because the server has to be told its own
 * address before it starts: `BASE_URL` is what agents are handed at enrollment
 * and what the mTLS certificate they pin is issued for, and there is no way to
 * write it after testcontainers has picked a random port. 19443 keeps clear of
 * 9443 (a fleet server someone is running locally) and of every port nscp
 * itself claims (5666, 5667, 5668, 8443, 6556).
 */
export const FLEET_SERVER_HOST_PORT = 19443;
const FLEET_SERVER_CONTAINER_PORT = 9443;

const ADMIN_EMAIL = "admin@fleet.test";
const ADMIN_PASSWORD = "integration-test-password-9f3a";

/** GET a JSON document, following the one redirect the GitHub API may issue. */
function getJson(url: string, headers: Record<string, string>): Promise<any> {
  return new Promise((resolve, reject) => {
    https
      .get(url, { headers }, (res) => {
        if (res.statusCode === 301 || res.statusCode === 302) {
          res.resume();
          const next = res.headers.location;
          if (!next) return reject(new Error(`${url}: redirect without a location`));
          return resolve(getJson(next, headers));
        }
        const chunks: Buffer[] = [];
        res.on("data", (c) => chunks.push(c));
        res.on("end", () => {
          const text = Buffer.concat(chunks).toString("utf8");
          if (res.statusCode !== 200) {
            return reject(new Error(`${url} -> ${res.statusCode}: ${text.slice(0, 300)}`));
          }
          try {
            resolve(JSON.parse(text));
          } catch (e) {
            reject(new Error(`${url}: response was not JSON: ${text.slice(0, 200)}`));
          }
        });
      })
      .on("error", reject);
  });
}

/**
 * The version of the fleet server to test against: the newest GitHub release.
 *
 * Deliberately resolved from *releases* rather than from the published
 * container image. The two can disagree — v0.1.0 was released with binaries on
 * 2026-09-14 while `ghcr.io/mickem/nsclient-fleet:latest` still served
 * 0.0.1-rc.27 from two days earlier, which is a whole protocol behind. Testing
 * the image would have meant testing a server nobody is being asked to run.
 *
 * `NSCP_FLEET_SERVER_VERSION` pins it (e.g. `0.1.0`); `GITHUB_TOKEN` is used
 * when present, since the unauthenticated API allows 60 requests an hour per
 * address and a busy CI runner shares one.
 */
async function resolveFleetVersion(): Promise<string> {
  const pinned = process.env.NSCP_FLEET_SERVER_VERSION;
  if (pinned) return pinned.replace(/^v/, "");

  const headers: Record<string, string> = {
    Accept: "application/vnd.github+json",
    // The API rejects a request without one.
    "User-Agent": "nscp-integration-tests",
  };
  const token = process.env.GITHUB_TOKEN;
  if (token) headers.Authorization = `Bearer ${token}`;

  const release = await getJson(
    `https://api.github.com/repos/${FLEET_REPO}/releases/latest`,
    headers,
  );
  const tag = release?.tag_name;
  if (typeof tag !== "string" || tag.length === 0) {
    throw new Error(`Could not read tag_name from the latest ${FLEET_REPO} release`);
  }
  return tag.replace(/^v/, "");
}

/** The release asset suffix for the architecture the tests are running on. */
function releaseTarget(): string {
  switch (process.arch) {
    case "x64":
      return "x86_64-unknown-linux-musl";
    case "arm64":
      return "aarch64-unknown-linux-musl";
    default:
      throw new Error(`No nsclient-fleet musl release for arch ${process.arch}`);
  }
}

/**
 * A minimal image around the released binary.
 *
 * Not a copy of the server repo's own Dockerfile — that one exists to produce
 * something shippable (unprivileged user, tini, healthcheck, an entrypoint
 * that picks a TLS mode and refuses to lose data). None of that matters for a
 * container that lives for one test and is told exactly how to run, so this
 * stays short enough to read: fetch the binary, check it against the release's
 * SHA256SUMS, point its state at /data.
 *
 * The binary is fully static and carries the frontend, so there is nothing to
 * install beyond a CA bundle.
 */
function dockerfile(): string {
  return [
    "FROM alpine:3.20",
    "ARG FLEET_VERSION",
    "ARG FLEET_TARGET",
    "RUN apk add --no-cache ca-certificates curl",
    "WORKDIR /tmp",
    `RUN curl -fsSL --retry 3 --retry-delay 5 -o nsclient-fleet "https://github.com/${FLEET_REPO}/releases/download/v\${FLEET_VERSION}/nsclient-fleet-\${FLEET_TARGET}"`,
    `RUN curl -fsSL --retry 3 -o SHA256SUMS "https://github.com/${FLEET_REPO}/releases/download/v\${FLEET_VERSION}/SHA256SUMS"`,
    // The sums file lists every target; check only our line, so a sibling
    // asset that is missing or renamed cannot fail an otherwise valid build.
    'RUN cp nsclient-fleet "nsclient-fleet-${FLEET_TARGET}" && grep -F " nsclient-fleet-${FLEET_TARGET}" SHA256SUMS | sha256sum -c - && rm -f "nsclient-fleet-${FLEET_TARGET}"',
    "RUN chmod 0755 nsclient-fleet && mv nsclient-fleet /usr/local/bin/nsclient-fleet && mkdir -p /data",
    "ENV DATABASE_PATH=/data/fleet.db",
    "ENV BUNDLE_DIR=/data/bundles",
    "ENV ACME_CACHE_DIR=/data/acme",
    "ENV MTLS_STATE_DIR=/data",
    "ENV TLS_STATE_DIR=/data",
    // What the shipped entrypoint would export for us when no ACME domain or
    // certificate is configured. The agent pins whatever it is handed at
    // enrollment, so self-signed is not a compromise here.
    "ENV TLS_SELF_SIGNED=true",
    'CMD ["nsclient-fleet"]',
    "",
  ].join("\n");
}

/**
 * The image to run, built on demand and cached by version. Docker skips the
 * download layer on every run after the first for a given release.
 *
 * `NSCP_FLEET_SERVER_IMAGE` bypasses the build entirely, for an image built
 * from a working tree:
 *
 *   docker build --build-arg FLEET_VERSION=0.1.0 -t nsclient-fleet:0.1.0 docker
 *   NSCP_FLEET_SERVER_IMAGE=nsclient-fleet:0.1.0 npx jest fleet-server-live
 */
async function fleetServerContainer(): Promise<GenericContainer> {
  const override = process.env.NSCP_FLEET_SERVER_IMAGE;
  if (override) return new GenericContainer(override);

  const version = await resolveFleetVersion();
  const context = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-fleet-img-"));
  try {
    fs.writeFileSync(path.join(context, "Dockerfile"), dockerfile());
    return await GenericContainer.fromDockerfile(context)
      .withBuildArgs({ FLEET_VERSION: version, FLEET_TARGET: releaseTarget() })
      .build(`nscp-it/nsclient-fleet:${version}`, { deleteOnExit: false });
  } finally {
    fs.rmSync(context, { recursive: true, force: true });
  }
}

/** One HTTP response, with the body left unparsed so a failure can be shown. */
interface RawResponse {
  status: number;
  headers: Record<string, string | string[] | undefined>;
  text: string;
}

/**
 * The operator API, as a test drives it.
 *
 * Requests go over HTTPS with verification off: the container generates a
 * self-signed certificate on first start and there is nothing to establish by
 * checking it — the agent, which *does* care, pins the certificate it is
 * handed at enrollment instead.
 */
export class FleetServerApi {
  private readonly cookies = new Map<string, string>();

  constructor(readonly baseUrl: string) {}

  private request(method: string, path: string, body?: unknown): Promise<RawResponse> {
    const url = new URL(path, this.baseUrl);
    const payload = body === undefined ? undefined : Buffer.from(JSON.stringify(body), "utf8");
    const headers: Record<string, string> = { Accept: "application/json" };
    if (payload) {
      headers["Content-Type"] = "application/json";
      headers["Content-Length"] = String(payload.length);
    }
    if (this.cookies.size > 0) {
      headers["Cookie"] = [...this.cookies].map(([k, v]) => `${k}=${v}`).join("; ");
    }
    // No Sec-Fetch-Site header: the server's CSRF layer refuses a mutating
    // request a *browser* labels cross-site, and deliberately lets through
    // anything that sends no such header at all. A node client is in the same
    // position as curl or the agent.
    return new Promise((resolve, reject) => {
      const req = https.request(
        {
          method,
          hostname: url.hostname,
          port: url.port,
          path: url.pathname + url.search,
          headers,
          rejectUnauthorized: false,
        },
        (res) => {
          const chunks: Buffer[] = [];
          res.on("data", (c) => chunks.push(c));
          res.on("end", () => {
            for (const cookie of (res.headers["set-cookie"] ?? []) as string[]) {
              const [pair] = cookie.split(";");
              const eq = pair.indexOf("=");
              if (eq > 0) this.cookies.set(pair.slice(0, eq).trim(), pair.slice(eq + 1).trim());
            }
            resolve({
              status: res.statusCode ?? 0,
              headers: res.headers,
              text: Buffer.concat(chunks).toString("utf8"),
            });
          });
        },
      );
      req.on("error", reject);
      if (payload) req.write(payload);
      req.end();
    });
  }

  /** Request, insisting on a status and parsing JSON; throws with the body otherwise. */
  private async json(
    method: string,
    path: string,
    body?: unknown,
    expect: number[] = [200, 201, 204],
  ): Promise<any> {
    const r = await this.request(method, path, body);
    if (!expect.includes(r.status)) {
      throw new Error(`${method} ${path} -> ${r.status}: ${r.text.slice(0, 500)}`);
    }
    if (r.text.length === 0) return undefined;
    try {
      return JSON.parse(r.text);
    } catch {
      return r.text;
    }
  }

  /**
   * Sign in as the on-prem admin. `ON_PREM=true` seeds a `default` tenant and
   * this one account at startup, so there is no signup or magic link to walk.
   *
   * Answers 303 (it is the form post a browser makes, and it redirects into
   * the UI); the session cookie rides on that response like any other.
   */
  async login(): Promise<void> {
    await this.json(
      "POST",
      "/api/auth/login",
      { email: ADMIN_EMAIL, password: ADMIN_PASSWORD },
      [200, 204, 303],
    );
  }

  /** Provision a host, returning its id and single-use bootstrap token. */
  async createHost(): Promise<{ hostId: string; bootstrapToken: string }> {
    const body = await this.json("POST", "/api/hosts", {});
    return { hostId: body.host_id, bootstrapToken: body.bootstrap_token };
  }

  /** Set an operator-sourced tag, which is what group selectors match on here. */
  async setHostTag(hostId: string, key: string, value: string): Promise<void> {
    await this.json("PUT", `/api/hosts/${hostId}/tags/${encodeURIComponent(key)}`, { value });
  }

  async getHost(hostId: string): Promise<any> {
    return this.json("GET", `/api/hosts/${hostId}`);
  }

  /**
   * Build a bundle server-side from a config fragment. The server writes the
   * zip (`bundle.toml` + `config.json`) and signs it with the tenant key — so
   * the signature the agent verifies is genuinely the server's, which is the
   * entire reason this fixture exists.
   */
  async composeBundle(name: string, version: string, configJson: unknown): Promise<any> {
    return this.json("POST", "/api/bundles/compose", {
      name,
      version,
      config_json: configJson,
    });
  }

  async createGroup(name: string, selector: unknown): Promise<any> {
    return this.json("POST", "/api/groups", { name, selector });
  }

  async assignBundle(groupId: string, bundleId: string, priority = 100): Promise<void> {
    await this.json("POST", `/api/groups/${groupId}/bundles`, {
      bundle_id: bundleId,
      priority,
    });
  }
}

export interface StartedFleetServer {
  /** The address agents dial and the API is reached on. */
  url: string;
  api: FleetServerApi;
  container: StartedTestContainer;
  /** Container stdout+stderr, for a failure message worth reading. */
  logs(): Promise<string>;
  stop(): Promise<void>;
}

/**
 * Build (or pull) and start the fleet server, waiting until `/healthz` answers
 * — which it only does once the database is open, so it is a real readiness
 * signal rather than "the process exists".
 */
export async function startFleetServer(): Promise<StartedFleetServer> {
  const url = `https://127.0.0.1:${FLEET_SERVER_HOST_PORT}`;
  const image = await fleetServerContainer();
  const container = await image
    .withExposedPorts({ container: FLEET_SERVER_CONTAINER_PORT, host: FLEET_SERVER_HOST_PORT })
    .withEnvironment({
      // Encrypts the tenant CA and host overrides. Throwaway: the container's
      // volume dies with the test.
      MASTER_KEY: crypto.randomBytes(32).toString("base64"),
      // Said explicitly rather than left to the image's default, which has
      // moved between releases (0.0.1-rc.27 serves 8443; later builds use 9443
      // to stop colliding with the NSClient++ web UI).
      LISTEN_HTTPS: `0.0.0.0:${FLEET_SERVER_CONTAINER_PORT}`,
      // Must match what the agent will dial, since it decides both the URL
      // handed out at enrollment and the SAN of the pinned certificate.
      BASE_URL: url,
      // And say it again explicitly, because BASE_URL alone is not enough on
      // every release. 0.0.1-rc.27 derives the agent's URL from the port the
      // server *binds*, not the one BASE_URL advertises, so a published
      // container port ends up handing agents `:9443` — the port inside the
      // container. An agent then dials the host's 9443, which is somebody
      // else's server (a fleet server running locally, say) and fails on the
      // pinned certificate rather than on anything to do with this test.
      // MTLS_URL is the documented override and is honoured by every release.
      MTLS_URL: url,
      ON_PREM: "true",
      ON_PREM_ADMIN_EMAIL: ADMIN_EMAIL,
      ON_PREM_ADMIN_PASSWORD: ADMIN_PASSWORD,
    })
    .withWaitStrategy(
      Wait.forHttp("/healthz", FLEET_SERVER_CONTAINER_PORT).usingTls().allowInsecure(),
    )
    // Generous: the server generates a CA and two certificates before it
    // answers.
    .withStartupTimeout(180_000)
    .start();

  const api = new FleetServerApi(url);
  return {
    url,
    api,
    container,
    logs: async () => {
      const stream = await container.logs();
      return new Promise<string>((resolve) => {
        let out = "";
        stream.on("data", (c) => (out += c.toString()));
        stream.on("end", () => resolve(out));
        // Never leave a failing test hanging on a log read.
        setTimeout(() => resolve(out), 5_000);
      });
    },
    stop: async () => {
      await container.stop();
    },
  };
}
