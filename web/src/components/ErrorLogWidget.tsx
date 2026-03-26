import { useGetLogStatusQuery } from "../api/api.ts";
import { useNavigate } from "react-router";
import NscpAlert from "./atoms/NscpAlert.tsx";
import { Box } from "@mui/material";
import { useAppSelector } from "../store/store.ts";

export default function ErrorLogWidget() {
  const refreshRate = useAppSelector((state) => state.dashboard.refreshRate);
  const { data: logStatus } = useGetLogStatusQuery(undefined, {
    pollingInterval: refreshRate || undefined,
  });
  const navigate = useNavigate();
  const errors = logStatus?.errors || 0;

  const lastError = logStatus?.last_error || "No errors found";

  const viewLogs = () => {
    navigate("/logs");
  };

  return (
    <>
      {errors > 0 && (
        <Box sx={{ paddingBottom: 3 }}>
          <NscpAlert severity="error" text={"Errors found: " + lastError} actionTitle="Show log" onClick={viewLogs} />
        </Box>
      )}
    </>
  );
}
