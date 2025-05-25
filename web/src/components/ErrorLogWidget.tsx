import { nsclientApi, useGetLogStatusQuery } from "../api/api.ts";
import { useEffect } from "react";
import { useNavigate } from "react-router";
import { useAppDispatch } from "../store/store.ts";
import NscpAlert from "./atoms/NscpAlert.tsx";
import { Box } from "@mui/material";

export default function ErrorLogWidget() {
  const { data: logStatus } = useGetLogStatusQuery();
  const dispatch = useAppDispatch();
  const navigate = useNavigate();
  const errors = logStatus?.errors || 0;

  useEffect(() => {
    const onRefresh = () => dispatch(nsclientApi.util.invalidateTags(["LogStatus", "Logs"]));
    const interval = setInterval(onRefresh, 5_000);
    return () => {
      clearInterval(interval);
    };
  }, [dispatch]);
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
