import {beforeAll, describe, expect, it} from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";
describe("log", () => {
    let key = undefined;
    let index = 0;

    beforeAll(async () => {
        await request(URL)
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


    it("clear_log", async () => {
        await request(URL)
            .delete("/api/v2/logs")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toEqual({count: 0});
            });
    });

    it("reset status", async () => {
        await request(URL)
            .delete("/api/v2/logs/status")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toEqual("");
            });
    });

    it("log_status", async () => {
        await request(URL)
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
        await request(URL)
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
                await request(URL)
                    .get("/api/v2/logs")
                    .set("Authorization", `Bearer ${key}`)
                    .query({per_page: 200, page: 1})
                    .trustLocalhost(true)
                    .expect(200)
                    .then((response) => {
                        expect(response.body).toBeDefined();
                        expect(Array.isArray(response.body)).toBe(true);
                        const logs = response.body;
                        const testLog = logs.find((log: any) => log.message === "001: This is a test log message from automated tests");
                        if (!testLog) {
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
                // wait 500ms before retrying
                await new Promise((resolve) => setTimeout(resolve, 500));
                continue;
            }
            break;
        }
    });

    it("fetch_logs_since", async () => {
        await request(URL)
            .get("/api/v2/logs/since")
            .set("Authorization", `Bearer ${key}`)
            .query({per_page: 200, page: 1})
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(Array.isArray(response.body)).toBe(true);
                const logs = response.body;
                const testLog = logs.find((log: any) => log.message === "001: This is a test log message from automated tests");
                expect(testLog).toBeDefined();
                expect(testLog.level).toBe("info");
                expect(testLog.file).toBe("rest.test.ts");
                expect(testLog.line).toBe(123);
                index = parseInt(response.headers['x-log-index'], 10);
            });

    });


    it("add_another_entry", async () => {
        await request(URL)
            .post("/api/v2/logs")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .send({
                level: "error",
                message: "002: This is a test error log message from automated tests",
                file: "rest.test.ts",
                line: 456,
            })
    });


    it("fetch_logs_since", async () => {
        for (let attempt = 0; attempt < 5; attempt++) {
            try {
                await request(URL)
                    .get("/api/v2/logs/since")
                    .set("Authorization", `Bearer ${key}`)
                    .query({per_page: 200, page: 1, since: index})
                    .trustLocalhost(true)
                    .expect(200)
                    .then((response) => {
                        expect(response.body).toBeDefined();
                        expect(Array.isArray(response.body)).toBe(true);
                        const logs = response.body;
                        const testLog1 = logs.find((log: any) => log.message === "001: This is a test log message from automated tests");
                        expect(testLog1).not.toBeDefined();
                        const testLog2 = logs.find((log: any) => log.message === "002: This is a test error log message from automated tests");

                        expect(testLog2.level).toBe("error");
                        expect(testLog2.file).toBe("rest.test.ts");
                        expect(testLog2.line).toBe(456);
                    });
            } catch (err) {
                if (attempt === 4) {
                    throw err;
                }
                // wait 500ms before retrying
                await new Promise((resolve) => setTimeout(resolve, 500));
                continue;
            }
            break;
        }
    });

    it("clear_log", async () => {
        await request(URL)
            .delete("/api/v2/logs")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toEqual({count: 0});
            });
    });

    it("ensure_log_was_cleared", async () => {
        await request(URL)
            .get("/api/v2/logs")
            .set("Authorization", `Bearer ${key}`)
            .query({per_page: 200, page: 1})
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(Array.isArray(response.body)).toBe(true);
                const logs = response.body;
                const testLog = logs.find((log: any) => log.message === "001: This is a test log message from automated tests");
                expect(testLog).not.toBeDefined();
            });
        });
});
