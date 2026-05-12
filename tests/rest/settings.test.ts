import { beforeAll, describe, expect, it } from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";

// Tests for the settings controller — primarily covering the DELETE method
// added in commit aad7c3f8 alongside the existing GET / PUT shape. The
// controller exposes /api/v{1,2}/settings:
//   GET    /<path>            — fetch keys under a path (grant: settings.get)
//   GET    /descriptions/...  — fetch metadata for a path (grant: settings.get)
//   GET    /status            — pending-change status (grant: settings.get)
//   GET    /diff              — pending-change diff (grant: settings.get)
//   POST   /command           — apply a settings command (grant: settings.put)
//   PUT    /<path>            — write keys (grant: settings.put)
//   DELETE /<path>            — remove keys (grant: settings.put)
//
// The tests roundtrip a write/read/delete cycle under a dedicated test path
// so they don't perturb anything the rest of the suite relies on.
describe("settings controller", () => {
    let key: string | undefined;
    const TEST_PATH = "/settings/rest-test/scratch";

    beforeAll(async () => {
        await request(URL)
            .get("/api/v2/login")
            .auth("admin", "default-password")
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                key = response.body.key;
            });
    });

    describe("auth gating", () => {
        it("GET /api/v2/settings/... requires authentication", async () => {
            await request(URL)
                .get(`/api/v2/settings${TEST_PATH}`)
                .trustLocalhost(true)
                .expect(403);
        });

        it("PUT /api/v2/settings/... requires authentication", async () => {
            await request(URL)
                .put(`/api/v2/settings${TEST_PATH}`)
                .send([{ key: "foo", value: "bar" }])
                .trustLocalhost(true)
                .expect(403);
        });

        it("DELETE /api/v2/settings/... requires authentication", async () => {
            await request(URL)
                .delete(`/api/v2/settings${TEST_PATH}`)
                .trustLocalhost(true)
                .expect(403);
        });
    });

    describe("GET /api/v2/settings/status", () => {
        it("returns a status object", async () => {
            await request(URL)
                .get("/api/v2/settings/status")
                .set("Authorization", `Bearer ${key}`)
                .trustLocalhost(true)
                .expect(200)
                .then((response) => {
                    expect(response.body).toBeDefined();
                });
        });
    });

    describe("PUT then GET round-trip", () => {
        it("writes a key via PUT", async () => {
            await request(URL)
                .put(`/api/v2/settings${TEST_PATH}`)
                .set("Authorization", `Bearer ${key}`)
                .send([{ key: "marker", value: "hello-from-rest-test" }])
                .trustLocalhost(true)
                .expect(200)
                .then((response) => {
                    expect(response.body.status).toEqual("success");
                    expect(response.body.keys).toEqual(1);
                });
        });

        it("reads the key back via GET", async () => {
            await request(URL)
                .get(`/api/v2/settings${TEST_PATH}`)
                .set("Authorization", `Bearer ${key}`)
                .trustLocalhost(true)
                .expect(200)
                .then((response) => {
                    expect(Array.isArray(response.body)).toBe(true);
                    const node = response.body.find((n: any) => n.key === "marker");
                    expect(node).toBeDefined();
                    expect(node.value).toEqual("hello-from-rest-test");
                });
        });
    });

    describe("DELETE method (commit aad7c3f8)", () => {
        it("removes a key via DELETE with a body", async () => {
            // Ensure the key exists first so the delete has something to act on.
            await request(URL)
                .put(`/api/v2/settings${TEST_PATH}`)
                .set("Authorization", `Bearer ${key}`)
                .send([{ key: "to-delete", value: "transient" }])
                .trustLocalhost(true)
                .expect(200);

            await request(URL)
                .delete(`/api/v2/settings${TEST_PATH}`)
                .set("Authorization", `Bearer ${key}`)
                .set("Content-Type", "application/json")
                .send([{ key: "to-delete" }])
                .trustLocalhost(true)
                .expect(200);

            // Verify the key is gone.
            await request(URL)
                .get(`/api/v2/settings${TEST_PATH}`)
                .set("Authorization", `Bearer ${key}`)
                .trustLocalhost(true)
                .expect(200)
                .then((response) => {
                    const node = response.body.find((n: any) => n.key === "to-delete");
                    expect(node).toBeUndefined();
                });
        });

        it("rejects DELETE with no key and no path (safety net)", async () => {
            // Path-level deletes need either a body (so the intent is explicit)
            // or no body at all — an empty entry in a batch must not silently
            // succeed.
            await request(URL)
                .delete(`/api/v2/settings`)
                .set("Authorization", `Bearer ${key}`)
                .set("Content-Type", "application/json")
                .send([{}])
                .trustLocalhost(true)
                .expect(400);
        });
    });
});
