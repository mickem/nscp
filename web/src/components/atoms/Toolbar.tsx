import React from "react";
import { Paper, Stack } from "@mui/material";
import { SxProps } from "@mui/system";
import { Theme } from "@mui/material/styles";

interface Props {
  children: React.ReactNode;
  sx?: SxProps<Theme>;
}

export function Toolbar({ children, sx = {} }: Props) {
  return (
    <Paper elevation={1} sx={sx}>
      <Stack direction="row" alignItems={"center"} sx={{ p: 1 }} spacing={1}>
        {children}
      </Stack>
    </Paper>
  );
}
