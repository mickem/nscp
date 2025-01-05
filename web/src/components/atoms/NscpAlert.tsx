import { Alert } from "@mui/material";
import ErrorIcon from "@mui/icons-material/Error";
import WarningIcon from "@mui/icons-material/Warning";
import InfoIcon from "@mui/icons-material/Info";
import DoneIcon from "@mui/icons-material/Done";

const ICONS = {
  success: <DoneIcon />,
  info: <InfoIcon />,
  warning: <WarningIcon />,
  error: <ErrorIcon />,
};

interface Props {
  severity: "success" | "info" | "warning" | "error";
  text: string;
}
export default function NscpAlert({ severity, text }: Props) {
  return (
    <Alert icon={ICONS[severity]} severity={severity}>
      {text}
    </Alert>
  );
}
