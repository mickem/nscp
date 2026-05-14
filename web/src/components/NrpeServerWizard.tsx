import {
  Settings,
  useGetSettingsQuery,
  useUpdateSettingsMutation,
} from "../api/api.ts";
import {
  Box,
  Button,
  Card,
  CardContent,
  CardHeader,
  FormControl,
  FormControlLabel,
  InputLabel,
  MenuItem,
  Radio,
  RadioGroup,
  Select,
  Stack,
  TextField,
  Typography,
} from "@mui/material";
import { useEffect, useMemo, useState } from "react";
import NscpAlert from "./atoms/NscpAlert.tsx";

type SecurityMode = "insecure" | "encrypted" | "two-way-tls";
type ArgsMode = "false" | "safe" | "all";

const NRPE_PATH = "/settings/NRPE/server";
const DEFAULT_PATH = "/settings/default";
const MODULES_PATH = "/modules";

const INSECURE_CIPHERS = "ALL:!MD5:@STRENGTH:@SECLEVEL=0";
const SECURE_CIPHERS = "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH";
const SECURE_SSL_OPTIONS = "no-sslv2,no-sslv3";

function readSetting(
  stored: Settings[] | undefined,
  path: string,
  key: string,
  dflt: string,
): string {
  if (!stored) return dflt;
  // /v2/settings can list the same (path, key) more than once — last write
  // wins, which mirrors how the backend resolves the effective value.
  let value: string | undefined;
  for (const s of stored) {
    if (s.path === path && s.key === key) value = s.value;
  }
  return value ?? dflt;
}

function readBool(
  stored: Settings[] | undefined,
  path: string,
  key: string,
  dflt: boolean,
): boolean {
  const v = readSetting(stored, path, key, dflt ? "true" : "false").toLowerCase();
  return v === "true" || v === "1";
}

export default function NrpeServerWizard() {
  const { data: storedSettings } = useGetSettingsQuery();
  const [updateSettings] = useUpdateSettingsMutation();

  // Compute the wizard's "current" snapshot from the stored values, mirroring
  // the CLI wizard's initialization (including the cipher-string override of
  // the insecure flag).
  const initial = useMemo(() => {
    const allowedHosts = readSetting(
      storedSettings,
      DEFAULT_PATH,
      "allowed hosts",
      "127.0.0.1",
    );
    const port = readSetting(storedSettings, NRPE_PATH, "port", "5666");
    let insecure = readBool(storedSettings, NRPE_PATH, "insecure", false);
    const ca = readSetting(storedSettings, NRPE_PATH, "ca", "");
    const certificate = readSetting(
      storedSettings,
      NRPE_PATH,
      "certificate",
      "${certificate-path}/certificate.pem",
    );
    const certificateKey = readSetting(storedSettings, NRPE_PATH, "certificate key", "");
    const ciphers = readSetting(storedSettings, NRPE_PATH, "allowed ciphers", "");
    if (ciphers === INSECURE_CIPHERS) insecure = true;
    else if (ciphers === SECURE_CIPHERS) insecure = false;

    const allowArgs = readBool(storedSettings, NRPE_PATH, "allow arguments", false);
    const allowNasty = readBool(storedSettings, NRPE_PATH, "allow nasty characters", false);
    let args: ArgsMode = "false";
    if (allowArgs && allowNasty) args = "all";
    else if (allowArgs) args = "safe";

    const verifyRaw = readSetting(storedSettings, NRPE_PATH, "verify mode", "peer-cert");
    // Three high-level modes — the legacy `peer` value (cert-validated when
    // presented but optional) is bucketed into "encrypted" because the radio
    // doesn't expose it directly. Users can still write `peer` manually via
    // the regular settings list.
    let mode: SecurityMode;
    if (insecure) mode = "insecure";
    else if (verifyRaw === "peer-cert") mode = "two-way-tls";
    else mode = "encrypted";

    const payloadLength = readSetting(storedSettings, NRPE_PATH, "payload length", "1024");

    return {
      allowedHosts,
      port,
      mode,
      ca,
      certificate,
      certificateKey,
      args,
      payloadLength,
    };
  }, [storedSettings]);

  const [allowedHosts, setAllowedHosts] = useState(initial.allowedHosts);
  const [port, setPort] = useState(initial.port);
  const [mode, setMode] = useState<SecurityMode>(initial.mode);
  const [ca, setCa] = useState(initial.ca);
  const [certificate, setCertificate] = useState(initial.certificate);
  const [certificateKey, setCertificateKey] = useState(initial.certificateKey);
  const [args, setArgs] = useState<ArgsMode>(initial.args);
  const [payloadLength, setPayloadLength] = useState(initial.payloadLength);

  // Refresh the form whenever the stored snapshot changes (e.g., after the
  // user saves a value elsewhere in the page or the cache refetches).
  useEffect(() => {
    setAllowedHosts(initial.allowedHosts);
    setPort(initial.port);
    setMode(initial.mode);
    setCa(initial.ca);
    setCertificate(initial.certificate);
    setCertificateKey(initial.certificateKey);
    setArgs(initial.args);
    setPayloadLength(initial.payloadLength);
  }, [initial]);

  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<string | null>(null);

  const showCertFields = mode !== "insecure";
  const showCaField = mode === "two-way-tls";

  // Live hints + Apply-blocking validation. Insecure mode has no required
  // fields; Encrypted needs a server certificate (we'll still let an empty
  // value through and emit a warning to mirror the CLI); Two-way TLS needs
  // a CA path so the server can validate client certs.
  const hints: string[] = [];
  if (mode === "encrypted" && !certificate.trim()) {
    hints.push("Encrypted mode is missing a server certificate path.");
  }
  if (mode === "two-way-tls" && !ca.trim()) {
    hints.push("Two-way TLS requires a CA path so client certificates can be verified — Apply is blocked until set.");
  }

  const blockReason = (() => {
    if (mode === "two-way-tls" && !ca.trim()) {
      return "Two-way TLS requires a CA path. Switch to Encrypted to drop client-certificate validation, or to Insecure for legacy NRPE.";
    }
    return null;
  })();

  const onApply = async () => {
    setError(null);
    setResult(null);
    if (blockReason) {
      setError(blockReason);
      return;
    }

    setBusy(true);
    const messages: string[] = [];
    const write = (path: string, key: string, value: string) =>
      updateSettings({ path, key, value }).unwrap();

    try {
      messages.push(`Enabling NRPE via SSL from: ${allowedHosts} on port ${port}.`);

      await write(DEFAULT_PATH, "allowed hosts", allowedHosts);
      await write(MODULES_PATH, "NRPEServer", "enabled");
      await write(NRPE_PATH, "port", port);
      await write(NRPE_PATH, "ssl", "true");

      if (mode === "insecure") {
        messages.push("WARNING: NRPE is currently insecure.");
        await write(NRPE_PATH, "insecure", "true");
        await write(NRPE_PATH, "allowed ciphers", INSECURE_CIPHERS);
        await write(NRPE_PATH, "ssl options", "");
        await write(NRPE_PATH, "verify mode", "");
        await write(NRPE_PATH, "ca", "");
        await write(NRPE_PATH, "certificate", "");
        await write(NRPE_PATH, "certificate key", "");
      } else {
        const verifyValue = mode === "two-way-tls" ? "peer-cert" : "none";
        if (mode === "two-way-tls") {
          messages.push(
            "NRPE is reasonably secure and will require client certificates issued from the configured CA.",
          );
        } else {
          messages.push(
            `WARNING: NRPE has encryption but no client authentication beyond the allowed-hosts list (${allowedHosts}).`,
          );
        }
        if (certificateKey.trim()) {
          messages.push(`Traffic encrypted using ${certificate} and ${certificateKey}.`);
        } else {
          messages.push(`Traffic encrypted using ${certificate}.`);
        }

        await write(NRPE_PATH, "insecure", "false");
        await write(NRPE_PATH, "allowed ciphers", SECURE_CIPHERS);
        await write(NRPE_PATH, "ssl options", SECURE_SSL_OPTIONS);
        await write(NRPE_PATH, "verify mode", verifyValue);
        // CA only matters in two-way TLS — clear it in encrypted mode so a
        // stale value doesn't quietly influence behavior if mode changes back.
        await write(NRPE_PATH, "ca", mode === "two-way-tls" ? ca : "");
        await write(NRPE_PATH, "certificate", certificate);
        await write(NRPE_PATH, "certificate key", certificateKey);
      }

      if (args === "all") {
        messages.push("UNSAFE Arguments are allowed.");
        await write(NRPE_PATH, "allow arguments", "true");
        await write(NRPE_PATH, "allow nasty characters", "true");
      } else if (args === "safe") {
        messages.push("SAFE Arguments are allowed.");
        await write(NRPE_PATH, "allow arguments", "true");
        await write(NRPE_PATH, "allow nasty characters", "false");
      } else {
        messages.push("Arguments are NOT allowed.");
        await write(NRPE_PATH, "allow arguments", "false");
        await write(NRPE_PATH, "allow nasty characters", "false");
      }

      if (payloadLength.trim()) {
        await write(NRPE_PATH, "payload length", payloadLength);
        if (payloadLength !== "1024") {
          messages.push(
            `WARNING: NRPE is using non-standard payload length ${payloadLength} — consider NRPE v3.`,
          );
        }
      }

      setResult(messages.join("\n"));
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };

  return (
    <Card>
      <CardHeader
        title={<Typography variant="h6">Setup NRPE Server</Typography>}
        subheader="Configures the NRPE server end-to-end: transport, TLS, arguments, and the allowed-hosts list. Mirrors the CLI `nrpe-server install` wizard."
      />
      <CardContent>
        <Stack spacing={2}>
          <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
            <TextField
              label="Allowed hosts"
              value={allowedHosts}
              onChange={(e) => setAllowedHosts(e.target.value)}
              fullWidth
              variant="standard"
              helperText="Comma-separated list of hosts allowed to connect."
              disabled={busy}
            />
            <TextField
              label="Port"
              value={port}
              onChange={(e) => setPort(e.target.value)}
              variant="standard"
              sx={{ minWidth: 120 }}
              disabled={busy}
            />
            <TextField
              label="Payload length"
              value={payloadLength}
              onChange={(e) => setPayloadLength(e.target.value)}
              variant="standard"
              sx={{ minWidth: 140 }}
              helperText="Must match clients (default 1024)."
              disabled={busy}
            />
          </Stack>

          <FormControl disabled={busy}>
            <Typography variant="subtitle2" sx={{ mb: 0.5 }}>
              Security mode
            </Typography>
            <RadioGroup
              value={mode}
              onChange={(e) => setMode(e.target.value as SecurityMode)}
            >
              <FormControlLabel
                value="insecure"
                control={<Radio size="small" />}
                label="Insecure — legacy NRPE, no TLS validation, no certificates."
              />
              <FormControlLabel
                value="encrypted"
                control={<Radio size="small" />}
                label="Encrypted — TLS server certificate only, clients are not authenticated."
              />
              <FormControlLabel
                value="two-way-tls"
                control={<Radio size="small" />}
                label="Two-way TLS — server presents a certificate and validates client certificates against a CA."
              />
            </RadioGroup>
          </FormControl>

          {showCertFields && (
            <Stack spacing={2}>
              {showCaField && (
                <TextField
                  label="CA bundle"
                  value={ca}
                  onChange={(e) => setCa(e.target.value)}
                  variant="standard"
                  fullWidth
                  disabled={busy}
                  helperText="Path to the CA used to validate client certificates."
                />
              )}
              <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
                <TextField
                  label="Certificate"
                  value={certificate}
                  onChange={(e) => setCertificate(e.target.value)}
                  variant="standard"
                  fullWidth
                  disabled={busy}
                />
                <TextField
                  label="Certificate key"
                  value={certificateKey}
                  onChange={(e) => setCertificateKey(e.target.value)}
                  variant="standard"
                  fullWidth
                  disabled={busy}
                  helperText="Optional — leave empty if the key is bundled with the certificate."
                />
              </Stack>
            </Stack>
          )}

          <FormControl variant="standard" sx={{ minWidth: 260 }} disabled={busy}>
            <InputLabel id="nrpe-args-label">Arguments</InputLabel>
            <Select
              labelId="nrpe-args-label"
              value={args}
              onChange={(e) => setArgs(e.target.value as ArgsMode)}
            >
              <MenuItem value="false">Disabled — no arguments accepted</MenuItem>
              <MenuItem value="safe">Safe — arguments allowed, no shell metacharacters</MenuItem>
              <MenuItem value="all">Unsafe — any argument allowed (use with caution)</MenuItem>
            </Select>
          </FormControl>

          {hints.map((h, i) => (
            <NscpAlert key={i} severity="warning" text={h} />
          ))}
          {error && <NscpAlert severity="error" text={error} />}
          {result && (
            <Box
              sx={{
                whiteSpace: "pre-wrap",
                fontFamily: "monospace",
                fontSize: 13,
                p: 1.5,
                border: 1,
                borderColor: "divider",
                borderRadius: 1,
              }}
            >
              {result}
            </Box>
          )}

          <Box>
            <Button
              variant="contained"
              onClick={() => void onApply()}
              disabled={busy || !!blockReason}
              loading={busy}
            >
              Apply configuration
            </Button>
          </Box>
        </Stack>
      </CardContent>
    </Card>
  );
}
