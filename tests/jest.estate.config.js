const live = require("./jest.live.config");

/**
 * Jest config for the role checks of the Azure estate (live/estate.test.ts):
 * the live config, narrowed to that suite, with a global setup that finds the
 * estate on its own.
 *
 *   npm run test:estate                    # every estate that is up
 *   npm run test:estate -- -t mssql        # only the machines whose name matches
 *
 * NSCP_ESTATE_MANIFEST picks a manifest instead; see live/README.md.
 *
 * @type {import('jest').Config}
 */
module.exports = {
  ...live,
  testMatch: ["<rootDir>/live/estate.test.ts"],
  globalSetup: "<rootDir>/src/global-setup-estate.ts",
};
