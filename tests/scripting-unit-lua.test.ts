/**
 * `nscp unit --language lua` against the agent under test, with `${scripts}`
 * somewhere other than next to the binary.
 *
 * The Lua test scripts themselves are ctest targets (NSCP_ADD_LUA_TEST in
 * tests/CMakeLists.txt), so every build and the sanitizer job run them from
 * the build tree. What the build tree cannot show is a package: there
 * `${scripts}` is not `${base-path}/scripts`, and that is the folder
 * LUAScript used to root `require()` at - so `require("test_helper")`, and
 * any shared helper in `scripts/lua/lib`, could not load from an installed
 * agent. scriptingUnitInstance() puts `${scripts}` in a scratch dir, and the
 * first case requires a module that exists nowhere else.
 *
 * It also pins `--case` and `--show-all`, which `nscp unit` passes to the
 * script as two extra queries the Lua helper did not register.
 */
import * as fs from "fs";
import * as path from "path";

import {
  NscpInstance,
  describeWithModules,
  runScriptingUnit,
  scriptingUnitInstance,
} from "@fixtures/index";

jest.setTimeout(300_000);

// Only in the scratch `${scripts}/lua/lib`. A build tree has a test_helper.lua
// under `${base-path}/scripts/lua/lib` as well, so requiring test_helper alone
// passes whichever root the lookup uses; this module can only come from
// `${scripts}`.
const SCRATCH_LIB = `
local M = {}
function M.answer() return 'from the scratch lib' end
return M
`;

const REQUIRE_FIXTURE = `
local test = require("test_helper")
local scratch = require("scratch_only_lib")

local RequireTest = { name = "require from scripts" }
function RequireTest:install(arguments)
	local conf = Settings()
	conf:set_string('/modules', 'luatest', 'LUAScript')
	conf:set_string('/settings/luatest/scripts', 'test_require_fixture', 'test_require_fixture.lua')
end
function RequireTest:setup() end
function RequireTest:teardown() end
function RequireTest:run()
	local result = test.TestResult:new{message = 'require() from the scripts folder'}
	result:assert_equals(scratch.answer(), 'from the scratch lib', 'scratch lib loaded')
	return result
end

local instances = { RequireTest }
test.init_test_manager(instances)
function main(args)
	return test.install_test_manager(instances)
end
`;

describeWithModules(
  "LUAScript",
  "NRPEServer",
  "NRPEClient",
  "CheckHelpers",
  "NSCAServer",
  "NSCAClient",
)("nscp unit --language lua", () => {
  let nscp: NscpInstance;

  beforeAll(() => {
    nscp = scriptingUnitInstance("lua");
    const lua = path.join(nscp.pathOverrides.scripts, "lua");
    fs.writeFileSync(path.join(lua, "lib", "scratch_only_lib.lua"), SCRATCH_LIB);
    fs.writeFileSync(path.join(lua, "test_require_fixture.lua"), REQUIRE_FIXTURE);
  });

  it("resolves require() from ${scripts}/lua/lib", async () => {
    const r = await runScriptingUnit(nscp, "lua", "test_require_fixture");
    expect(r.all).not.toMatch(/module '\w+' not found/);
    expect(r.all).toMatch(/\d+ test cases succeeded/);
    expect(r.exitCode).toBe(0);
  });

  // One check relayed over an NRPE client target to a server in the same
  // agent, as the bundled script does it.
  it("test_nrpe_relay", async () => {
    const r = await runScriptingUnit(nscp, "lua", "test_nrpe_relay");
    expect(r.all).toMatch(/\d+ test cases succeeded/);
    expect(r.exitCode).toBe(0);
  });

  it("runs the suite --case names and logs passes with --show-all", async () => {
    const r = await runScriptingUnit(nscp, "lua", "test_nsca", { cases: ["nsca"], showAll: true });
    expect(r.all).toMatch(/\d+ test cases succeeded/);
    // Only printed for a passing suite when --show-all reached the script.
    expect(r.all).toContain("[OK ] - Running suite: NSCA round trip");
    expect(r.exitCode).toBe(0);
  });

  // A case naming no suite is a failure, not a silent pass of zero tests.
  it("fails when --case matches no suite", async () => {
    const r = await runScriptingUnit(nscp, "lua", "test_nsca", { cases: ["no-such-suite"] });
    expect(r.all).toMatch(/No suite matches --case no-such-suite/);
    expect(r.exitCode).not.toBe(0);
  });
});
