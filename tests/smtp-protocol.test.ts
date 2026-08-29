/**
 * SMTP wire conversation and argument handling.
 *
 * smtp-send.test.ts covers delivery against a real aiosmtpd server over
 * STARTTLS and implicit TLS, and needs Docker. This suite covers what the
 * client puts on the wire and how it parses its own arguments, which needs
 * no TLS and no container - so it also runs where Docker is unavailable.
 *
 * The cases here are the ones the protocol is easy to get wrong in ways a
 * unit test cannot see: arguments that only fail over REST, headers that only
 * exist once the message is assembled, and the EHLO name, which is the one
 * attacker-influenced value written straight into a command line.
 */
import * as net from "net";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

/** Everything the fake server saw during one session. */
type Conversation = {
  connected: boolean;
  ehlo?: string;
  mailFrom?: string;
  rcptTo: string[];
  /** The DATA payload: headers, a blank line, then the body. */
  data?: string;
};

/**
 * A minimal one-shot SMTP server that records the conversation. It advertises
 * no STARTTLS and no AUTH, so the client talks to it with security=none and
 * every assertion is about the plain submission flow.
 */
class FakeSmtpServer {
  readonly seen: Conversation = { connected: false, rcptTo: [] };
  private readonly server: net.Server;

  private constructor(server: net.Server) {
    this.server = server;
  }

  static async listen(): Promise<FakeSmtpServer> {
    const server = net.createServer();
    const fake = new FakeSmtpServer(server);
    server.on("connection", (socket) => fake.handle(socket));
    await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
    return fake;
  }

  get port(): number {
    return (this.server.address() as net.AddressInfo).port;
  }

  async close(): Promise<void> {
    await new Promise<void>((resolve) => this.server.close(() => resolve()));
  }

  private handle(socket: net.Socket): void {
    this.seen.connected = true;
    let buffer = "";
    let inData = false;
    let body = "";

    socket.write("220 fake.example.com ESMTP\r\n");
    socket.on("error", () => {
      /* the client hanging up mid-session is a case under test */
    });
    socket.on("data", (chunk) => {
      buffer += chunk.toString("latin1");
      for (let nl = buffer.indexOf("\r\n"); nl !== -1; nl = buffer.indexOf("\r\n")) {
        const line = buffer.slice(0, nl);
        buffer = buffer.slice(nl + 2);

        if (inData) {
          if (line === ".") {
            inData = false;
            this.seen.data = body;
            socket.write("250 2.0.0 Ok: queued\r\n");
          } else {
            // RFC 5321 transparency: a leading dot was doubled on the wire.
            body += (line.startsWith("..") ? line.slice(1) : line) + "\n";
          }
          continue;
        }

        const upper = line.toUpperCase();
        if (upper.startsWith("EHLO ") || upper.startsWith("HELO ")) {
          this.seen.ehlo = line.slice(5);
          socket.write("250-fake.example.com\r\n250 HELP\r\n");
        } else if (upper.startsWith("MAIL FROM:")) {
          this.seen.mailFrom = line.slice("MAIL FROM:".length).replace(/^<|>$/g, "");
          socket.write("250 2.1.0 Ok\r\n");
        } else if (upper.startsWith("RCPT TO:")) {
          this.seen.rcptTo.push(line.slice("RCPT TO:".length).replace(/^<|>$/g, ""));
          socket.write("250 2.1.5 Ok\r\n");
        } else if (upper === "DATA") {
          inData = true;
          body = "";
          socket.write("354 End data with <CR><LF>.<CR><LF>\r\n");
        } else if (upper === "QUIT") {
          socket.write("221 2.0.0 Bye\r\n");
          socket.end();
        } else {
          socket.write("250 2.0.0 Ok\r\n");
        }
      }
    });
  }
}

describe("SMTP protocol and arguments", () => {
  let nscp: NscpInstance;

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  /**
   * Submit to `server` with the given extra arguments. `security` is a
   * parameter rather than something a caller appends, because
   * program_options rejects a repeated option outright ("cannot be specified
   * more than once") - so a second --security would test the parser, not the
   * mode.
   */
  async function submit(
    server: FakeSmtpServer,
    extra: string[],
    opts: { allowFailure?: boolean; security?: string } = {},
  ) {
    return nscp.run(
      [
        "smtp",
        "--host=127.0.0.1",
        `--port=${server.port}`,
        `--security=${opts.security ?? "none"}`,
        "--sender=agent@example.com",
        "--recipient=ops@example.com",
        "--subject",
        "[NSClient++] %source%",
        "--template",
        "body: %source% says %message%",
        "--command",
        "mycheck",
        "--result",
        "0",
        "--message",
        "disk is fine",
        ...extra,
      ],
      { allowFailure: opts.allowFailure ?? false },
    );
  }

  it("puts the envelope, headers and substituted body on the wire", async () => {
    const server = await FakeSmtpServer.listen();
    try {
      const r = await submit(server, []);

      expect(r.exitCode).toBe(0);
      expect(server.seen.mailFrom).toBe("agent@example.com");
      expect(server.seen.rcptTo).toEqual(["ops@example.com"]);
      const data = server.seen.data ?? "";
      expect(data).toContain("From: agent@example.com");
      expect(data).toContain("To: ops@example.com");
      // %source% is the check name, %message% the plugin output.
      expect(data).toContain("Subject: [NSClient++] mycheck");
      expect(data).toContain("body: mycheck says disk is fine");
    } finally {
      await server.close();
    }
  });

  it("sends a Message-ID, and a different one each time", async () => {
    // Mail without a Message-ID is scored as spam, and a repeated one invites
    // receivers to drop the second copy as a duplicate - a lost alert either
    // way. The header is only assembled at submission time, so a unit test on
    // the builder cannot show that it actually reaches the wire.
    const ids: string[] = [];
    for (let i = 0; i < 2; i++) {
      const server = await FakeSmtpServer.listen();
      try {
        expect((await submit(server, [])).exitCode).toBe(0);
        const match = /^Message-ID: (<[^>]+>)$/m.exec(server.seen.data ?? "");
        expect(match).not.toBeNull();
        ids.push(match![1]);
      } finally {
        await server.close();
      }
    }

    expect(ids[0]).toContain("@example.com>");
    expect(ids[0]).not.toEqual(ids[1]);
  });

  it("accepts insecure-skip-verify as one valued token, the way REST passes it", async () => {
    // Declared as a bool_switch this is rejected with "does not take any
    // arguments" - and only on this form, so the bare CLI flag the other
    // suite uses would keep working while every REST caller got an error.
    const server = await FakeSmtpServer.listen();
    try {
      const r = await submit(server, ["--insecure-skip-verify=true"]);

      expect(r.exitCode).toBe(0);
      expect(`${r.stdout}${r.stderr}`).not.toContain("does not take any arguments");
      expect(server.seen.data).toBeDefined();
    } finally {
      await server.close();
    }
  });

  it("accepts --source-host and announces it in EHLO", async () => {
    // Registered by both the shared client options and the module's own
    // reader, this used to be rejected outright as an ambiguous option.
    const server = await FakeSmtpServer.listen();
    try {
      const r = await submit(server, ["--source-host", "agent-box.example.com"]);

      expect(`${r.stdout}${r.stderr}`).not.toContain("ambiguous");
      expect(r.exitCode).toBe(0);
      expect(server.seen.ehlo).toBe("agent-box.example.com");
    } finally {
      await server.close();
    }
  });

  it("lets --ehlo-hostname override the source host", async () => {
    const server = await FakeSmtpServer.listen();
    try {
      const r = await submit(server, [
        "--source-host",
        "agent-box.example.com",
        "--ehlo-hostname",
        "mail-gw.example.com",
      ]);

      expect(r.exitCode).toBe(0);
      expect(server.seen.ehlo).toBe("mail-gw.example.com");
    } finally {
      await server.close();
    }
  });

  it("refuses an EHLO name carrying a command injection, before connecting", async () => {
    // The EHLO argument is written straight into a command line and defaults
    // to the submitting sender's host name, which for a relayed submission
    // comes from the request header. A CRLF there would run commands of the
    // sender's choosing on an authenticated session.
    const server = await FakeSmtpServer.listen();
    try {
      const r = await submit(
        server,
        ["--ehlo-hostname", "agent.example.com\r\nMAIL FROM:<attacker@evil.example>"],
        { allowFailure: true },
      );

      expect(r.exitCode).not.toBe(0);
      expect(`${r.stdout}${r.stderr}`).toContain("EHLO name contains an illegal character");
      // Rejected with the envelope checks, ahead of resolve and connect.
      expect(server.seen.connected).toBe(false);
    } finally {
      await server.close();
    }
  });

  it("fails with a clear message when the CA bundle cannot be loaded", async () => {
    // Continuing here would leave verification pointed at an empty trust
    // store and fail later as an unrelated-looking issuer error.
    const server = await FakeSmtpServer.listen();
    try {
      const r = await submit(server, ["--ca", "/nonexistent/no-such-ca-bundle.pem"], {
        allowFailure: true,
        security: "tls",
      });

      expect(r.exitCode).not.toBe(0);
      expect(`${r.stdout}${r.stderr}`).toContain("failed to load CA bundle");
    } finally {
      await server.close();
    }
  });

  it("treats security=ssl as an alias for tls", async () => {
    // The alias is accepted but was undocumented. The fake server speaks no
    // TLS, so the session still fails - at the handshake, which is what
    // proves the mode parsed rather than being rejected as invalid.
    const server = await FakeSmtpServer.listen();
    try {
      const r = await submit(server, ["--insecure-skip-verify=true"], {
        allowFailure: true,
        security: "ssl",
      });

      const output = `${r.stdout}${r.stderr}`;
      expect(output).not.toContain("invalid security mode");
      expect(output).toContain("TLS handshake");
    } finally {
      await server.close();
    }
  });
});
