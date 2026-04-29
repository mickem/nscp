import { IconButton, InputAdornment, TextField } from "@mui/material";
import SearchIcon from "@mui/icons-material/Search";
import ClearIcon from "@mui/icons-material/Clear";

interface Props {
  value: string;
  onChange: (value: string) => void;
  placeholder?: string;
  minWidth?: number | string;
}

/**
 * A small reusable filter/search input used by list pages
 * (Modules, Queries, Settings, ...).
 */
export default function FilterField({ value, onChange, placeholder = "Filter…", minWidth = 260 }: Props) {
  return (
    <TextField
      size="small"
      placeholder={placeholder}
      value={value}
      onChange={(e) => onChange(e.target.value)}
      sx={{ minWidth }}
      slotProps={{
        input: {
          startAdornment: (
            <InputAdornment position="start">
              <SearchIcon fontSize="small" />
            </InputAdornment>
          ),
          endAdornment: value ? (
            <InputAdornment position="end">
              <IconButton size="small" onClick={() => onChange("")} aria-label="clear filter">
                <ClearIcon fontSize="small" />
              </IconButton>
            </InputAdornment>
          ) : null,
        },
      }}
    />
  );
}

