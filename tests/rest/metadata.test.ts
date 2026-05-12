import { beforeAll, describe, expect, it } from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";

// Tests for the metadata controller (commit 32f7c69e — "Add metadata
// controller for handling metadata API requests"). The controller exposes:
//   GET /api/v2/metadata           — index of available metadata resources
//                                    (grant: metadata.list)
//   GET /api/v2/metadata/counters  — PDH counter catalog (grant:
//                                    metadata.get.counters; requires
//                                    CheckSystem module loaded — Windows)
//   GET /api/v2/metadata/channels  — registered submission channels (grant:
//                                    metadata.get.channels)
//
// CheckSystem is disabled in the test nsclient.ini, so /counters is expected
// to return 500 with a "is the CheckSystem module loaded?" message rather
// than the actual data. We pin that error shape so a future regression that
// silently changed it would surface here.
describe("metadata controller", () => {
    let key: string | undefined;

    beforeAll(async () => {
        await request(URL)
            .get("/api/v2/login")
            .auth("admin", "default-password")
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body.user).toEqual("admin");
                key = response.body.key;
            });
    });

    describe("GET /api/v2/metadata", () => {
        it("requires authentication", async () => {
            await request(URL).get("/api/v2/metadata").trustLocalhost(true).expect(403);
        });

        it("returns the index of metadata resources", async () => {
            await request(URL)
                .get("/api/v2/metadata")
                .set("Authorization", `Bearer ${key}`)
                .trustLocalhost(true)
                .expect(200)
                .then((response) => {
                    expect(Array.isArray(response.body)).toBe(true);
                    const names = response.body.map((e: any) => e.name);
                    expect(names).toEqual(expect.arrayContaining(["counters", "channels"]));
                    // Each entry advertises a follow-up URL the client can
                    // hit. Anchor it loosely so the base URL or scheme can
                    // change without rewriting the test.
                    for (const entry of response.body) {
                        expect(typeof entry.url).toBe("string");
                        expect(entry.url).toMatch(/\/api\/v[12]\/metadata\//);
                        expect(typeof entry.title).toBe("string");
                    }
                });
        });
    });

    describe("GET /api/v2/metadata/channels", () => {
        it("requires authentication", async () => {
            await request(URL).get("/api/v2/metadata/channels").trustLocalhost(true).expect(403);
        });

        it("returns an array of channel descriptors", async () => {
            await request(URL)
                .get("/api/v2/metadata/channels")
                .set("Authorization", `Bearer ${key}`)
                .trustLocalhost(true)
                .expect(200)
                .then((response) => {
                    expect(Array.isArray(response.body)).toBe(true);
                    for (const channel of response.body) {
                        expect(typeof channel.name).toBe("string");
                        expect(Array.isArray(channel.plugins)).toBe(true);
                    }
                });
        });
    });

    describe("GET /api/v2/metadata/counters", () => {
        it("requires authentication", async () => {
            await request(URL).get("/api/v2/metadata/counters").trustLocalhost(true).expect(403);
        });

        it("returns a server error when CheckSystem is not loaded", async () => {
            // nsclient.ini disables CheckSystem for the test suite. The
            // controller forwards to a CheckSystem command (pdh --list) and
            // turns the empty response into a 500 with a helpful message.
            await request(URL)
                .get("/api/v2/metadata/counters")
                .set("Authorization", `Bearer ${key}`)
                .trustLocalhost(true)
                .expect(500)
                .then((response) => {
                    expect(response.text).toMatch(/CheckSystem/);
                });
        });
    });

    describe("v1 alias still routed", () => {
        it("GET /api/v1/metadata returns the same shape", async () => {
            await request(URL)
                .get("/api/v1/metadata")
                .set("Authorization", `Bearer ${key}`)
                .trustLocalhost(true)
                .expect(200)
                .then((response) => {
                    expect(Array.isArray(response.body)).toBe(true);
                });
        });
    });
});
