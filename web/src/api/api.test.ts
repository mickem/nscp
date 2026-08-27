import { describe, expect, it } from "vitest";
import { FetchBaseQueryMeta } from "@reduxjs/toolkit/query";
import { responseHandler, transformPaginatedResponse } from "./api";

function metaWithHeaders(headers: Record<string, string>): FetchBaseQueryMeta {
  return {
    response: new Response(null, { headers }),
  } as FetchBaseQueryMeta;
}

describe("transformPaginatedResponse", () => {
  it("reads pagination info from the response headers", () => {
    const meta = metaWithHeaders({
      "X-Pagination-Count": "42",
      "X-Pagination-Page": "2",
      "X-Pagination-Limit": "10",
    });
    const page = transformPaginatedResponse(["a", "b"], meta);
    expect(page.content).toEqual(["a", "b"]);
    expect(page.count).toBe(42);
    expect(page.page).toBe(2);
    expect(page.limit).toBe(10);
    expect(page.pages).toBe(5);
  });

  it("defaults missing headers to zero", () => {
    const page = transformPaginatedResponse([], metaWithHeaders({}));
    expect(page.count).toBe(0);
    expect(page.page).toBe(0);
    expect(page.limit).toBe(0);
  });
});

describe("responseHandler", () => {
  it("parses JSON bodies on success", async () => {
    const response = new Response(JSON.stringify({ hello: "world" }), { status: 200 });
    await expect(responseHandler(response)).resolves.toEqual({ hello: "world" });
  });

  it.each([400, 401, 403, 404, 500])("wraps a %i response as an error object", async (status) => {
    const response = new Response("boom", { status });
    await expect(responseHandler(response)).resolves.toEqual({ error: "boom", status });
  });
});
