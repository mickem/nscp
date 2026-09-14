/**
 * REST legacy query scenarios — migrated from tests/rest/legacy-query.test.ts.
 *
 * Exercises the pre-v1 /query/<command> endpoint that older clients still
 * use. Returns a payload shaped like the protobuf submit_response with the
 * full perf-data array for mock_query, and the canonical OK/WARNING/
 * CRITICAL/UNKNOWN result envelopes for the helper checks.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

// Just enough protobuf wire format to build a QueryRequestMessage by hand:
// the raw /query.pb endpoint takes protobuf bytes and there is no generated
// TypeScript stub in this suite. Only length-delimited (wire type 2) fields
// are needed - every field used below is a string or a sub-message.
const tag = (field: number, wireType = 2): Buffer => varint((field << 3) | wireType);

function varint(value: number): Buffer {
  const bytes: number[] = [];
  let v = value;
  do {
    let b = v & 0x7f;
    v >>>= 7;
    if (v) b |= 0x80;
    bytes.push(b);
  } while (v);
  return Buffer.from(bytes);
}

const lengthDelimited = (field: number, payload: Buffer): Buffer =>
  Buffer.concat([tag(field), varint(payload.length), payload]);

const stringField = (field: number, value: string): Buffer =>
  lengthDelimited(field, Buffer.from(value, "utf8"));

// PB.Common.KeyValue: key = 1, value = 2.
const keyValue = (key: string, value: string): Buffer =>
  Buffer.concat([stringField(1, key), stringField(2, value)]);

// PB.Common.Header: metadata = 8. QueryRequestMessage: header = 1, payload = 2.
// QueryRequestMessage.Request: command = 2, arguments = 4.
function queryRequest(command: string, metadata: Array<[string, string]> = []): Buffer {
  const parts: Buffer[] = [];
  if (metadata.length > 0) {
    const header = Buffer.concat(metadata.map(([k, v]) => lengthDelimited(8, keyValue(k, v))));
    parts.push(lengthDelimited(1, header));
  }
  parts.push(lengthDelimited(2, stringField(2, command)));
  return Buffer.concat(parts);
}

describe("REST query (legacy)", () => {
  let nscp: NscpInstance;
  let key: string | undefined = undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("can login", async () => {
    await request(REST_URL)
      .get("/api/v1/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.user).toEqual("admin");
        expect(response.body.key).toBeDefined();
        key = response.body.key;
      });
  });

  it("can execute query (json)", async () => {
    await request(REST_URL)
      .get("/query/mock_query?a=b&c=d&e=f")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          payload: [
            {
              command: "mock_query",
              lines: [
                {
                  message: "mock_query::a=b,c=d,e=f",
                  perf: [
                    {
                      alias: "a label",
                      float_value: {
                        critical: 30,
                        maximum: 50,
                        minimum: 10,
                        unit: "Z",
                        value: 30,
                        warning: 20,
                      },
                    },
                    {
                      alias: "another label",
                      float_value: {
                        critical: 30,
                        unit: "Z",
                        value: 33,
                        warning: 20,
                      },
                    },
                  ],
                },
              ],
              result: "OK",
            },
          ],
        });
      });
  });

  it("can execute query (json, warning)", async () => {
    await request(REST_URL)
      .get("/query/check_warning?message=this+is+a+message")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          payload: [
            {
              command: "check_warning",
              lines: [
                {
                  message: "this is a message",
                  perf: [],
                },
              ],
              result: "WARNING",
            },
          ],
        });
      });
  });

  it("can execute query (json, critical)", async () => {
    await request(REST_URL)
      .get("/query/check_critical?message=this+is+a+message")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          payload: [
            {
              command: "check_critical",
              lines: [
                {
                  message: "this is a message",
                  perf: [],
                },
              ],
              result: "CRITICAL",
            },
          ],
        });
      });
  });

  it("can execute query (json, unknown)", async () => {
    await request(REST_URL)
      .get("/query/check_unknown?message=this+is+a+message")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          payload: [
            {
              command: "check_unknown",
              lines: [
                {
                  message: "Unknown command(s): check_unknown",
                  perf: [],
                },
              ],
              result: "UNKNOWN",
            },
          ],
        });
      });
  });

  // The raw-protobuf endpoint forwards the caller's message into the core,
  // header included, and the core permission layer reads the calling module
  // and principal out of two header metadata keys. A caller who sets them
  // would pick its own subject and match any allow-list rule written for
  // another module or user, so the endpoint refuses such a request outright.
  it("executes a raw protobuf query that carries no identity metadata", async () => {
    await request(REST_URL)
      .post("/query.pb")
      .set("Authorization", `Bearer ${key}`)
      .set("Content-Type", "application/octet-stream")
      .send(queryRequest("check_ok"))
      .trustLocalhost(true)
      .expect(200);
  });

  it.each([["nscp.caller_plugin_id", "1"] as const, ["nscp.principal", "admin"] as const])(
    "rejects a raw protobuf query that forges %s",
    async (metaKey, metaValue) => {
      await request(REST_URL)
        .post("/query.pb")
        .set("Authorization", `Bearer ${key}`)
        .set("Content-Type", "application/octet-stream")
        .send(queryRequest("check_ok", [[metaKey, metaValue]]))
        .trustLocalhost(true)
        .expect(400);
    },
  );

  it("rejects a body that is not a query request at all", async () => {
    await request(REST_URL)
      .post("/query.pb")
      .set("Authorization", `Bearer ${key}`)
      .set("Content-Type", "application/octet-stream")
      .send(Buffer.from([0xff, 0xff, 0xff, 0xff]))
      .trustLocalhost(true)
      .expect(400);
  });
});
