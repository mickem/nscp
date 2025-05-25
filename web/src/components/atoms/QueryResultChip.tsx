import { Chip } from "@mui/material";

interface Props {
  result: 0 | 1 | 2 | 3;
}

export function QueryResultChip({ result }: Props) {
  if (result === 0) {
    return <Chip size="small" label="Ok" color="success" />;
  }
  if (result === 1) {
    return <Chip size="small" label="Warning" color="warning" />;
  }
  if (result === 2) {
    return <Chip size="small" label="Critical" color="error" />;
  }
  return <Chip size="small" label="Unknown" color="secondary" />;
}
