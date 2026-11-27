// SMTP targets: the mail servers results are sent through.
import { targetsCollection } from "../kit/targetsCollection.tsx";
import { connectionFields, isOn } from "../kit/targetFields.ts";
import type { TargetSpec } from "../kit/targets.ts";
import type { ModulePage } from "../types.ts";

const SECURITY = ["starttls", "tls", "none"];

export const SMTP_CLIENT_TARGETS: TargetSpec = {
  moduleId: "SMTPClient",
  path: "/settings/SMTP/client/targets",
  noun: "mail server",
  fields: {
    ...connectionFields("587"),
    security: {
      label: "Connection security",
      kind: (v) => ({
        options: SECURITY.includes(v("security")) ? SECURITY : [...SECURITY, v("security")],
      }),
      builtIn: "starttls",
      help: "starttls upgrades a plain connection before logging in (port 587); tls connects with TLS (port 465, which the address has to name); none is plain SMTP.",
    },
    username: {
      label: "User name",
      kind: "text",
      builtIn: "",
      help: "Empty sends without logging in, for a relay that allows it.",
    },
    password: { label: "Password", kind: "secret", builtIn: "" },
    ca: {
      label: "CA",
      kind: "text",
      builtIn: "${ca-path}",
      help: "${ca-path} is the system's trusted roots; none uses OpenSSL's own.",
    },
    "insecure-skip-verify": {
      label: "Do not verify the server",
      kind: "bool",
      builtIn: "false",
      help: "Only for a test server with a self-signed certificate.",
    },
    sender: { label: "From", kind: "text", builtIn: "nscp@localhost" },
    recipient: { label: "To", kind: "text", builtIn: "nscp@localhost" },
    subject: {
      label: "Subject",
      kind: "text",
      builtIn: "[NSClient++] %source%",
      help: "%source% is the check, %message% its output.",
    },
    template: {
      label: "Body",
      kind: "text",
      builtIn: "Hello, this is %source% reporting %message%!",
    },
    "ehlo-hostname": {
      label: "EHLO host name",
      kind: "text",
      builtIn: "",
      help: "The agent's host name when empty; some services want a real FQDN.",
    },
    "allow recipient override": {
      label: "Let a request choose From and To",
      kind: "bool",
      builtIn: "false",
      help: "Off refuses recipient= and sender= on a request that logs in with this target's credentials.",
    },
  },
  sections: [
    { title: "Connection", keys: ["address", "timeout", "retries"] },
    { title: "Security", keys: ["security", "username", "password", "insecure-skip-verify"] },
    { title: "Message", keys: ["sender", "recipient", "subject", "template"] },
  ],
  risks: (v) => {
    const out = [];
    const plain = v("security") === "none";
    if (plain && v("username") !== "")
      out.push({
        severity: "error" as const,
        title: "Every mail is refused",
        reason:
          "The agent will not send a login over plain SMTP. Use starttls or tls, or no user name.",
      });
    else if (plain)
      out.push({
        severity: "warning" as const,
        title: "Sent in clear",
        reason: "Plain SMTP: only for a trusted internal relay.",
      });
    if (!plain && isOn(v("insecure-skip-verify")))
      out.push({
        severity: "warning" as const,
        title: "The server is not verified",
        reason:
          "Anyone who can intercept the connection can pose as the mail server and collect the login.",
      });
    return out;
  },
  security: (v) => (v("security") === "none" ? "Plain SMTP" : v("security").toUpperCase()),
};

const SMTPClient: ModulePage = {
  id: "SMTPClient",
  purpose: {
    tools: ["Email"],
    transport: "SMTP",
    direction: "passive",
    platforms: ["windows", "linux", "macos"],
  },
  collections: { [SMTP_CLIENT_TARGETS.path]: targetsCollection(SMTP_CLIENT_TARGETS) },
};

export default SMTPClient;
