// The host name a client reports itself as: `auto` and its case variants, or
// a pattern of placeholders (socket_helpers::expand_hostname in
// include/net/socket/socket_helpers.cpp).

export const HOSTNAME_FORMS: Array<{ value: string; label: string }> = [
  { value: "auto", label: "The computer's name, as the system reports it" },
  { value: "auto-lc", label: "The computer's name, in lowercase" },
  { value: "auto-uc", label: "The computer's name, in uppercase" },
];

export const HOSTNAME_PLACEHOLDERS: Array<{ token: string; label: string }> = [
  { token: "${hostname}", label: "Full name, with the domain" },
  { token: "${hostname_lc}", label: "Full name, lowercase" },
  { token: "${hostname_uc}", label: "Full name, uppercase" },
  { token: "${host}", label: "Name up to the first dot" },
  { token: "${host_lc}", label: "Name up to the first dot, lowercase" },
  { token: "${host_uc}", label: "Name up to the first dot, uppercase" },
  { token: "${domain}", label: "Domain (after the first dot)" },
  { token: "${domain_lc}", label: "Domain, lowercase" },
  { token: "${domain_uc}", label: "Domain, uppercase" },
  { token: "${address_ipv4}", label: "IPv4 address" },
  { token: "${address_ipv6}", label: "IPv6 address (compressed)" },
  { token: "${address_ipv6_lc_uncomp}", label: "IPv6 address, uncompressed" },
  { token: "${address_ipv6_uc_comp}", label: "IPv6 address, uppercase" },
  { token: "${address_ipv6_uc_uncomp}", label: "IPv6 address, uppercase, uncompressed" },
];

// One line saying what a value reports, for a list row.
export function describeHostname(value: string): string {
  const form = HOSTNAME_FORMS.find((f) => f.value === value);
  if (form) return `${form.label}.`;
  if (value === "") return "Not set.";
  return value.includes("${") ? "A pattern of the computer's name or address." : "A fixed name.";
}

export const HOSTNAME_HELP =
  "The name results are reported under: it has to match the host's name in the monitoring system.";
