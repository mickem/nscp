import NodeEnvironment from "jest-environment-node";
import type { EnvironmentContext, JestEnvironmentConfig } from "@jest/environment";
import type { Circus } from "@jest/types";

type Dumper = () => string | Promise<string>;

/**
 * Jest environment that runs registered "failure dumpers" when an `it`
 * fails. Dumpers are pushed onto `globalThis.__nscpFailureDumpers` from
 * test code (see `trackContainerLogs` and `NscpInstance`); on
 * `test_done` with errors we drain the list to stderr so the captured
 * container / nscp output is visible in the test runner without having
 * to re-run with `logOutput: true`.
 */
export default class FailureDumpEnvironment extends NodeEnvironment {
  constructor(config: JestEnvironmentConfig, context: EnvironmentContext) {
    super(config, context);
  }

  async setup(): Promise<void> {
    await super.setup();
    (this.global as unknown as { __nscpFailureDumpers: Dumper[] }).__nscpFailureDumpers = [];
  }

  async handleTestEvent(event: Circus.Event): Promise<void> {
    if (event.name !== "test_done") return;
    if (event.test.errors.length === 0) return;
    // Don't clear the dumper list between tests: most suites register
    // their sources (nscp + tracked containers) once in `beforeAll`,
    // and each dumper closes over the current state of its source so
    // re-reading reflects the latest output on every failed `it`.
    const dumpers = (this.global as unknown as { __nscpFailureDumpers?: Dumper[] }).__nscpFailureDumpers ?? [];
    const testName = event.test.name;
    process.stderr.write(`\n----- failure dump (${testName}) -----\n`);
    for (const d of dumpers) {
      try {
        const text = await d();
        if (text) process.stderr.write(text);
      } catch (e) {
        process.stderr.write(`[failure-dump] dumper threw: ${(e as Error).message}\n`);
      }
    }
    process.stderr.write(`----- end failure dump -----\n`);
  }
}
