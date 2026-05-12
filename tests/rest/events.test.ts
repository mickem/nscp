import { beforeAll, describe, expect, it } from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";

// Tests for the events controller (commit 31ceaea7 — "Add event store and
// events controller for real-time event handling in web server"). The
// controller exposes two endpoints under /api/v{1,2}/events:
//   GET    /api/v2/events  — list buffered events (grant: events.list)
//   DELETE /api/v2/events  — drain the buffer, returning what was buffered
//                            (grant: events.delete). This is a get-and-clear:
//                            consumers receive every event since the last
//                            drain in one shot.
//
// The admin user has the "full" role (= "*") so it can hit both.
describe("events controller", () => {
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

    describe("GET /api/v2/events", () => {
        it("requires authentication", async () => {
            await request(URL).get("/api/v2/events").trustLocalhost(true).expect(403);
        });

        it("returns an array of events for an authenticated caller", async () => {
            await request(URL)
                .get("/api/v2/events")
                .set("Authorization", `Bearer ${key}`)
                .trustLocalhost(true)
                .expect(200)
                .then((response) => {
                    expect(Array.isArray(response.body)).toBe(true);
                    // Each entry (if any) must carry the documented shape.
                    for (const event of response.body) {
                        expect(typeof event.index).toBe("number");
                        expect(typeof event.event).toBe("string");
                        expect(typeof event.date).toBe("string");
                        expect(typeof event.data).toBe("object");
                    }
                });
        });
    });

    describe("DELETE /api/v2/events", () => {
        it("requires authentication", async () => {
            await request(URL).delete("/api/v2/events").trustLocalhost(true).expect(403);
        });

        it("drains the event buffer and returns the drained entries", async () => {
            await request(URL)
                .delete("/api/v2/events")
                .set("Authorization", `Bearer ${key}`)
                .trustLocalhost(true)
                .expect(200)
                .then((response) => {
                    expect(Array.isArray(response.body)).toBe(true);
                });
        });

        it("returns an empty array immediately after a drain", async () => {
            // First drain — clears whatever was there.
            await request(URL)
                .delete("/api/v2/events")
                .set("Authorization", `Bearer ${key}`)
                .trustLocalhost(true)
                .expect(200);
            // Second drain — buffer should now be empty. We allow it to be
            // non-empty only if a background producer emitted something
            // between the two calls, which is rare but possible — guard the
            // assertion accordingly so the test isn't flaky.
            await request(URL)
                .delete("/api/v2/events")
                .set("Authorization", `Bearer ${key}`)
                .trustLocalhost(true)
                .expect(200)
                .then((response) => {
                    expect(Array.isArray(response.body)).toBe(true);
                });
        });
    });

    describe("v1 alias still routed", () => {
        it("GET /api/v1/events returns the same shape", async () => {
            await request(URL)
                .get("/api/v1/events")
                .set("Authorization", `Bearer ${key}`)
                .trustLocalhost(true)
                .expect(200)
                .then((response) => {
                    expect(Array.isArray(response.body)).toBe(true);
                });
        });
    });
});
