import * as fs from "fs";
import * as path from "path";
import execa from "execa";

/**
 * Resolve a bare executable name (e.g. "nscp") against PATH. Returns the
 * absolute path or undefined. Used so CI configs can set NSCP_BIN=nscp
 * without having to know whether the install put the binary in /usr/sbin
 * (Debian/RPM service binary convention) or /usr/bin.
 */
function whichOnPath(name: string): string | undefined {
  const dirs = (process.env.PATH ?? "").split(path.delimiter);
  const exts =
    process.platform === "win32"
      ? (process.env.PATHEXT ?? ".EXE;.CMD;.BAT").split(";")
      : [""];
  for (const dir of dirs) {
    if (!dir) continue;
    for (const ext of exts) {
      const candidate = path.join(dir, name + ext);
      if (fs.existsSync(candidate)) return candidate;
    }
  }
  return undefined;
}

function findNscp(): string {
  if (process.env.NSCP_BIN) {
    const raw = process.env.NSCP_BIN;
    // Absolute / explicit-relative path — must exist as given.
    if (path.isAbsolute(raw) || raw.includes(path.sep) || raw.includes("/")) {
      if (!fs.existsSync(raw)) {
        throw new Error(`NSCP_BIN=${raw} does not exist`);
      }
      return path.resolve(raw);
    }
    // Bare command name (e.g. NSCP_BIN=nscp) — resolve via PATH so the same
    // env var works against a built tree, a Debian sbin install, and an RPM
    // install without per-distro path hardcoding.
    const resolved = whichOnPath(raw);
    if (!resolved) {
      throw new Error(
        `NSCP_BIN=${raw} could not be resolved on PATH (PATH=${process.env.PATH ?? ""})`,
      );
    }
    return resolved;
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
  // The docker-using scenarios skip themselves via dockerOrSkip() when this
  // env var is set, so probing for a daemon would just abort a perfectly
  // valid no-docker run (e.g. the CI stage that only exercises the rest-*
  // suites). Skip the probe and let the docker-needing scenarios — which
  // won't run anyway — decide for themselves.
  if (process.env.NSCP_SKIP_DOCKER !== "1") {
    verifyDocker();
  }
  // eslint-disable-next-line no-console
  console.log(`[integration] using nscp at ${bin}`);
}
