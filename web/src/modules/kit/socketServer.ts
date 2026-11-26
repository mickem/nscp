// What the socket servers (NSCA, NSClient, check_mk; NRPE has modes of its
// own) have in common: the core options every one of them registers, read
// from [/settings/default] when the module leaves them out
// (add_core_server_opts), and the TLS option set with three modes.
import { useCallback, useState } from "react";
import { Settings } from "../../api/api.ts";
import { SettingChange } from "./changes.ts";
import { fixed, KeySpec } from "./keys.ts";
import { CUSTOM, Custom } from "./modes.ts";
import type { TlsFiles } from "./TlsSection.tsx";
import {
  detectSocketTls,
  readSocketTlsFiles,
  socketTlsChanges,
  SocketTlsDefaults,
  SocketTlsMode,
  tlsFilesProblem,
} from "./tls.ts";
import type { ValueKind } from "./ValueField.tsx";

// One key as a page lays it out.
export interface FieldSpec {
  key: string;
  label: string;
  kind: ValueKind | "secret";
  help?: string;
}

export function coreServerKeys(): Record<string, KeySpec> {
  return {
    "allowed hosts": { builtIn: fixed("127.0.0.1"), inherits: true },
    timeout: { builtIn: fixed("30"), inherits: true },
    "bind to": { builtIn: fixed(""), inherits: true },
    "cache allowed hosts": { builtIn: fixed("true"), inherits: true },
    "thread pool": { builtIn: fixed("10"), inherits: true },
    "socket queue size": { builtIn: fixed("0"), inherits: true },
  };
}

// The core options a page keeps under Advanced.
export const CORE_ADVANCED: FieldSpec[] = [
  {
    key: "bind to",
    label: "Bind to address",
    kind: "text",
    help: "A local IP address to listen on; empty for all.",
  },
  {
    key: "cache allowed hosts",
    label: "Cache allowed hosts",
    kind: "bool",
    help: "Resolve host names in the allowed-hosts list once; turn off for monitoring servers with dynamic addresses.",
  },
  {
    key: "thread pool",
    label: "Thread pool",
    kind: "number",
    help: "Worker threads serving connections.",
  },
  {
    key: "socket queue size",
    label: "Listen queue",
    kind: "number",
    help: "Connections queued before new ones are refused; 0 is the system default.",
  },
];

// The TLS options a page keeps under Advanced; the mode owns the rest.
export const SOCKET_TLS_ADVANCED: FieldSpec[] = [
  {
    key: "allowed ciphers",
    label: "Allowed ciphers",
    kind: "text",
    help: "OpenSSL cipher list.",
  },
  { key: "certificate format", label: "Certificate format", kind: { options: ["PEM", "asn1"] } },
  {
    key: "dh",
    label: "DH parameters",
    kind: "text",
    help: "File with the Diffie-Hellman parameters; empty for none.",
  },
  {
    key: "ssl options",
    label: "SSL options",
    kind: "text",
    help: "Extra OpenSSL flags, comma separated (default-workarounds, no-sslv2, ...).",
  },
  {
    key: "debug verify",
    label: "Debug certificate verification",
    kind: "bool",
    help: "Print certificate verification errors to stdout.",
  },
];

// The reader's edits to the TLS mode and files. Kept apart from the panel's
// draft because one choice writes several keys.
export interface SocketTlsDraft {
  mode?: SocketTlsMode | Custom;
  files?: TlsFiles;
}

export function useSocketTlsDraft() {
  const [draft, setDraft] = useState<SocketTlsDraft>({});
  const reset = useCallback(() => setDraft({}), []);
  return { draft, setDraft, reset };
}

export interface SocketTlsView {
  // What the stored keys describe, and what the page shows.
  current: SocketTlsMode | Custom;
  mode: SocketTlsMode | Custom;
  files: TlsFiles;
  changes: SettingChange[];
  problem: string | null;
}

export function socketTlsView(
  draft: SocketTlsDraft,
  stored: Settings[] | undefined,
  path: string,
  d: SocketTlsDefaults,
): SocketTlsView {
  const current = detectSocketTls(stored, path, d);
  const mode = draft.mode ?? current;
  const files = draft.files ?? readSocketTlsFiles(stored, path);
  // Untouched controls write nothing, and Custom leaves the keys alone.
  const touched = draft.mode !== undefined || draft.files !== undefined;
  const changes = touched && mode !== CUSTOM ? socketTlsChanges(path, mode, files, d) : [];
  const problem = mode === CUSTOM ? null : tlsFilesProblem(mode, files);
  return { current, mode, files, changes, problem };
}
