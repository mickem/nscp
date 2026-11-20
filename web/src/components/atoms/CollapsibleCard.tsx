import { Box, ButtonBase, Card, CardContent, Collapse, Stack, Typography } from "@mui/material";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import { ReactNode, useId, useState } from "react";

/**
 * An outlined card whose body folds away under its title: the settings cards
 * that sit above a page's data (fact sets, diagnostics tables, the failed-check
 * log) are what you set once and then want out of the way.
 *
 * Whether it is open is remembered per card in this browser, under
 * `storageKey`. That is a convenience only: storage can be missing or refuse
 * (a private window, blocked site data), and the card then simply starts at
 * `defaultExpanded`.
 */

const STORAGE_PREFIX = "nscp.collapsible.";

function readExpanded(storageKey: string, fallback: boolean): boolean {
  try {
    const stored = window.localStorage.getItem(STORAGE_PREFIX + storageKey);
    if (stored === "true") return true;
    if (stored === "false") return false;
  } catch {
    // Storage unavailable: use the default.
  }
  return fallback;
}

function writeExpanded(storageKey: string, expanded: boolean) {
  try {
    window.localStorage.setItem(STORAGE_PREFIX + storageKey, expanded ? "true" : "false");
  } catch {
    // Storage unavailable: the card still toggles, it just is not remembered.
  }
}

interface CollapsibleCardProps {
  title: string;
  /** Names the card in storage; unique per card. */
  storageKey: string;
  defaultExpanded?: boolean;
  children: ReactNode;
}

export default function CollapsibleCard({ title, storageKey, defaultExpanded = true, children }: CollapsibleCardProps) {
  const [expanded, setExpanded] = useState(() => readExpanded(storageKey, defaultExpanded));
  const bodyId = useId();

  const toggle = () => {
    const next = !expanded;
    setExpanded(next);
    writeExpanded(storageKey, next);
  };

  return (
    <Card variant="outlined">
      <ButtonBase
        onClick={toggle}
        aria-expanded={expanded}
        aria-controls={bodyId}
        sx={{ width: "100%", justifyContent: "stretch", textAlign: "left", px: 2, py: 1.5 }}
      >
        <Stack direction="row" sx={{ alignItems: "center", width: "100%" }}>
          <Typography variant="h6" component="div" sx={{ flexGrow: 1 }}>
            {title}
          </Typography>
          <ExpandMoreIcon
            sx={{
              color: "text.secondary",
              transition: (theme) => theme.transitions.create("transform"),
              transform: expanded ? "rotate(180deg)" : "none",
            }}
          />
        </Stack>
      </ButtonBase>
      <Collapse in={expanded} unmountOnExit>
        <Box id={bodyId}>
          <CardContent sx={{ pt: 0 }}>{children}</CardContent>
        </Box>
      </Collapse>
    </Card>
  );
}
