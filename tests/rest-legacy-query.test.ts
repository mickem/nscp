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
});
