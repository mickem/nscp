/**
 * Port of tests/nrpe/run-test.bat. Builds the `check_nrpe` client image
 * once (Debian + check-nrpe + 4096-byte payload variant), then walks the
 * same six phases: payload length, NRPE v2, no-TLS, 1-way TLS, 2-way
 * TLS, and CN-based permissions.
 *
 * The certificates that NRPE's TLS phases need are generated via the
 * shared TLS fixture (replaces the openssl-CLI invocations in the bat).
 */
import * as path from "path";
import {
  DOCKER_HOST_ALLOWED_HOSTS,
  GenericContainer,
  NscpInstance,
  bundledLuaScript,
  bundledSecurityFile,
  dockerOrSkip,
  dockerRunOnce,
  generateCertChain,
  hostGatewayExtraHosts,
} from "@fixtures/index";

jest.setTimeout(900_000);

dockerOrSkip()("NRPE integration", () => {
  let nscp: NscpInstance;
  let image: string;
  let certs: ReturnType<typeof generateCertChain>;
  let certDir: string;

  // Note: each phase rewrites the NRPE settings and re-starts nscp
  // because the bat file does. Sharing one instance across phases would
  // be cheaper but the install + restart cycle is part of what's under
  // test (settings persistence across `nrpe install`).
  beforeAll(async () => {
    image = "check_nrpe";
    await GenericContainer.fromDockerfile(
      path.resolve(__dirname),
      "Dockerfiles/nrpe.Dockerfile",
    ).build(image, { deleteOnExit: false });
    nscp = new NscpInstance();
    certDir = nscp.scratch("nrpe_test");
    certs = generateCertChain({
      outDir: certDir,
      signed: {
        server: { commonName: "localhost", isServer: true },
        client: { commonName: "localhost" },
        denied: { commonName: "denied-client" },
      },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  async function installNrpe(
    extra: Record<string, string | boolean> = {},
    perms = false,
  ): Promise<void> {
    await nscp.stop();
    // Use the `--key=value` form for the boolean knobs. The
    // space-separated form `--insecure false` is parsed by boost.po
    // as the flag (defaulting to true) followed by an ignored
    // positional, so the resulting INI ends up with `ssl enabled:
    // none` regardless of what we passed — silently breaking every
    // TLS phase that didn't also pass `--ca`.
    const args = [
      "nrpe",
      "install",
      `--allowed-hosts=${DOCKER_HOST_ALLOWED_HOSTS}`,
      `--insecure=${String(extra.insecure ?? true)}`,
      `--verify=${String(extra.verify ?? "none")}`,
    ];
    if (extra.certificate) args.push(`--certificate=${String(extra.certificate)}`);
    if (extra["certificate-key"])
      args.push(`--certificate-key=${String(extra["certificate-key"])}`);
    if (extra.ca) args.push(`--ca=${String(extra.ca)}`);
    await nscp.run(args);
    // Pin the DH params at the file shipped in the build tree —
    // certificate-path is overridden to a scratch dir per-instance,
    // which doesn't have nrpe_dh_2048.pem generated, and missing-DH
    // would otherwise abort the NRPE server bind.
    const serverSettings: Record<string, string | number | boolean> = {
      dh: bundledSecurityFile("nrpe_dh_2048.pem"),
    };
    if (extra["payload length"]) {
      serverSettings["payload length"] = String(extra["payload length"]);
    }
    await nscp.configure({ "/settings/NRPE/server": serverSettings });
    if (perms) {
      await nscp.configure({
        "/settings/NRPE/server": { "client identity source": "cn" },
        "/settings/permissions": {
          enabled: true,
          "log denials": true,
          "log allows": true,
        },
        // Rule table is fail-closed: with `enabled=true` and no rules,
        // every subject is denied. Allow the localhost-CN client to run
        // the two mock commands the test invokes (query + exit); the
        // denied-client cert has a different CN and so doesn't match
        // this rule, which is exactly what Phase 5 is verifying.
        "/settings/permissions/policies": {
          "NRPEServer:localhost": "mock_query, mock_exit",
        },
      });
    }
    await nscp.run(["lua", "add", "--script", bundledLuaScript("mock")]);
    nscp.start();
    await nscp.waitForPort(5666, { timeoutMs: 30_000 });
    // Restarting `nscp test` between phases leaves gvisor's port
    // mapping briefly stale on Rancher Desktop — the host-side socket
    // is already accepting, but the container-side 192.168.127.254:5666
    // forward still points at the previous instance and refuses
    // SYNs for ~hundreds of ms after the rebind. Give it a beat.
    await new Promise((res) => setTimeout(res, 1500));
  }

  async function shutdownViaMockExit(extraArgs: string[] = []): Promise<void> {
    await dockerRunOnce(
      image,
      [
        "check_nrpe",
        "-H",
        "host.docker.internal",
        "-p",
        "5666",
        "-t5",
        ...extraArgs,
        "-c",
        "mock_exit",
      ],
      {
        extraHosts: hostGatewayExtraHosts(),
        // Always mount the cert dir; harmless for the no-TLS phases since
        // their shutdown doesn't reference any `/test/...` file.
        bindMounts: [{ source: certDir, target: "/test", ro: true }],
        allowFailure: true,
      },
    );
    // Belt-and-braces: ensure the process is reaped.
    await nscp.stop();
  }

  it("Phase 1 - non-standard payload length (4096)", async () => {
    await installNrpe({ "payload length": "4096" });
    try {
      const r1 = await nscp.run([
        "nrpe",
        "--host",
        "127.0.0.1",
        "--insecure",
        "--version",
        "2",
        "--payload-length",
        "4096",
        "--command",
        "mock_query",
      ]);
      expect(r1.exitCode).toBe(0);
      expect(r1.all).toContain("mock_query::");

      const r2 = await dockerRunOnce(
        image,
        ["check_nrpe_4096", "-H", "host.docker.internal", "-p", "5666", "-t5", "-c", "mock_query"],
        { extraHosts: hostGatewayExtraHosts() },
      );
      expect(r2.exitCode).toBe(0);
      expect(r2.all).toContain("mock_query::");
    } finally {
      await shutdownViaMockExit();
    }
  });

  it("Phase 2 - NRPE v2 default payload", async () => {
    await installNrpe();
    try {
      const r1 = await nscp.run([
        "nrpe",
        "--host",
        "127.0.0.1",
        "--insecure",
        "--version",
        "2",
        "--command",
        "mock_query",
      ]);
      expect(r1.exitCode).toBe(0);
      expect(r1.all).toContain("mock_query::");

      const r2 = await dockerRunOnce(
        image,
        ["check_nrpe", "-H", "host.docker.internal", "-p", "5666", "-t5", "-c", "mock_query"],
        { extraHosts: hostGatewayExtraHosts() },
      );
      expect(r2.exitCode).toBe(0);
      expect(r2.all).toContain("mock_query::");
    } finally {
      await shutdownViaMockExit();
    }
  });

  it("Phase 3 - 1-way TLS with no cert verification", async () => {
    await installNrpe({
      insecure: false,
      verify: "none",
      certificate: certs.signed.server.certPath,
      "certificate-key": certs.signed.server.keyPath,
    });
    try {
      const r = await nscp.run(["nrpe", "--host", "127.0.0.1", "--command", "mock_query"]);
      expect(r.exitCode).toBe(0);
      expect(r.all).toContain("mock_query::");

      const rc = await dockerRunOnce(
        image,
        [
          "check_nrpe",
          "-H",
          "host.docker.internal",
          "-p",
          "5666",
          "-t5",
          "--ssl-version",
          "TLSv1.2+",
          "-c",
          "mock_query",
        ],
        { extraHosts: hostGatewayExtraHosts() },
      );
      expect(rc.exitCode).toBe(0);
      expect(rc.all).toContain("mock_query::");
    } finally {
      await shutdownViaMockExit(["--ssl-version", "TLSv1.2+"]);
    }
  });

  it("Phase 4 - 2-way TLS with peer-cert verification", async () => {
    await installNrpe({
      insecure: false,
      verify: "peer-cert",
      certificate: certs.signed.server.certPath,
      "certificate-key": certs.signed.server.keyPath,
      ca: certs.ca.certPath,
    });
    try {
      const r1 = await nscp.run([
        "nrpe",
        "--host",
        "127.0.0.1",
        "--certificate",
        certs.signed.client.certPath,
        "--certificate-key",
        certs.signed.client.keyPath,
        "--command",
        "mock_query",
      ]);
      expect(r1.exitCode).toBe(0);

      const r2 = await dockerRunOnce(
        image,
        [
          "check_nrpe",
          "-H",
          "host.docker.internal",
          "-p",
          "5666",
          "-t5",
          "--ssl-version",
          "TLSv1.2+",
          "--client-cert",
          "/test/client.crt",
          "--key-file",
          "/test/client.key",
          "-c",
          "mock_query",
        ],
        {
          extraHosts: hostGatewayExtraHosts(),
          bindMounts: [{ source: certDir, target: "/test", ro: true }],
        },
      );
      expect(r2.exitCode).toBe(0);
      expect(r2.all).toContain("mock_query::");

      // Without a client cert the server should reject the call
      // (CRITICAL = 2 / UNKNOWN = 3 — accept anything non-zero).
      const r3 = await nscp.run(["nrpe", "--host", "127.0.0.1", "--command", "mock_query"], {
        allowFailure: true,
      });
      expect(r3.exitCode).not.toBe(0);
    } finally {
      await shutdownViaMockExit([
        "--ssl-version",
        "TLSv1.2+",
        "--client-cert",
        "/test/client.crt",
        "--key-file",
        "/test/client.key",
      ]);
    }
  });

  it("Phase 5 - 2-way TLS + CN-based permission gating", async () => {
    await installNrpe(
      {
        insecure: false,
        verify: "peer-cert",
        certificate: certs.signed.server.certPath,
        "certificate-key": certs.signed.server.keyPath,
        ca: certs.ca.certPath,
      },
      /* perms */ true,
    );
    try {
      // Allowed client (CN=localhost)
      const rOk = await dockerRunOnce(
        image,
        [
          "check_nrpe",
          "-H",
          "host.docker.internal",
          "-p",
          "5666",
          "-t5",
          "--ssl-version",
          "TLSv1.2+",
          "--client-cert",
          "/test/client.crt",
          "--key-file",
          "/test/client.key",
          "-c",
          "mock_query",
        ],
        {
          extraHosts: hostGatewayExtraHosts(),
          bindMounts: [{ source: certDir, target: "/test", ro: true }],
        },
      );
      expect(rOk.exitCode).toBe(0);
      expect(rOk.all).toContain("mock_query::");

      // Denied client (CN=denied-client) — TLS succeeds, policy rejects.
      const rDeny = await dockerRunOnce(
        image,
        [
          "check_nrpe",
          "-H",
          "host.docker.internal",
          "-p",
          "5666",
          "-t5",
          "--ssl-version",
          "TLSv1.2+",
          "--client-cert",
          "/test/denied.crt",
          "--key-file",
          "/test/denied.key",
          "-c",
          "mock_query",
        ],
        {
          extraHosts: hostGatewayExtraHosts(),
          bindMounts: [{ source: certDir, target: "/test", ro: true }],
          allowFailure: true,
        },
      );
      expect(rDeny.exitCode).toBe(3);
      expect(rDeny.all).toContain("Permission denied");
    } finally {
      await shutdownViaMockExit([
        "--ssl-version",
        "TLSv1.2+",
        "--client-cert",
        "/test/client.crt",
        "--key-file",
        "/test/client.key",
      ]);
    }
  });
});
