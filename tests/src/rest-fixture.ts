/**
 * Shared setup for the REST API scenarios. Replaces the static
 * tests/rest/nsclient.ini that the old `npm run test` sub-project (and
 * its rest-launcher.test.ts wrapper) used to load.
 *
 * Each rest-*.test.ts scenario spins up its own NscpInstance and calls
 * `setupRestNscp(nscp)` in beforeAll, so test files stay independent
 * and the harness gives each one a clean port-8443 server with the same
 * baseline config (admin/client/legacy users, mock.lua script, alias
 * definitions).
 *
 * The historical INI lives in version control at the move's parent
 * commit if you need to diff against the source-of-truth.
 */
import { bundledLuaScript } from "./nscp";
import type { NscpInstance } from "./nscp";

/** Base URL the supertest suites point at. */
export const REST_URL = "https://127.0.0.1:8443";

/**
 * Apply the REST scenarios' baseline config and start nscp on 8443.
 * Call from beforeAll(). The caller is responsible for `nscp.stop()`
 * in afterAll.
 */
export async function setupRestNscp(nscp: NscpInstance): Promise<void> {
  await nscp.configure({
    "/modules": {
      CheckSystem: "disabled",
      WEBServer: "enabled",
      CheckHelpers: "enabled",
      LUAScript: "enabled",
    },
    "/settings/default": {
      password: "default-password",
      "allowed hosts": "127.0.0.1,::1",
    },
    "/settings/lua/scripts": {
      // The original nsclient.ini wrote `mock = mock.lua` and relied on
      // ${scripts} expanding correctly; per-test workdirs make that
      // unreliable, so we always pass the absolute path.
      mock: bundledLuaScript("mock"),
    },
    "/settings/WEB/server/roles": {
      view: "*",
      legacy: "legacy,login.get",
      full: "*",
      client:
        "public,info.get,info.get.version,queries.list,queries.get,queries.execute,aliases.list,login.get,modules.list",
    },
    // CheckHelpers aliases used by the alias-endpoint and queries scenarios:
    // mock_alias is mapped to a real QUERY (so we can confirm QUERY_ALIAS
    // items are returned and that the underlying QUERY is not), and
    // echo_alias proves arguments survive the alias indirection.
    "/settings/check helpers/alias": {
      mock_alias: "mock_query",
      echo_alias: "check_warning message=hello",
    },
    "/settings/WEB/server/users/client": {
      role: "client",
      password: "client-password",
    },
    "/settings/WEB/server/users/legacy": {
      role: "legacy",
      password: "legacy-password",
    },
    // The admin password is set explicitly so the Jest suite's
    // auth("admin", "default-password") calls work. Pre-0.13 the
    // WEBServer overwrote it with /settings/default/password on every
    // boot; 0.13 removed that, so we now pin it here to match the
    // hard-coded literal in the test files.
    "/settings/WEB/server/users/admin": {
      role: "full",
      password: "default-password",
    },
    "/settings/WEB/server/users/default-admin": {
      role: "full",
    },
  });

  nscp.start();
  await nscp.waitForPort(8443, { timeoutMs: 30_000 });
}
