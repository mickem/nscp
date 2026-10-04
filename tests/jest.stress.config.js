const path = require("path");

/**
 * Jest config for the opt-in stress harness under tests/stress/.
 *
 * The scenarios there (`*.stress.ts`) load one agent for a minute or more and
 * pass or fail on invariants (nothing lost, still responsive, no growth, a
 * clean log) while recording throughput and latency into
 * `stress/results/<scenario>.json`. They are never part of `npm test`, whose
 * testMatch only picks up the top-level *.test.ts files:
 *
 *   npm run test:stress
 *   NSCP_STRESS_DURATION=300 npx jest --config jest.stress.config.js rest-flood
 *
 * Knobs (environment): NSCP_STRESS_DURATION (seconds, default 60),
 * NSCP_STRESS_CONCURRENCY (default 32), NSCP_STRESS_RPS (0 = unbounded); see
 * stress/lib/knobs.ts for the rest.
 *
 * @type {import('jest').Config}
 */
module.exports = {
  testEnvironment: "<rootDir>/src/test-env.ts",
  rootDir: __dirname,
  testMatch: ["<rootDir>/stress/**/*.stress.ts"],
  testPathIgnorePatterns: ["/node_modules/"],
  transform: {
    "^.+\\.tsx?$": [
      require.resolve("ts-jest"),
      { tsconfig: path.resolve(__dirname, "tsconfig.json") },
    ],
  },
  moduleNameMapper: {
    "^@fixtures/(.*)$": "<rootDir>/src/$1",
    "^@stress/(.*)$": "<rootDir>/stress/lib/$1",
  },
  modulePaths: ["<rootDir>/node_modules"],
  maxWorkers: 1,
  // A soak (NSCP_STRESS_DURATION=1800) plus its warm-up and teardown; the
  // scenarios set their own, tighter, timeout from the knobs.
  testTimeout: 4 * 60 * 60 * 1000,
  bail: false,
  verbose: true,
  globalSetup: "<rootDir>/src/global-setup-stress.ts",
};
