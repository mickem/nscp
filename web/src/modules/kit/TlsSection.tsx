// The certificate fields of a TLS server. Controlled: the page owns the values
// and turns them into changes with its own recipe.
import { Stack, TextField } from "@mui/material";

export interface TlsFiles {
  certificate: string;
  certificateKey: string;
  ca: string;
}

interface Props {
  value: TlsFiles;
  onChange: (value: TlsFiles) => void;
  // Show the CA used to verify client certificates (two-way TLS only).
  showCa: boolean;
  disabled?: boolean;
}

export default function TlsSection({ value, onChange, showCa, disabled }: Props) {
  const set = (field: keyof TlsFiles) => (e: { target: { value: string } }) =>
    onChange({ ...value, [field]: e.target.value });
  return (
    <Stack spacing={2}>
      <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
        <TextField
          label="Server certificate"
          value={value.certificate}
          onChange={set("certificate")}
          variant="standard"
          fullWidth
          disabled={disabled}
          helperText="Presented to clients so they can verify this agent."
        />
        <TextField
          label="Certificate key"
          value={value.certificateKey}
          onChange={set("certificateKey")}
          variant="standard"
          fullWidth
          disabled={disabled}
          helperText="Leave empty if the key is in the certificate file."
        />
      </Stack>
      {showCa && (
        <TextField
          label="Client CA"
          value={value.ca}
          onChange={set("ca")}
          variant="standard"
          fullWidth
          disabled={disabled}
          helperText="Clients must present a certificate issued by this CA."
        />
      )}
    </Stack>
  );
}
