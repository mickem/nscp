/**
 * REST log scenarios — migrated from tests/rest/log.test.ts.
 *
 * Covers /api/v2/logs (GET list, POST add, DELETE clear) and
 * /api/v2/logs/since (incremental fetch by x-log-index). The fetch
 * cases retry up to 5 × 500 ms because log additions go through the
 * service's async log pipeline before the GET can observe them.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

describe("REST log", () => {
  let nscp: NscpInstance;
  let key: string | undefined = undefined;
  let index = 0;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);

    await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.user).toEqual("admin");
        expect(response.body.key).toBeDefined();
        key = response.body.key;
      });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("clear_log", async () => {
    await request(REST_URL)
      .delete("/api/v2/logs")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toEqual({ count: 0 });
      });
  });

  it("reset status", async () => {
    await request(REST_URL)
      .delete("/api/v2/logs/status")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toEqual("");
      });
  });

  it("log_status", async () => {
    await request(REST_URL)
      .get("/api/v2/logs/status")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.errors).toBeDefined();
        expect(response.body.last_error).toBeDefined();
      });
  });

  it("add_log", async () => {
    await request(REST_URL)
      .post("/api/v2/logs")
      .set("Authorization", `Bearer ${key}`)
      .send({
        level: "info",
        message: "001: This is a test log message from automated tests",
        file: "rest.test.ts",
        line: 123,
      })
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toEqual("");
      });
  });

  it("fetch_logs", async () => {
    for (let attempt = 0; attempt < 5; attempt++) {
      try {
        await request(REST_URL)
          .get("/api/v2/logs")
          .set("Authorization", `Bearer ${key}`)
          .query({ per_page: 200, page: 1 })
          .trustLocalhost(true)
          .expect(200)
          .then((response) => {
            expect(response.body).toBeDefined();
            expect(Array.isArray(response.body)).toBe(true);
            const logs = response.body;
            const testLog = logs.find(
              (log: { message: string }) =>
                log.message === "001: This is a test log message from automated tests",
            );
            if (!testLog) {
              // eslint-disable-next-line no-console
              console.log("Logs returned:", logs);
            }
            expect(testLog).toBeDefined();
            expect(testLog.level).toBe("info");
            expect(testLog.file).toBe("rest.test.ts");
            expect(testLog.line).toBe(123);
          });
      } catch (err) {
        if (attempt === 4) {
          throw err;
        }
        await new Promise((resolve) => setTimeout(resolve, 500));
        continue;
      }
      break;
    }
  });

  it("fetch_logs_since (first read records the index)", async () => {
    await request(REST_URL)
      .get("/api/v2/logs/since")
      .set("Authorization", `Bearer ${key}`)
      .query({ per_page: 200, page: 1 })
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(Array.isArray(response.body)).toBe(true);
        const logs = response.body;
        const testLog = logs.find(
          (log: { message: string }) =>
            log.message === "001: This is a test log message from automated tests",
        );
        expect(testLog).toBeDefined();
        expect(testLog.level).toBe("info");
        expect(testLog.file).toBe("rest.test.ts");
        expect(testLog.line).toBe(123);
        index = parseInt(response.headers["x-log-index"], 10);
      });
  });

  it("add_another_entry", async () => {
    await request(REST_URL)
      .post("/api/v2/logs")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .send({
        level: "error",
        message: "002: This is a test error log message from automated tests",
        file: "rest.test.ts",
        line: 456,
      });
  });

  it("fetch_logs_since (incremental — only new entry)", async () => {
    for (let attempt = 0; attempt < 5; attempt++) {
      try {
        await request(REST_URL)
          .get("/api/v2/logs/since")
          .set("Authorization", `Bearer ${key}`)
          .query({ per_page: 200, page: 1, since: index })
          .trustLocalhost(true)
          .expect(200)
          .then((response) => {
            expect(response.body).toBeDefined();
            expect(Array.isArray(response.body)).toBe(true);
            const logs = response.body;
            const testLog1 = logs.find(
              (log: { message: string }) =>
                log.message === "001: This is a test log message from automated tests",
            );
            expect(testLog1).not.toBeDefined();
            const testLog2 = logs.find(
              (log: { message: string }) =>
                log.message === "002: This is a test error log message from automated tests",
            );

            expect(testLog2.level).toBe("error");
            expect(testLog2.file).toBe("rest.test.ts");
            expect(testLog2.line).toBe(456);
          });
      } catch (err) {
        if (attempt === 4) {
          throw err;
        }
        await new Promise((resolve) => setTimeout(resolve, 500));
        continue;
      }
      break;
    }
  });

  it("clear_log (final)", async () => {
    await request(REST_URL)
      .delete("/api/v2/logs")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toEqual({ count: 0 });
      });
  });

  it("ensure_log_was_cleared", async () => {
    await request(REST_URL)
      .get("/api/v2/logs")
      .set("Authorization", `Bearer ${key}`)
      .query({ per_page: 200, page: 1 })
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(Array.isArray(response.body)).toBe(true);
        const logs = response.body;
        const testLog = logs.find(
          (log: { message: string }) =>
            log.message === "001: This is a test log message from automated tests",
        );
        expect(testLog).not.toBeDefined();
      });
  });
});
