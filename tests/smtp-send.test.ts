/**
 * Port of tests/smtp/run-test.bat. Brings up an aiosmtpd test SMTP
 * server that:
 *   - listens plain + STARTTLS on :1025
 *   - listens implicit-TLS on :1465
 *   - accepts AUTH LOGIN / AUTH PLAIN with fixed credentials
 *   - captures every accepted message to /inbox/messages.txt
 *
 * Tests cover plain / STARTTLS / implicit-TLS happy paths, refusal
 * when credentials are presented over a clear-text security level,
 * wrong-password rejection, and CRLF-injection sanitisation in the
 * Subject header.
 */
import * as fs from "fs";
import * as path from "path";
import {
  GenericContainer,
  NscpInstance,
  Wait,
  anyFileContains,
  containerChmodReadable,
  dockerOrSkip,
  trackContainerLogs,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(600_000);

const USERNAME = "alerts@example.com";
const PASSWORD = "change_me";

dockerOrSkip()("SMTP integration", () => {
  let nscp: NscpInstance;
  let server: StartedTestContainer;
  let inboxDir: string;
  let plainPort: number;
  let tlsPort: number;

  beforeAll(async () => {
    nscp = new NscpInstance();
    inboxDir = nscp.scratch("nscp_smtp_test");
    fs.writeFileSync(path.join(inboxDir, "messages.txt"), "");

    const image = await GenericContainer.fromDockerfile(
      path.resolve(__dirname),
      "Dockerfiles/smtp.Dockerfile",
    ).build("smtp_test_server", { deleteOnExit: false });
    server = await trackContainerLogs(
      await image
        .withExposedPorts(1025, 1465)
        .withEnvironment({ SMTP_USERNAME: USERNAME, SMTP_PASSWORD: PASSWORD })
        .withBindMounts([{ source: inboxDir, target: "/inbox", mode: "rw" }])
        .withWaitStrategy(Wait.forListeningPorts())
        .start(),
      "smtp_test_server",
    );
    plainPort = server.getMappedPort(1025);
    tlsPort = server.getMappedPort(1465);
    await new Promise((res) => setTimeout(res, 1500));
  });

  afterAll(async () => {
    await server?.stop();
    await nscp?.stop();
  });

  async function expectInInbox(needle: string): Promise<void> {
    await containerChmodReadable(server, "/inbox");
    await new Promise((r) => setTimeout(r, 500));
    expect(anyFileContains(inboxDir, needle)).toBe(true);
  }

  async function expectNotInInbox(needle: string): Promise<void> {
    await containerChmodReadable(server, "/inbox");
    await new Promise((r) => setTimeout(r, 500));
    expect(anyFileContains(inboxDir, needle)).toBe(false);
  }

  it("Test 1 - plain SMTP, no auth (security=none)", async () => {
    const r = await nscp.run([
      "smtp",
      "--host=127.0.0.1",
      `--port=${plainPort}`,
      "--security=none",
      "--sender=plain@example.com",
      "--recipient=ops@example.com",
      "--subject",
      "T1 plain",
      "--template",
      "T1-body %message%",
      "--command",
      "t1",
      "--result",
      "0",
      "--message",
      "T1-msg",
    ]);
    expect(r.exitCode).toBe(0);
    await expectInInbox("Subject: T1 plain");
    await expectInInbox("T1-body T1-msg");
    await expectInInbox("MAIL_FROM=plain@example.com");
  });

  it("Test 2 - STARTTLS + AUTH (security=starttls)", async () => {
    const r = await nscp.run([
      "smtp",
      "--host=127.0.0.1",
      `--port=${plainPort}`,
      "--security=starttls",
      "--insecure-skip-verify",
      `--username=${USERNAME}`,
      `--password=${PASSWORD}`,
      "--sender=alerts@example.com",
      "--recipient=ops@example.com",
      "--subject",
      "T2 starttls",
      "--template",
      "T2-body %message%",
      "--command",
      "t2",
      "--result",
      "0",
      "--message",
      "T2-msg",
    ]);
    expect(r.exitCode).toBe(0);
    await expectInInbox("Subject: T2 starttls");
    await expectInInbox("T2-body T2-msg");
    await expectInInbox("AUTH=True");
  });

  it("Test 3 - implicit TLS + AUTH (security=tls, port 1465)", async () => {
    const r = await nscp.run([
      "smtp",
      "--host=127.0.0.1",
      `--port=${tlsPort}`,
      "--security=tls",
      "--insecure-skip-verify",
      `--username=${USERNAME}`,
      `--password=${PASSWORD}`,
      "--sender=alerts@example.com",
      "--recipient=ops@example.com",
      "--subject",
      "T3 implicit-tls",
      "--template",
      "T3-body %message%",
      "--command",
      "t3",
      "--result",
      "0",
      "--message",
      "T3-msg",
    ]);
    expect(r.exitCode).toBe(0);
    await expectInInbox("Subject: T3 implicit-tls");
    await expectInInbox("T3-body T3-msg");
  });

  it("Test 4 - credentials with security=none must be refused", async () => {
    const r = await nscp.run(
      [
        "smtp",
        "--host=127.0.0.1",
        `--port=${plainPort}`,
        "--security=none",
        `--username=${USERNAME}`,
        `--password=${PASSWORD}`,
        "--sender=alerts@example.com",
        "--recipient=ops@example.com",
        "--subject",
        "T4 should-not-arrive",
        "--template",
        "should-not-arrive",
        "--command",
        "t4",
        "--result",
        "0",
        "--message",
        "T4-msg",
      ],
      { allowFailure: true },
    );
    expect(r.exitCode).not.toBe(0);
    await expectNotInInbox("T4 should-not-arrive");
  });

  it("Test 5 - wrong password is rejected", async () => {
    const r = await nscp.run(
      [
        "smtp",
        "--host=127.0.0.1",
        `--port=${plainPort}`,
        "--security=starttls",
        "--insecure-skip-verify",
        `--username=${USERNAME}`,
        "--password=wrong-password",
        "--sender=alerts@example.com",
        "--recipient=ops@example.com",
        "--subject",
        "T5 wrong-password",
        "--template",
        "should-not-arrive",
        "--command",
        "t5",
        "--result",
        "0",
        "--message",
        "T5-msg",
      ],
      { allowFailure: true },
    );
    expect(r.exitCode).not.toBe(0);
    await expectNotInInbox("T5 wrong-password");
  });

  it("Test 6 - CRLF injection in subject is sanitised", async () => {
    // Pass an actual CRLF in the subject (Node spawns argv directly,
    // no shell parsing — \r\n in a JS string is two bytes the OS sees).
    const subject = "hi\r\nBcc: evil@example.com";
    const r = await nscp.run([
      "smtp",
      "--host=127.0.0.1",
      `--port=${plainPort}`,
      "--security=starttls",
      "--insecure-skip-verify",
      `--username=${USERNAME}`,
      `--password=${PASSWORD}`,
      "--sender=alerts@example.com",
      "--recipient=ops@example.com",
      "--subject",
      subject,
      "--template",
      "T6-body",
      "--command",
      "t6",
      "--result",
      "0",
      "--message",
      "T6-msg",
    ]);
    expect(r.exitCode).toBe(0);
    // No real Bcc header should have landed on a separate envelope.
    await expectNotInInbox("RCPT_TO=evil@example.com");
  });
});
