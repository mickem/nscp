import { Link } from "@mui/material";

/**
 * The control for a list capped by `useCapped`; renders nothing while the
 * whole list is on the page, so a short list carries no control that would
 * do nothing.
 */
export function ShowMore({ hidden, showAll, onToggle }: { hidden: number; showAll: boolean; onToggle: () => void }) {
  if (hidden === 0) return null;
  return (
    <Link
      component="button"
      type="button"
      variant="caption"
      underline="hover"
      onClick={onToggle}
      sx={{ alignSelf: "flex-start", mt: 1, ml: 0.5 }}
    >
      {showAll ? "Show fewer" : `… ${hidden} more`}
    </Link>
  );
}
