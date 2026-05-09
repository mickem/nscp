import {
  MetadataOption,
  SettingsDescription,
  useGetChannelMetadataQuery,
  useGetCounterMetadataQuery,
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
import { useEffect, useState } from "react";

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

type EnumOption = { value: string; label: string };

// Some schema entries share the same generic title (e.g. "SYNTAX" is used
// by `top syntax`, `ok syntax`, `detail syntax`), which makes the rendered
// labels look duplicated. Override per key when needed.
const LABEL_OVERRIDES: Record<string, string> = {
  "top syntax": "Top syntax",
  "ok syntax": "OK syntax",
  "detail syntax": "Detail syntax",
};

const COUNTER_TYPE_OPTIONS: EnumOption[] = ["double", "long", "large", "long long"].map(
  (v) => ({ value: v, label: v }),
);
const REALTIME_PHYS_OPTIONS: EnumOption[] = ["physical", "committed", "virtual"].map(
  (v) => ({ value: v, label: v }),
);
const COUNTER_FLAGS_OPTIONS = ["nocap100", "1000", "noscale"];
const COLLECTION_STRATEGY_OPTIONS: EnumOption[] = [
  { value: "static", label: "static" },
  { value: "rrd", label: "round robin" },
];
const SEVERITY_OPTIONS: EnumOption[] = ["OK", "WARNING", "CRITICAL", "UNKNOWN"].map((v) => ({
  value: v,
  label: v,
}));

function toEnumOptions(opts: string[] | undefined): EnumOption[] | undefined {
  return opts?.map((o) => ({ value: o, label: o }));
}

// Resolve the `type` field's allowed values based on which collection the
// instance lives under. Multiple collections expose a `type` field with
// completely different vocabularies (PDH counter type vs realtime resource
// type), so we key off the path.
function resolveTypeOptions(path: string): EnumOption[] | undefined {
  if (path.includes("/system/windows/counters/")) return COUNTER_TYPE_OPTIONS;
  if (/\/system\/windows\/real-time\/(cpu|memory)\//.test(path)) return REALTIME_PHYS_OPTIONS;
  return undefined;
}

// Best-effort enum extraction from the description text.
// Looks for the patterns `Values: a, b, c` or `(a, b, c)` (length 2-5 short tokens).
function inferEnum(text: string | undefined): string[] | undefined {
  if (!text) return undefined;
  const valuesMatch = text.match(/Values?:\s*([A-Za-z0-9_,\s]+)/i);
  if (valuesMatch) {
    const opts = valuesMatch[1]
      .split(/[,\s]+/)
      .map((s) => s.trim())
      .filter(Boolean);
    if (opts.length >= 2 && opts.length <= 8) return opts;
  }
  return undefined;
}

// Extract a list of options from a parenthetical, e.g. "Extra flags (nocap100, 1000, noscale)".
function inferOptionsFromParens(text: string | undefined): string[] | undefined {
  if (!text) return undefined;
  const m = text.match(/\(([A-Za-z0-9_,\s]+)\)/);
  if (!m) return undefined;
  const opts = m[1]
    .split(/[,\s]+/)
    .map((s) => s.trim())
    .filter(Boolean);
  if (opts.length >= 2 && opts.length <= 8) return opts;
  return undefined;
}

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
    setValue(next);
    setDirty(false);
    if (next !== initial) persist(next);
  };

  const label = LABEL_OVERRIDES[description.key] ?? (description.title || description.key);
  const help = description.description;
  const enumOpts: EnumOption[] | undefined =
    description.key === "type"
      ? (resolveTypeOptions(path) ?? toEnumOptions(inferEnum(help)))
      : description.key === "collection strategy"
        ? COLLECTION_STRATEGY_OPTIONS
        : description.key === "severity"
          ? SEVERITY_OPTIONS
          : description.key === "parent" && parentOptions
            ? parentOptions.map((n) => ({ value: n, label: n }))
            : toEnumOptions(inferEnum(help));

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
          {enumOpts && !forceDefault && (
            <MenuItem value="">
              <em style={{ color: "rgba(255,255,255,0.6)" }}>
                {description.default_value
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
