/**
 * Web/application server status checks against the real servers: Apache
 * httpd's mod_status, NGINX's stub_status, PHP-FPM's pm.status_path (behind
 * an nginx front door) and Tomcat's manager status XML, each running in its
 * official container image and queried through one-shot client commands
 * (`nscp client --module CheckNet --boot --query <cmd> url=...`).
 *
 * The sibling cases in checknet-commands.test.ts feed the parsers canned
 * fixture pages, which proves the parsing; this suite proves the fixtures
 * still look like what the real servers emit today - a format drift in
 * mod_status or the manager XML fails here first. Values are live, so the
 * assertions pin structure (the rendered syntax and perf keys) and use
 * always-true/always-false thresholds where a deterministic status is needed.
 *
 * Client-query output carries no status-word prefix, but every one of these
 * checks renders `${status}` in its top-syntax, so OK/WARNING/CRITICAL is
 * still visible in the message.
 */
import * as path from "path";
import {
  GenericContainer,
  NscpInstance,
  Wait,
  dockerOrSkip,
  trackContainerLogs,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(600_000);

dockerOrSkip()("Web server status checks against live servers", () => {
  let nscp: NscpInstance;

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  /** Run a CheckNet query and return the combined output. */
  async function query(command: string, args: string[]): Promise<string> {
    const r = await nscp.run(["client", "--module", "CheckNet", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  async function startServer(dockerfile: string, tag: string, port: number, wait = Wait.forListeningPorts()): Promise<StartedTestContainer> {
    const image = await GenericContainer.fromDockerfile(path.resolve(__dirname), `Dockerfiles/${dockerfile}`).build(tag, {
      deleteOnExit: false,
    });
    return trackContainerLogs(await image.withExposedPorts(port).withWaitStrategy(wait).start(), tag);
  }

  describe("Apache mod_status", () => {
    let server: StartedTestContainer;
    let url: string;

    beforeAll(async () => {
      server = await startServer("apache-status.Dockerfile", "apache_status_server", 80);
      url = `http://127.0.0.1:${server.getMappedPort(80)}/server-status`;
    });

    afterAll(async () => {
      await server?.stop();
    });

    it("check_apache_status reads a real mod_status page", async () => {
      const out = await query("check_apache_status", [`url=${url}`]);
      // ExtendedStatus gives real worker and rate numbers; a fresh httpd has
      // at least the worker serving this very request.
      expect(out).toMatch(/OK: ok: \d+ busy and \d+ idle workers, [\d.e+-]+ req\/s, uptime \d+s/);
      expect(out).toMatch(/'[^']*busy_workers'=\d+/);
      expect(out).toMatch(/'[^']*idle_workers'=\d+/);
    });

    it("check_apache_status feeds real values into thresholds", async () => {
      // busy_workers >= 0 is always true on a live server, so WARNING here
      // proves the parsed numbers reach the threshold engine.
      const out = await query("check_apache_status", [`url=${url}`, "warning=busy_workers >= 0"]);
      expect(out).toMatch(/WARNING/);
    });
  });

  describe("NGINX stub_status", () => {
    let server: StartedTestContainer;
    let url: string;

    beforeAll(async () => {
      server = await startServer("nginx-status.Dockerfile", "nginx_status_server", 8080);
      url = `http://127.0.0.1:${server.getMappedPort(8080)}/stub_status`;
    });

    afterAll(async () => {
      await server?.stop();
    });

    it("check_nginx_status reads a real stub_status page", async () => {
      const out = await query("check_nginx_status", [`url=${url}`]);
      // The connection fetching this page counts itself: active >= 1.
      expect(out).toMatch(/OK: ok: \d+ active \(\d+ reading, \d+ writing, \d+ waiting\)/);
      expect(out).toMatch(/'[^']*active'=\d+/);
    });

    it("check_nginx_status feeds real values into thresholds", async () => {
      const out = await query("check_nginx_status", [`url=${url}`, "warning=active >= 1"]);
      expect(out).toMatch(/WARNING/);
    });
  });

  describe("PHP-FPM status page", () => {
    let server: StartedTestContainer;
    let url: string;

    beforeAll(async () => {
      server = await startServer("phpfpm-status.Dockerfile", "phpfpm_status_server", 8080);
      url = `http://127.0.0.1:${server.getMappedPort(8080)}/status`;
    });

    afterAll(async () => {
      await server?.stop();
    });

    it("check_phpfpm_status reads a real status page", async () => {
      const out = await query("check_phpfpm_status", [`url=${url}`]);
      // Default warning is `listen_queue > 0`; an idle pool has an empty
      // queue, so a live fetch lands on OK with the pool's real name.
      expect(out).toMatch(/OK: ok: pool www: \d+ active, \d+ idle, 0 queued/);
      expect(out).toMatch(/'www_active_processes'=\d+/);
      expect(out).toMatch(/'www_idle_processes'=\d+/);
    });

    it("check_phpfpm_status feeds real values into thresholds", async () => {
      // The dynamic pool starts with idle children waiting, so this fires.
      const out = await query("check_phpfpm_status", [`url=${url}`, "warning=idle_processes >= 1"]);
      expect(out).toMatch(/WARNING/);
    });
  });

  describe("Tomcat manager status", () => {
    let server: StartedTestContainer;
    let url: string;

    beforeAll(async () => {
      // Ports open before the manager app finishes deploying; wait for
      // Catalina's startup line so the first query cannot race a 404.
      server = await startServer("tomcat-status.Dockerfile", "tomcat_status_server", 8080, Wait.forLogMessage(/Server startup in/));
      url = `http://127.0.0.1:${server.getMappedPort(8080)}/manager/status`;
    });

    afterAll(async () => {
      await server?.stop();
    });

    it("check_tomcat_status reads the real manager XML with basic auth", async () => {
      const out = await query("check_tomcat_status", [`url=${url}`, "username=status", "password=tomcat-status"]);
      // One record per connector; a stock Tomcat 10 has the http-nio one.
      expect(out).toMatch(/OK: .*http-nio[^ ]* ok: \d+\/\d+ threads busy/);
      expect(out).toMatch(/'.*threads_busy'=\d+/);
    });

    it("check_tomcat_status reports the HTTP code when authentication is missing", async () => {
      // No credentials: the manager answers 401, the default critical
      // expression (`result != 'ok' or ...`) turns that into CRITICAL with
      // the code in the record - the documented failure contract, produced
      // by a real server rather than a scripted response.
      const out = await query("check_tomcat_status", [`url=${url}`]);
      expect(out).toMatch(/CRITICAL/);
      expect(out).toMatch(/http_401/);
    });
  });
});
