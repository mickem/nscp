import { Alert } from "@mui/material";
import ErrorIcon from "@mui/icons-material/Error";
import WarningIcon from "@mui/icons-material/Warning";
import InfoIcon from "@mui/icons-material/Info";
import DoneIcon from "@mui/icons-material/Done";
import Button from "@mui/material/Button";

const ICONS = {
  success: <DoneIcon fontSize="inherit" />,
  info: <InfoIcon fontSize="inherit" />,
  warning: <WarningIcon fontSize="inherit" />,
  error: <ErrorIcon fontSize="inherit" />,
};

interface Props {
  severity: "success" | "info" | "warning" | "error";
  text: string;
  actionTitle?: string;
  onClick?: () => void;
}

export default function NscpAlert({ severity, text, actionTitle, onClick }: Props) {
  return (
    <Alert
      icon={ICONS[severity]}
      severity={severity}
      action={
        <>
          {actionTitle && (
            <Button color="inherit" size="small" onClick={onClick}>
              {actionTitle}
            </Button>
          )}
        </>
      }
    >
      {text}
    </Alert>
  );
}
