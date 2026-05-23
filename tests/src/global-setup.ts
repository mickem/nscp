import * as fs from "fs";
import * as path from "path";
import execa from "execa";

function findNscp(): string {
  if (process.env.NSCP_BIN) {
    if (!fs.existsSync(process.env.NSCP_BIN)) {
      throw new Error(`NSCP_BIN=${process.env.NSCP_BIN} does not exist`);
    }
    return path.resolve(process.env.NSCP_BIN);
  }
  const buildDirs = [
    process.env.NSCP_BUILD_DIR,
    path.resolve(__dirname, "../../cmake-build-debug-wsl"),
    path.resolve(__dirname, "../../cmake-build-debug"),
    path.resolve(__dirname, "../../build"),
  ].filter(Boolean) as string[];
  const exe = process.platform === "win32" ? "nscp.exe" : "nscp";
  for (const dir of buildDirs) {
    const candidate = path.join(dir, exe);
    if (fs.existsSync(candidate)) return candidate;
  }
  throw new Error(
    `Could not locate ${exe}. Set NSCP_BIN to its path, or NSCP_BUILD_DIR to a directory containing it. Searched: ${buildDirs.join(", ")}`,
  );
}

function verifyDocker(): void {
  try {
    execa.sync("docker", ["version", "--format", "{{.Server.Version}}"], {
      timeout: 10_000,
    });
  } catch (err) {
    throw new Error(
      `Docker is required for integration tests but \`docker version\` failed: ${(err as Error).message}`,
    );
  }
}

export default async function globalSetup(): Promise<void> {
  const bin = findNscp();
  process.env.NSCP_BIN = bin;
  verifyDocker();
  // eslint-disable-next-line no-console
  console.log(`[integration] using nscp at ${bin}`);
}
