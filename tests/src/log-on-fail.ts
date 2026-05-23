import type { StartedTestContainer } from "testcontainers";

type Dumper = () => string | Promise<string>;

interface GlobalWithDumpers {
  __nscpFailureDumpers?: Dumper[];
}

/**
 * Push a dumper onto the per-test failure-dump registry. The custom
 * test environment drains this list on `test_done` when the test
 * failed. No-op outside the custom environment (e.g. raw ts-node
 * scripts).
 */
export function registerFailureDumper(fn: Dumper): void {
  const g = globalThis as unknown as GlobalWithDumpers;
  if (Array.isArray(g.__nscpFailureDumpers)) {
    g.__nscpFailureDumpers.push(fn);
  }
}

/**
 * Stream a started testcontainers container's logs into a buffer and
 * register that buffer for the failure dumper. Returns the same
 * container so it can be chained:
 *
 *   const c = await trackContainerLogs(
 *     await new GenericContainer(image).start(),
 *     "nrdp-server",
 *   );
 */
export async function trackContainerLogs(
  container: StartedTestContainer,
  label?: string,
): Promise<StartedTestContainer> {
  const tag = label ?? container.getName();
  const chunks: string[] = [];
  let stream;
  try {
    stream = await container.logs();
  } catch (e) {
    registerFailureDumper(() => `--- container logs: ${tag} ---\n[failed to open log stream: ${(e as Error).message}]\n`);
    return container;
  }
  const onData = (chunk: Buffer | string): void => {
    chunks.push(Buffer.isBuffer(chunk) ? chunk.toString("utf8") : String(chunk));
  };
  stream.on("data", onData);
  stream.on("err", onData);
  registerFailureDumper(() => {
    if (chunks.length === 0) return `--- container logs: ${tag} (no output) ---\n`;
    return `--- container logs: ${tag} ---\n${chunks.join("")}${chunks.join("").endsWith("\n") ? "" : "\n"}`;
  });
  return container;
}
