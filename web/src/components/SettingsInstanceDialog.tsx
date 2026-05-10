import { SettingsDescription } from "../api/api.ts";
import {
  Box,
  Button,
  Chip,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  Stack,
  Tab,
  Tabs,
  Typography,
} from "@mui/material";
import BookmarkBorderIcon from "@mui/icons-material/BookmarkBorder";
import { useMemo, useState } from "react";
import SettingsField from "./atoms/SettingsField.tsx";
import InstanceNameField from "./atoms/InstanceNameField.tsx";

export interface FieldGroup {
  name: string;
  keys: string[]; // ordered keys belonging to this group
}

interface Props {
  open: boolean;
  onClose: () => void;
  instanceName: string;
  instancePath: string;
  fields: SettingsDescription[];
  isTemplate: boolean;
  isDefaultTemplate: boolean;
  parentOptions: string[];
  groups: FieldGroup[];
  isFieldVisible: (key: string) => boolean;
  existingNames: string[];
  onRename: (oldPath: string, oldName: string, newName: string) => Promise<void>;
}

export default function SettingsInstanceDialog({
  open,
  onClose,
  instanceName,
  instancePath,
  fields,
  isTemplate,
  isDefaultTemplate,
  parentOptions,
  groups,
  isFieldVisible,
  existingNames,
  onRename,
}: Props) {
  const [tabIdx, setTabIdx] = useState(0);

  // Compute the field bucket for each group, then sweep any leftover schema
  // fields into a final "Other" tab so nothing is hidden by accident.
  const tabs = useMemo(() => {
    const used = new Set<string>();
    const visibleFields = fields.filter((f) => isFieldVisible(f.key));
    const out: { name: string; fields: SettingsDescription[] }[] = [];
    for (const g of groups) {
      const bucket: SettingsDescription[] = [];
      for (const key of g.keys) {
        const f = visibleFields.find((vf) => vf.key === key);
        if (f) {
          bucket.push(f);
          used.add(key);
        }
      }
      if (bucket.length > 0) out.push({ name: g.name, fields: bucket });
    }
    const leftovers = visibleFields.filter((f) => !used.has(f.key));
    if (leftovers.length > 0) {
      out.push({ name: groups.length === 0 ? "Settings" : "Other", fields: leftovers });
    }
    return out;
  }, [fields, isFieldVisible, groups]);

  const safeIdx = Math.min(tabIdx, Math.max(0, tabs.length - 1));
  const current = tabs[safeIdx];

  // Derived from the largest tab so the dialog body doesn't shrink/grow as the
  // user switches between tabs. Two-column layout, ~96 px per row including
  // helper text + caption; clamped so very large groups don't blow the dialog
  // off the viewport.
  const maxRows = Math.max(1, ...tabs.map((t) => Math.ceil(t.fields.length / 2)));
  const minContentHeight = Math.min(640, 80 + maxRows * 96);

  return (
    <Dialog open={open} onClose={onClose} maxWidth="md" fullWidth>
      <DialogTitle>
        <Stack direction="row" alignItems="center" spacing={1}>
          <InstanceNameField
            name={instanceName}
            path={instancePath}
            isDefaultTemplate={isDefaultTemplate}
            existingNames={existingNames.filter((n) => n !== instanceName)}
            onRename={onRename}
          />
          {isTemplate && (
            <Chip
              size="small"
              icon={<BookmarkBorderIcon fontSize="small" />}
              label="Template"
              color="secondary"
              variant="outlined"
            />
          )}
          <Typography variant="caption" color="text.secondary" sx={{ fontFamily: "monospace" }}>
            {instancePath}
          </Typography>
        </Stack>
      </DialogTitle>
      {tabs.length > 1 && (
        <Tabs
          value={safeIdx}
          onChange={(_e, v) => setTabIdx(v)}
          variant="scrollable"
          scrollButtons="auto"
          sx={{ borderBottom: 1, borderColor: "divider" }}
        >
          {tabs.map((t) => (
            <Tab key={t.name} label={t.name} />
          ))}
        </Tabs>
      )}
      <DialogContent
        sx={{
          // Reserve enough vertical space for the largest tab so the dialog
          // doesn't jump (and the tab strip doesn't move) as the user clicks
          // through Source / Filter / Syntax / Target / Other.
          minHeight: minContentHeight,
        }}
      >
        {current ? (
          <Box
            sx={{
              display: "grid",
              gridTemplateColumns: { xs: "1fr", md: "1fr 1fr" },
              columnGap: 3,
              rowGap: 2,
              pt: 1,
            }}
          >
            {current.fields.map((f) => (
              <SettingsField
                key={f.key}
                path={instancePath}
                description={f}
                parentOptions={f.key === "parent" ? parentOptions : undefined}
                forceDefault={isDefaultTemplate}
              />
            ))}
          </Box>
        ) : (
          <Typography variant="body2" color="text.secondary">
            No editable fields.
          </Typography>
        )}
      </DialogContent>
      <DialogActions>
        {tabs.length > 1 && (
          <>
            <Button onClick={() => setTabIdx(Math.max(0, safeIdx - 1))} disabled={safeIdx === 0}>
              Previous
            </Button>
            <Button
              onClick={() => setTabIdx(Math.min(tabs.length - 1, safeIdx + 1))}
              disabled={safeIdx === tabs.length - 1}
            >
              Next
            </Button>
            <Box sx={{ flexGrow: 1 }} />
          </>
        )}
        <Button onClick={onClose} variant="contained">
          Close
        </Button>
      </DialogActions>
    </Dialog>
  );
}
