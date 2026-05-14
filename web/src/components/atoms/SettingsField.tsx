import {
  MetadataOption,
  SettingsDescription,
  useGetChannelMetadataQuery,
  useGetCounterMetadataQuery,
  useGetSettingsQuery,
  useUpdateSettingsMutation,
} from "../../api/api.ts";
import {
  Autocomplete,
  Box,
  CircularProgress,
  createFilterOptions,
  FormControl,
  FormControlLabel,
  FormGroup,
  IconButton,
  InputAdornment,
  MenuItem,
  Stack,
  Switch,
  TextField,
  Tooltip,
  Typography,
} from "@mui/material";
import HelpOutlineIcon from "@mui/icons-material/HelpOutline";
import CheckIcon from "@mui/icons-material/Check";
import { useEffect, useMemo, useState } from "react";
import {
  COUNTER_FLAGS_OPTIONS,
  EnumOption,
  inferOptionsFromParens,
  resolveEnumOptions,
} from "./fieldEnums.ts";

interface Props {
  path: string;
  description: SettingsDescription;
  // When the field is a "parent" reference, the caller can supply the list of
  // template names that the user is allowed to inherit from. If omitted the
  // field falls back to a free-form text/select inferred from the description.
  parentOptions?: string[];
  // When true, the field belongs to the root "default" template: empty values
  // are not allowed (no "(inherit default)" option), and the default_value
  // backfills any unset stored value.
  forceDefault?: boolean;
}

// Some schema entries share the same generic title (e.g. "SYNTAX" is used
// by `top syntax`, `ok syntax`, `detail syntax`), which makes the rendered
// labels look duplicated. Override per key when needed.
const LABEL_OVERRIDES: Record<string, string> = {
  "top syntax": "Top syntax",
  "ok syntax": "OK syntax",
  "detail syntax": "Detail syntax",
};

export default function SettingsField({
  path,
  description,
  parentOptions,
  forceDefault = false,
}: Props) {
  const initial = forceDefault
    ? description.value || description.default_value || ""
    : (description.value ?? "");
  const [value, setValue] = useState(initial);
  const [dirty, setDirty] = useState(false);
  const [saving, setSaving] = useState(false);
  const [updateSettings] = useUpdateSettingsMutation();

  useEffect(() => {
    setValue(initial);
    setDirty(false);
  }, [initial]);

  const persist = async (next: string) => {
    setSaving(true);
    try {
      await updateSettings({ path, key: description.key, value: next }).unwrap();
      setDirty(false);
    } finally {
      setSaving(false);
    }
  };

  const onChange = (next: string) => {
    setValue(next);
    setDirty(next !== initial);
  };

  const onSelectChange = (next: string) => {
    // On the default template (forceDefault), an empty value isn't legal —
    // picking the "reset" option means "snap to the schema default" rather
    // than "leave it blank".
    const effective =
      forceDefault && next === "" && description.default_value
        ? description.default_value
        : next;
    setValue(effective);
    setDirty(false);
    if (effective !== initial) persist(effective);
  };

  const label = LABEL_OVERRIDES[description.key] ?? (description.title || description.key);
  const help = description.description;
  const enumOpts: EnumOption[] | undefined = resolveEnumOptions(description, path, parentOptions);

  const helpAdornment = help ? (
    <Tooltip title={help} arrow>
      <HelpOutlineIcon fontSize="small" sx={{ color: "text.secondary" }} />
    </Tooltip>
  ) : null;

  // PDH counter picker — backed by /v1/metadata/counters. List can be very
  // long, so we cap rendered options and rely on the user typing to filter.
  if (description.key === "counter") {
    return (
      <MetadataAutocompleteField
        path={path}
        description={description}
        useMetadataQuery={useGetCounterMetadataQuery}
        emptyHint="Counter list unavailable — type the counter path manually."
        nounSingular="counter"
        nounPlural="counters"
      />
    );
  }
  // Channel picker — backed by /v2/metadata/channels/.
  if (description.key === "destination") {
    return (
      <MetadataAutocompleteField
        path={path}
        description={description}
        useMetadataQuery={useGetChannelMetadataQuery}
        emptyHint="Channel list unavailable — type the channel name manually."
        nounSingular="channel"
        nounPlural="channels"
      />
    );
  }

  // Multi-select flags field — options taken from the description parenthetical
  // when present, otherwise the well-known counter flags list.
  if (description.key === "flags") {
    const opts = inferOptionsFromParens(description.description) ?? COUNTER_FLAGS_OPTIONS;
    return <MultiFlagsField path={path} description={description} options={opts} />;
  }

  // Realtime memory filters accept any combination of memory types as a
  // comma-separated list, so render the `type` field as switches rather than
  // a single-select dropdown. CPU still uses single-select.
  if (
    description.key === "type" &&
    /\/system\/windows\/real-time\/memory\//.test(path)
  ) {
    return (
      <MultiFlagsField
        path={path}
        description={description}
        options={["physical", "committed", "virtual"]}
      />
    );
  }

  // `verify mode` — accepts free text but exposes the two common picks
  // (`none`, `peer-cert`) as quick options. The schema lets users combine
  // multiple modes as a comma-separated list, so freeSolo > strict enum.
  if (description.key === "verify mode") {
    return (
      <FreeSoloSelectField
        path={path}
        description={description}
        options={["none", "peer-cert"]}
      />
    );
  }

  // Role field on a WEB user — populate from existing non-template roles.
  if (
    description.key === "role" &&
    /^\/settings\/WEB\/server\/users\/[^/]+$/.test(path)
  ) {
    return <RoleSelectField path={path} description={description} />;
  }

  if (description.type === "bool") {
    const checked = value.toLowerCase() === "true" || value === "1";
    return (
      <Stack direction="row" alignItems="center" spacing={1}>
        <FormControlLabel
          control={
            <Switch
              checked={checked}
              onChange={(e) => {
                const next = e.target.checked ? "true" : "false";
                setValue(next);
                persist(next);
              }}
              disabled={saving}
            />
          }
          label={label}
        />
        {helpAdornment}
      </Stack>
    );
  }

  return (
    <Box sx={{ width: "100%" }}>
      <FormControl variant="standard" fullWidth>
        <TextField
          variant="standard"
          select={!!enumOpts}
          label={
            <Stack direction="row" spacing={0.5} alignItems="center" component="span">
              <span>{label}</span>
              {helpAdornment}
            </Stack>
          }
          type={description.type === "password" ? "password" : "text"}
          value={value}
          onChange={(e) => (enumOpts ? onSelectChange(e.target.value) : onChange(e.target.value))}
          onBlur={() => {
            if (enumOpts) return;
            // For root-default fields, an empty value isn't allowed: snap back
            // to the description's default and persist that instead.
            if (forceDefault && value === "" && description.default_value) {
              setValue(description.default_value);
              if (description.default_value !== initial) persist(description.default_value);
              return;
            }
            if (dirty) persist(value);
          }}
          disabled={saving}
          slotProps={{
            input: dirty
              ? {
                  endAdornment: (
                    <InputAdornment position="end">
                      <IconButton size="small" onClick={() => persist(value)} disabled={saving}>
                        <CheckIcon fontSize="small" color="primary" />
                      </IconButton>
                    </InputAdornment>
                  ),
                }
              : undefined,
          }}
        >
          {enumOpts && (!forceDefault || !!description.default_value) && (
            <MenuItem value="">
              <em style={{ color: "rgba(255,255,255,0.6)" }}>
                {forceDefault
                  ? `(reset to default — ${description.default_value})`
                  : description.default_value
                    ? `(inherit default — ${description.default_value})`
                    : "(unset)"}
              </em>
            </MenuItem>
          )}
          {enumOpts?.map((opt) => (
            <MenuItem key={opt.value} value={opt.value}>
              {opt.label}
            </MenuItem>
          ))}
        </TextField>
        {description.default_value && (
          <Typography variant="caption" color="text.secondary" sx={{ mt: 0.25 }}>
            default: {description.default_value}
          </Typography>
        )}
      </FormControl>
    </Box>
  );
}

function MultiFlagsField({
  path,
  description,
  options,
}: Props & { options: string[] }) {
  const parse = (raw: string) =>
    new Set(
      raw
        .split(",")
        .map((s) => s.trim())
        .filter(Boolean),
    );
  const [selected, setSelected] = useState<Set<string>>(parse(description.value ?? ""));
  const [saving, setSaving] = useState(false);
  const [updateSettings] = useUpdateSettingsMutation();

  useEffect(() => {
    setSelected(parse(description.value ?? ""));
  }, [description.value]);

  const persist = async (next: Set<string>) => {
    // Preserve the original option order when serializing.
    const value = options.filter((o) => next.has(o)).join(",");
    setSaving(true);
    try {
      await updateSettings({ path, key: description.key, value }).unwrap();
    } finally {
      setSaving(false);
    }
  };

  const toggle = (opt: string, on: boolean) => {
    const next = new Set(selected);
    if (on) next.add(opt);
    else next.delete(opt);
    setSelected(next);
    persist(next);
  };

  const label = LABEL_OVERRIDES[description.key] ?? (description.title || description.key);
  const help = description.description;
  const helpAdornment = help ? (
    <Tooltip title={help} arrow>
      <HelpOutlineIcon fontSize="small" sx={{ color: "text.secondary" }} />
    </Tooltip>
  ) : null;

  return (
    <Box sx={{ width: "100%" }}>
      <Stack direction="row" spacing={0.5} alignItems="center" sx={{ mb: 0.5 }}>
        <Typography variant="caption" color="text.secondary">
          {label}
        </Typography>
        {helpAdornment}
      </Stack>
      <FormGroup row sx={{ gap: 2 }}>
        {options.map((opt) => (
          <FormControlLabel
            key={opt}
            control={
              <Switch
                size="small"
                checked={selected.has(opt)}
                onChange={(e) => toggle(opt, e.target.checked)}
                disabled={saving}
              />
            }
            label={opt}
          />
        ))}
      </FormGroup>
    </Box>
  );
}

// Filter on the displayed label so users can find an option by either its
// stored value (the raw name) or any text in the parenthesised metadata.
const metadataFilter = createFilterOptions<MetadataOption>({
  limit: 200,
  stringify: (o) => o.label,
});

interface MetadataAutocompleteProps extends Props {
  useMetadataQuery: () => {
    data?: MetadataOption[];
    isFetching: boolean;
    error?: unknown;
  };
  emptyHint: string;
  nounSingular: string;
  nounPlural: string;
}

function MetadataAutocompleteField({
  path,
  description,
  useMetadataQuery,
  emptyHint,
  nounPlural,
}: MetadataAutocompleteProps) {
  const initial = description.value ?? "";
  const [value, setValue] = useState(initial);
  const [saving, setSaving] = useState(false);
  const [updateSettings] = useUpdateSettingsMutation();
  const { data: options, isFetching, error } = useMetadataQuery();

  useEffect(() => {
    setValue(initial);
  }, [initial]);

  const persist = async (next: string) => {
    setSaving(true);
    try {
      await updateSettings({ path, key: description.key, value: next }).unwrap();
    } finally {
      setSaving(false);
    }
  };

  const label = LABEL_OVERRIDES[description.key] ?? (description.title || description.key);
  const help = description.description;
  const helpAdornment = help ? (
    <Tooltip title={help} arrow>
      <HelpOutlineIcon fontSize="small" sx={{ color: "text.secondary" }} />
    </Tooltip>
  ) : null;

  // Resolve the current string `value` to a matching option when one exists,
  // so the input renders the selected option's label rather than the raw key.
  const matched = options?.find((o) => o.value === value);

  return (
    <Box sx={{ width: "100%" }}>
      <Autocomplete<MetadataOption, false, false, true>
        freeSolo
        size="small"
        options={options ?? []}
        value={matched ?? value}
        loading={isFetching}
        loadingText={`Loading ${nounPlural}…`}
        filterOptions={metadataFilter}
        getOptionLabel={(o) => (typeof o === "string" ? o : o.label)}
        isOptionEqualToValue={(o, v) =>
          typeof v === "string" ? o.value === v : o.value === v.value
        }
        onChange={(_e, next) => {
          const v = next == null ? "" : typeof next === "string" ? next : next.value;
          setValue(v);
          if (v !== initial) persist(v);
        }}
        onInputChange={(_e, next, reason) => {
          // Ignore the "reset" reason fired after selection (which would
          // otherwise overwrite our value with the formatted label).
          if (reason !== "reset") setValue(next);
        }}
        onBlur={() => {
          if (value !== initial) persist(value);
        }}
        disabled={saving}
        renderInput={(params) => (
          <TextField
            {...params}
            variant="standard"
            label={
              <Stack direction="row" spacing={0.5} alignItems="center" component="span">
                <span>{label}</span>
                {helpAdornment}
              </Stack>
            }
            slotProps={{
              input: {
                ...params.InputProps,
                endAdornment: (
                  <>
                    {isFetching ? <CircularProgress size={16} /> : null}
                    {params.InputProps.endAdornment}
                  </>
                ),
              },
            }}
          />
        )}
      />
      <Typography variant="caption" color="text.secondary" sx={{ mt: 0.25 }}>
        {error
          ? emptyHint
          : options
            ? `${options.length.toLocaleString()} ${nounPlural} available · type to filter`
            : ""}
      </Typography>
    </Box>
  );
}

function RoleSelectField({ path, description }: Pick<Props, "path" | "description">) {
  const initial = description.value ?? "";
  const [value, setValue] = useState(initial);
  const [saving, setSaving] = useState(false);
  const [updateSettings] = useUpdateSettingsMutation();
  const { data: storedSettings, isFetching } = useGetSettingsQuery();

  useEffect(() => {
    setValue(initial);
  }, [initial]);

  const roleNames = useMemo<string[]>(() => {
    if (!storedSettings) return [];
    const rolesRoot = "/settings/WEB/server/roles";
    const prefix = rolesRoot + "/";
    // First pass: discover candidate role names from pointer entries on the
    // collection root plus any direct child path that holds a stored key.
    const names = new Set<string>();
    for (const s of storedSettings) {
      if (s.path === rolesRoot && s.key !== "") names.add(s.key);
      if (s.path.startsWith(prefix) && s.key !== "") {
        const rest = s.path.slice(prefix.length);
        if (!rest.includes("/")) names.add(rest);
      }
    }
    // Second pass: drop any name whose stored config flags it as a template,
    // and drop the conventional "default" template that the schema always
    // surfaces even without explicit stored values.
    const isTemplate = (name: string) => {
      if (name === "default") return true;
      const instPath = prefix + name;
      return storedSettings.some(
        (s) =>
          s.path === instPath &&
          s.key === "is template" &&
          (s.value ?? "").toLowerCase() === "true",
      );
    };
    return [...names].filter((n) => !isTemplate(n)).sort((a, b) => a.localeCompare(b));
  }, [storedSettings]);

  const persist = async (next: string) => {
    setSaving(true);
    try {
      await updateSettings({ path, key: description.key, value: next }).unwrap();
    } finally {
      setSaving(false);
    }
  };

  const onSelectChange = (next: string) => {
    setValue(next);
    if (next !== initial) persist(next);
  };

  const label = LABEL_OVERRIDES[description.key] ?? (description.title || description.key);
  const help = description.description;
  const helpAdornment = help ? (
    <Tooltip title={help} arrow>
      <HelpOutlineIcon fontSize="small" sx={{ color: "text.secondary" }} />
    </Tooltip>
  ) : null;

  // Keep the current value visible even if it points at a role that no longer
  // exists (or hasn't loaded yet) so we don't silently change the saved value.
  const showOrphan = value !== "" && !roleNames.includes(value);

  return (
    <Box sx={{ width: "100%" }}>
      <FormControl variant="standard" fullWidth>
        <TextField
          variant="standard"
          select
          label={
            <Stack direction="row" spacing={0.5} alignItems="center" component="span">
              <span>{label}</span>
              {helpAdornment}
            </Stack>
          }
          value={value}
          onChange={(e) => onSelectChange(e.target.value)}
          disabled={saving || isFetching}
        >
          <MenuItem value="">
            <em style={{ color: "rgba(255,255,255,0.6)" }}>
              {description.default_value
                ? `(inherit default — ${description.default_value})`
                : "(unset)"}
            </em>
          </MenuItem>
          {showOrphan && (
            <MenuItem value={value}>
              <em>{value} (not found)</em>
            </MenuItem>
          )}
          {roleNames.map((name) => (
            <MenuItem key={name} value={name}>
              {name}
            </MenuItem>
          ))}
        </TextField>
        <Typography variant="caption" color="text.secondary" sx={{ mt: 0.25 }}>
          {roleNames.length > 0
            ? `${roleNames.length} role${roleNames.length === 1 ? "" : "s"} available`
            : "No roles defined under /settings/WEB/server/roles"}
        </Typography>
      </FormControl>
    </Box>
  );
}

function FreeSoloSelectField({
  path,
  description,
  options,
}: Pick<Props, "path" | "description"> & { options: string[] }) {
  const initial = description.value ?? "";
  const [value, setValue] = useState(initial);
  const [saving, setSaving] = useState(false);
  const [updateSettings] = useUpdateSettingsMutation();

  useEffect(() => {
    setValue(initial);
  }, [initial]);

  const persist = async (next: string) => {
    setSaving(true);
    try {
      await updateSettings({ path, key: description.key, value: next }).unwrap();
    } finally {
      setSaving(false);
    }
  };

  const label = LABEL_OVERRIDES[description.key] ?? (description.title || description.key);
  const help = description.description;
  const helpAdornment = help ? (
    <Tooltip title={help} arrow>
      <HelpOutlineIcon fontSize="small" sx={{ color: "text.secondary" }} />
    </Tooltip>
  ) : null;

  return (
    <Box sx={{ width: "100%" }}>
      <Autocomplete<string, false, false, true>
        freeSolo
        size="small"
        options={options}
        value={value}
        onChange={(_e, next) => {
          const v = next ?? "";
          setValue(v);
          if (v !== initial) void persist(v);
        }}
        onInputChange={(_e, next, reason) => {
          if (reason !== "reset") setValue(next);
        }}
        onBlur={() => {
          if (value !== initial) void persist(value);
        }}
        disabled={saving}
        renderInput={(params) => (
          <TextField
            {...params}
            variant="standard"
            label={
              <Stack direction="row" spacing={0.5} alignItems="center" component="span">
                <span>{label}</span>
                {helpAdornment}
              </Stack>
            }
          />
        )}
      />
      {description.default_value && (
        <Typography variant="caption" color="text.secondary" sx={{ mt: 0.25 }}>
          default: {description.default_value}
        </Typography>
      )}
    </Box>
  );
}
