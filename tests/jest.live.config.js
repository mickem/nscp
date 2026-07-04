const path = require("path");

/**
 * Jest config for the live/remote acceptance suite.
 *
 * These suites (tests/live/*.test.ts) connect to an nscp that is already
 * installed and running elsewhere (a provisioned VM or a local service) rather
 * than spawning their own via NscpInstance. They are therefore kept out of the
 * default `npm test` run (whose testMatch only picks up top-level *.test.ts)
 * and use a global-setup that validates NSCP_TARGET_* instead of NSCP_BIN.
 *
 *   npm run test:live
 *
 * @type {import('jest').Config}
 */
module.exports = {
  testEnvironment: "<rootDir>/src/test-env.ts",
  rootDir: __dirname,
  testMatch: ["<rootDir>/live/**/*.test.ts"],
  testPathIgnorePatterns: ["/node_modules/"],
  transform: {
    "^.+\\.tsx?$": [
      require.resolve("ts-jest"),
      { tsconfig: path.resolve(__dirname, "tsconfig.json") },
    ],
  },
  moduleNameMapper: {
    "^@fixtures/(.*)$": "<rootDir>/src/$1",
  },
  modulePaths: ["<rootDir>/node_modules"],
  maxWorkers: 1,
  testTimeout: 120000,
  bail: false,
  verbose: true,
  globalSetup: "<rootDir>/src/global-setup-live.ts",
};
