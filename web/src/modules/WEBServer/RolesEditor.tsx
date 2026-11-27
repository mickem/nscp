// The web server's roles: name -> comma-separated grants, edited with a
// grant picker. Saved one role at a time, as the other lists are.
import {
  Button,
  Card,
  CardContent,
  CardHeader,
  Chip,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  IconButton,
  Menu,
  MenuItem,
  ListItemText,
  Stack,
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableRow,
  TextField,
  Tooltip,
  Typography,
} from "@mui/material";
import AddIcon from "@mui/icons-material/Add";
import DeleteOutlineIcon from "@mui/icons-material/DeleteOutlined";
import EditIcon from "@mui/icons-material/Edit";
import { useMemo, useState } from "react";
import {
  useDeleteSettingsMutation,
  useGetSettingsQuery,
  useUpdateSettingsMutation,
} from "../../api/api.ts";
import NscpAlert from "../../components/atoms/NscpAlert.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import type { KeyValueEditorProps } from "../types.ts";
import GrantPicker from "./GrantPicker.tsx";
import { formatGrants, parseGrants, powerfulHeld, ROLE_PRESETS } from "./grants.ts";

const USERS = "/settings/WEB/server/users/";

function summary(grants: string[]): string {
  if (grants.includes("*")) return "Everything";
  if (grants.length <= 3) return grants.join(", ") || "Nothing";
  return `${grants.slice(0, 3).join(", ")} and ${grants.length - 3} more`;
}

interface Editing {
  name: string;
  grants: string[];
  isNew: boolean;
}

export default function RolesEditor({ collectionPath, descriptions }: KeyValueEditorProps) {
  const { data: stored } = useGetSettingsQuery();
  const [updateSettings] = useUpdateSettingsMutation();
  const [deleteSettings] = useDeleteSettingsMutation();
  const [editing, setEditing] = useState<Editing | null>(null);
  const [deleting, setDeleting] = useState<string | null>(null);
  const [menu, setMenu] = useState<HTMLElement | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const info = descriptions.find((s) => s.path === collectionPath && s.key === "");
  const roles = useMemo(() => {
    const byName = new Map<string, string>();
    for (const s of stored ?? [])
      if (s.path === collectionPath && s.key !== "") byName.set(s.key, s.value);
    return [...byName.entries()].sort(([a], [b]) => a.localeCompare(b));
  }, [stored, collectionPath]);
  // Which users hold each role.
  const usersOf = useMemo(() => {
    const out = new Map<string, string[]>();
    for (const s of stored ?? []) {
      if (!s.path.startsWith(USERS) || s.key !== "role") continue;
      const user = s.path.slice(USERS.length);
      out.set(s.value, [...(out.get(s.value) ?? []), user]);
    }
    return out;
  }, [stored]);

  const open = (e: Editing) => {
    setError(null);
    setEditing(e);
  };
  const save = async () => {
    if (!editing) return;
    const name = editing.name.trim();
    if (!name || name.includes("/")) return setError("A role needs a name without '/'.");
    if (editing.isNew && roles.some(([n]) => n === name))
      return setError("A role with that name exists.");
    setBusy(true);
    try {
      // An empty grant list would delete the key; store a role that grants
      // nothing as a single space, as the generic list does.
      const value = formatGrants(editing.grants) || " ";
      await updateSettings({ path: collectionPath, key: name, value }).unwrap();
      setEditing(null);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };
  const remove = async () => {
    if (!deleting) return;
    setBusy(true);
    try {
      await deleteSettings({ path: collectionPath, key: deleting }).unwrap();
      setDeleting(null);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };

  const powerful = editing ? powerfulHeld(editing.grants) : [];

  return (
    <Card>
      <CardHeader
        title={<Typography variant="h6">{info?.title || "Roles"}</Typography>}
        subheader="What each role may do. A user holds one role."
        action={
          <>
            <Button startIcon={<AddIcon />} onClick={(e) => setMenu(e.currentTarget)}>
              Add role
            </Button>
            <Menu anchorEl={menu} open={menu !== null} onClose={() => setMenu(null)}>
              <MenuItem
                onClick={() => {
                  setMenu(null);
                  open({ name: "", grants: ["public", "login.get"], isNew: true });
                }}
              >
                <ListItemText primary="Empty role" secondary="Log in and the API index only" />
              </MenuItem>
              {ROLE_PRESETS.map((p) => (
                <MenuItem
                  key={p.name}
                  onClick={() => {
                    setMenu(null);
                    const taken = new Set(roles.map(([n]) => n));
                    let name = p.name;
                    for (let i = 2; taken.has(name); ++i) name = `${p.name}_${i}`;
                    open({ name, grants: parseGrants(p.grants), isNew: true });
                  }}
                >
                  <ListItemText primary={`Like ${p.name}`} secondary={p.label} />
                </MenuItem>
              ))}
            </Menu>
          </>
        }
      />
      <CardContent sx={{ p: 0, "&:last-child": { pb: 0 } }}>
        {roles.length === 0 ? (
          <Typography variant="body2" color="text.secondary" sx={{ px: 2, pb: 2 }}>
            No roles are defined.
          </Typography>
        ) : (
          <Table size="small" aria-label="Roles">
            <TableHead>
              <TableRow>
                <TableCell>Role</TableCell>
                <TableCell>Grants</TableCell>
                <TableCell>Users</TableCell>
                <TableCell />
              </TableRow>
            </TableHead>
            <TableBody>
              {roles.map(([name, value]) => {
                const grants = parseGrants(value);
                const risky = powerfulHeld(grants).length > 0;
                return (
                  <TableRow key={name} hover>
                    <TableCell sx={{ fontFamily: "monospace" }}>{name}</TableCell>
                    <TableCell>
                      <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
                        <Typography variant="body2">{summary(grants)}</Typography>
                        {risky && (
                          <Chip
                            size="small"
                            color="warning"
                            variant="outlined"
                            label="powerful grants"
                          />
                        )}
                      </Stack>
                    </TableCell>
                    <TableCell>
                      <Typography variant="body2" color="text.secondary">
                        {(usersOf.get(name) ?? []).join(", ") || "-"}
                      </Typography>
                    </TableCell>
                    <TableCell align="right" sx={{ whiteSpace: "nowrap" }}>
                      <Tooltip title="Edit">
                        <IconButton
                          size="small"
                          onClick={() => open({ name, grants, isNew: false })}
                        >
                          <EditIcon fontSize="small" />
                        </IconButton>
                      </Tooltip>
                      <Tooltip title="Delete">
                        <IconButton size="small" onClick={() => setDeleting(name)}>
                          <DeleteOutlineIcon fontSize="small" />
                        </IconButton>
                      </Tooltip>
                    </TableCell>
                  </TableRow>
                );
              })}
            </TableBody>
          </Table>
        )}
      </CardContent>

      <Dialog
        open={editing !== null}
        onClose={() => !busy && setEditing(null)}
        maxWidth="md"
        fullWidth
      >
        <DialogTitle>{editing?.isNew ? "Add role" : `Role ${editing?.name}`}</DialogTitle>
        <DialogContent>
          <Stack spacing={2} sx={{ pt: 1 }}>
            {editing?.isNew && (
              <TextField
                label="Name"
                value={editing.name}
                onChange={(e) => setEditing({ ...editing, name: e.target.value })}
                variant="standard"
                autoFocus
              />
            )}
            {powerful.length > 0 && (
              <RiskBanner
                risk={{
                  severity: "warning",
                  title: "This role holds powerful grants",
                  reason: `It holds: ${powerful.join(", ")}. Give it only to callers you trust with that.`,
                }}
              />
            )}
            {editing && (
              <GrantPicker
                value={editing.grants}
                onChange={(grants) => setEditing({ ...editing, grants })}
                disabled={busy}
              />
            )}
            {error && <NscpAlert severity="error" text={error} />}
          </Stack>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setEditing(null)} disabled={busy}>
            Cancel
          </Button>
          <Button variant="contained" onClick={() => void save()} loading={busy}>
            Save
          </Button>
        </DialogActions>
      </Dialog>

      <Dialog open={deleting !== null} onClose={() => !busy && setDeleting(null)}>
        <DialogTitle>Delete role {deleting}?</DialogTitle>
        <DialogContent>
          {deleting && (usersOf.get(deleting) ?? []).length > 0 ? (
            <NscpAlert
              severity="warning"
              text={`${(usersOf.get(deleting) ?? []).join(", ")} hold this role and will be able to do nothing until given another.`}
            />
          ) : (
            <Typography variant="body2">No user holds this role.</Typography>
          )}
          {error && <NscpAlert severity="error" text={error} />}
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setDeleting(null)} disabled={busy}>
            Cancel
          </Button>
          <Button color="error" variant="contained" onClick={() => void remove()} loading={busy}>
            Delete
          </Button>
        </DialogActions>
      </Dialog>
    </Card>
  );
}
