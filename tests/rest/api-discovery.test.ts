import { beforeAll, describe, expect, it } from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";

// Tests for the API discovery endpoints exposed by api_controller:
//   GET /api      — list available API versions
//   GET /api/v1   — list endpoints in the legacy API (alias of v2 today)
//   GET /api/v2   — list endpoints in the current API
//
// These power client auto-discovery. A regression that drops one of the
// advertised URLs would silently break tooling that relies on the index, so
// we pin the exact set of URLs returned.
describe("api discovery", () => {
    let key: string | undefined;

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

    it("GET /api requires authentication", async () => {
        await request(URL).get("/api").trustLocalhost(true).expect(403);
    });

    it("GET /api returns the version index", async () => {
        await request(URL)
            .get("/api")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body.current_api).toEqual("https://127.0.0.1:8443/api/v2");
                expect(response.body.legacy_api).toEqual("https://127.0.0.1:8443/api/v1");
                expect(response.body.beta_api).toEqual("https://127.0.0.1:8443/api/v2");
            });
    });

    it("GET /api/v2 lists the endpoint URLs", async () => {
        await request(URL)
            .get("/api/v2")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body.scripts_url).toMatch(/\/api\/v2\/scripts$/);
                expect(response.body.modules_url).toMatch(/\/api\/v2\/modules$/);
                expect(response.body.queries_url).toMatch(/\/api\/v2\/queries$/);
                expect(response.body.settings_url).toMatch(/\/api\/v2\/settings$/);
                expect(response.body.logs_url).toMatch(/\/api\/v2\/logs$/);
                expect(response.body.info_url).toMatch(/\/api\/v2\/info$/);
            });
    });

    it("GET /api/v1 returns the same endpoint shape (alias)", async () => {
        await request(URL)
            .get("/api/v1")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body.modules_url).toBeDefined();
                expect(response.body.queries_url).toBeDefined();
                expect(response.body.info_url).toBeDefined();
            });
    });
});

// ============================================================================
// Legacy core endpoints (legacy_controller)
//   GET /core/isalive   — liveness probe, unauthenticated
//   POST /core/reload   — service reload (legacy grant)
//
// These are the only legacy_controller endpoints that aren't covered by
// legacy-query.test.ts or legacy-auth-icinga.test.ts.
// ============================================================================
describe("legacy core endpoints", () => {
    it("GET /core/isalive returns a status object without authentication", async () => {
        // Liveness probe — intentionally open so monitoring stacks can call
        // it before any credentials are minted.
        await request(URL)
            .get("/core/isalive")
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body.status).toBeDefined();
                expect(typeof response.body.status).toBe("string");
            });
    });
});
