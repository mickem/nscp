import { Chip, Tooltip } from "@mui/material";
import ScienceIcon from "@mui/icons-material/Science";

// What "experimental" means to the person reading the list: the thing works,
// but it is young enough that its options, keywords and output may still
// change. Modules and check commands declare it in their module.json and the
// agent reports it through the registry.
export const EXPERIMENTAL_TOOLTIP =
  "Experimental: usable, but its options, keywords and output may change in a future release.";

interface Props {
  /** Rendered as a compact label (lists) rather than a full chip (headers). */
  dense?: boolean;
}

export default function ExperimentalChip({ dense = false }: Props) {
  return (
    <Tooltip title={EXPERIMENTAL_TOOLTIP} arrow>
      <Chip
        label="Experimental"
        aria-label="Experimental"
        size="small"
        color="warning"
        variant="outlined"
        icon={dense ? undefined : <ScienceIcon />}
      />
    </Tooltip>
  );
}
