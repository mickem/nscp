// What the Docker page knows about CheckDocker's endpoint (docker_endpoint.hpp).

// Why the agent would refuse an endpoint, or null.
export function endpointProblem(v: string): string | null {
  const e = v.trim();
  if (e === "") return null;
  if (/^\\\\[.?]\\pipe\\[^\\/]+$/i.test(e)) return null;
  if (e.startsWith("/") && !/(^|\/)\.\.(\/|$)/.test(e)) return null;
  return "Docker is reached through a local socket (an absolute path) or a named pipe (\\\\.\\pipe\\name) only: no tcp://, https:// or relative paths.";
}
