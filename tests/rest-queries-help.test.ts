/**
 * REST query help scenarios.
 *
 * Covers GET /api/v2/queries/<name>/help — the vocabulary of one check: every
 * option it accepts with its default and description, and every filter keyword
 * it offers. It is what the web UI reads to highlight and complete an argument
 * line, the same way the interactive console (`nscp test`) reads the registry
 * for `desc` and `keywords`.
 *
 * Runs its own agent rather than the shared REST fixture: the fixture keeps the
 * check modules switched off, and the interesting answers here are the ones a
 * real filter based check gives. CheckDisk's check_files is the check used
 * because it exists, and is filter based, on every platform.
 */
import request from "supertest";
import { NscpInstance, REST_URL, describeWithModules } from "@fixtures/index";

jest.setTimeout(900_000);

interface Parameter {
  name: string;
  default_value: string;
  required: boolean;
  repeatable: boolean;
  content_type: string;
  short_description: string;
  long_description: string;
}

interface Field {
  name: string;
  short_description: string;
  long_description: string;
}

interface Help {
  name: string;
  keyword_source: string;
  parameters: Parameter[];
  fields: Field[];
}

const named = <T extends { name: string }>(items: T[], name: string): T | undefined =>
  items.find((item) => item.name === name);

// Skipped where the build has no CheckDisk (macOS, until it is ported).
describeWithModules("CheckDisk")("REST query help", () => {
  let nscp: NscpInstance;
  let key: string | undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        WEBServer: "enabled",
        CheckDisk: "enabled",
        CheckHelpers: "enabled",
      },
      "/settings/default": {
        password: "default-password",
        "allowed hosts": "127.0.0.1,::1",
      },
      // An alias declares no filter keywords of its own; the help endpoint has
      // to follow it to the command it stands for, which is the only place
      // they live.
      "/settings/check helpers/alias": {
        files_alias: "check_files",
      },
      "/settings/WEB/server/roles": {
        full: "*",
        // Deliberately without queries.get: the help is part of describing a
        // query, so it is gated by the same grant and this role must not see
        // it even though it may run the check.
        runner: "public,login.get,queries.execute",
      },
      "/settings/WEB/server/users/admin": {
        role: "full",
        password: "default-password",
      },
      "/settings/WEB/server/users/runner": {
        role: "runner",
        password: "runner-password",
      },
    });
    await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });

    await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        key = response.body.key;
      });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  const help = async (query: string): Promise<Help> => {
    const response = await request(REST_URL)
      .get(`/api/v2/queries/${query}/help`)
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    return response.body as Help;
  };

  it("requires authentication", async () => {
    await request(REST_URL).get("/api/v2/queries/check_files/help").trustLocalhost(true).expect(403);
  });

  it("is gated by queries.get, not by queries.execute", async () => {
    const login = await request(REST_URL)
      .get("/api/v2/login")
      .auth("runner", "runner-password")
      .trustLocalhost(true)
      .expect(200);
    await request(REST_URL)
      .get("/api/v2/queries/check_files/help")
      .set("Authorization", `Bearer ${login.body.key}`)
      .trustLocalhost(true)
      .expect(403);
  });

  it("answers 404 for a command that does not exist", async () => {
    await request(REST_URL)
      .get("/api/v2/queries/check_no_such_thing/help")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(404);
  });

  it("lists the options the check accepts, with defaults and descriptions", async () => {
    const body = await help("check_files");
    expect(body.name).toEqual("check_files");
    expect(body.parameters.length).toBeGreaterThan(0);

    const filter = named(body.parameters, "filter");
    expect(filter).toBeDefined();
    expect(filter!.content_type).toEqual("string");
    expect(filter!.short_description.length).toBeGreaterThan(0);
    // The marker that folds an option shared by every filter check out of the
    // per-command reference page - and, in the web UI, out of the list of
    // options this check defines itself. The client splits on it, so it has to
    // survive the trip.
    expect(filter!.long_description).toMatch(/Common option for all filter checks\.$/);

    // A flag reports itself as one even though it takes a token - it is a
    // value<bool> with an implicit value, because that is how REST spells a
    // flag. The client keys off this to complete it as `show-all=true`; were
    // it to read as a string the completion would produce a bare `show-all=`
    // and REST would reject it. The default comes along too: unlike a switch,
    // a flag declared this way has one.
    const showAll = named(body.parameters, "show-all");
    expect(showAll).toBeDefined();
    expect(showAll!.content_type).toEqual("bool");
    expect(showAll!.default_value).toEqual("false");

    // And a switch, which takes no token at all: bool with no default.
    const helpOption = named(body.parameters, "help");
    expect(helpOption).toBeDefined();
    expect(helpOption!.content_type).toEqual("bool");
    expect(helpOption!.default_value).toEqual("");
  });

  it("lists the filter keywords the check offers, functions marked as such", async () => {
    const body = await help("check_files");
    expect(body.keyword_source).toEqual("check_files");
    expect(body.fields.length).toBeGreaterThan(0);

    const names = body.fields.map((f) => f.name);
    // A keyword of this check, and one of the generic summary keywords every
    // filter check has. Both must be there: the first is what the highlighter
    // accepts as a name, the second is what it must not call a typo.
    expect(names).toEqual(expect.arrayContaining(["filename", "count"]));

    expect(named(body.fields, "count")!.long_description).toMatch(/Common option for all checks\.$/);
  });

  it("marks a filter function as one, the way the registry spells it", async () => {
    // The registry spells a filter function with a trailing "()", which is the
    // only thing that tells a function from a variable. Passing it through
    // verbatim is what lets the client paint `convert_bytes(free)` as a call
    // and a misspelled one as a mistake. check_drivesize registers the shared
    // format functions; check_files registers none, so this is asked of the
    // check that has some.
    const body = await help("check_drivesize");
    const names = body.fields.map((f) => f.name);
    expect(names).toEqual(expect.arrayContaining(["convert_bytes()"]));
    expect(names).toEqual(expect.arrayContaining(["free"]));
  });

  it("follows an alias to the keywords of the command it stands for", async () => {
    const body = await help("files_alias");
    expect(body.name).toEqual("files_alias");
    expect(body.keyword_source).toEqual("check_files");
    expect(body.fields.map((f) => f.name)).toEqual(expect.arrayContaining(["filename"]));
    expect(body.parameters.map((p) => p.name)).toEqual(expect.arrayContaining(["filter"]));
  });

  it("answers for a command that is not filter based without pretending it is", async () => {
    // check_critical takes a message and returns it; it has options but no
    // filter keywords at all. An empty list is the honest answer, and it is
    // what tells the client to say nothing about the names in a filter rather
    // than to call them all wrong.
    const body = await help("check_critical");
    expect(body.fields).toEqual([]);
    expect(body.parameters.length).toBeGreaterThan(0);
  });
});
