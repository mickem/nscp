import React from "react";
import { Stack } from "@mui/material";
import { SxProps } from "@mui/system";
import { Theme } from "@mui/material/styles";

interface Props {
  children: React.ReactNode;
  sx?: SxProps<Theme>;
}

export function Toolbar({ children, sx = {} }: Props) {
  return (
    <Stack
      direction="row"
      alignItems="center"
      spacing={1}
      sx={{
        px: 2,
        py: 1,
        mb: 2,
        borderBottom: 1,
        borderColor: "divider",
        ...sx as object,
      }}
    >
      {children}
    </Stack>
  );
}
