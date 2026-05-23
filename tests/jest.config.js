const path = require("path");

/** @type {import('jest').Config} */
module.exports = {
  testEnvironment: "<rootDir>/src/test-env.ts",
  rootDir: __dirname,
  testMatch: ["<rootDir>/*.test.ts"],
  testPathIgnorePatterns: [
    "/node_modules/",
  ],
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
  testTimeout: 600000,
  bail: false,
  verbose: true,
  globalSetup: "<rootDir>/src/global-setup.ts",
};
