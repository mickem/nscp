import Stack from "@mui/material/Stack";
import {
  AliasListItem,
  nsclientApi,
  useGetAliasesQuery,
  useGetQueriesQuery,
} from "../api/api.ts";
import {
  Box,
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableRow,
  TableSortLabel,
  Tooltip,
  Typography,
} from "@mui/material";
import { useNavigate } from "react-router";
import { Toolbar } from "../components/atoms/Toolbar.tsx";
import { Spacing } from "../components/atoms/Spacing.tsx";
import { RefreshButton } from "../components/atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import Trail from "../components/atoms/Trail.tsx";
import FilterField from "../components/atoms/FilterField.tsx";
import { useMemo, useState } from "react";

type SortKey = "name" | "plugin" | "description";
type SortDir = "asc" | "desc";

// Generic row shape that covers both QueryListItem and AliasListItem.
interface Row {
  name: string;
  plugin: string;
  description: string;
}

export default function Queries() {
  const dispatch = useAppDispatch();
  const navigate = useNavigate();
  const { data: queries } = useGetQueriesQuery();
  const { data: aliases } = useGetAliasesQuery();
  const [filter, setFilter] = useState<string>("");

  const [querySort, setQuerySort] = useState<{ key: SortKey; dir: SortDir }>({
    key: "name",
    dir: "asc",
  });
  const [aliasSort, setAliasSort] = useState<{ key: SortKey; dir: SortDir }>({
    key: "name",
    dir: "asc",
  });

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Queries", "Aliases"]));
  };

  // Hide legacy `checkXXX` aliases - the canonical names use the underscored
  // `check_XXX` form. Anything that starts with `check` but isn't `check_...`
  // is treated as a legacy alias and excluded from the queries list.
  const isLegacyCheckAlias = (name: string) =>
    name.startsWith("check") && !name.startsWith("check_");

  const needle = filter.trim().toLowerCase();

  const sortRows = (rows: Row[], sort: { key: SortKey; dir: SortDir }) => {
    const dir = sort.dir === "asc" ? 1 : -1;
    return [...rows].sort((a, b) => {
      const av = (a[sort.key] ?? "").toLowerCase();
      const bv = (b[sort.key] ?? "").toLowerCase();
      if (av < bv) return -1 * dir;
      if (av > bv) return 1 * dir;
      return 0;
    });
  };

  const filteredQueries = useMemo<Row[]>(() => {
    if (!queries) return [];
    const visible = queries.filter((q) => !isLegacyCheckAlias(q.name));
    const matched = needle
      ? visible.filter((q) =>
          [q.name, q.plugin, q.description].some((f) =>
            (f ?? "").toLowerCase().includes(needle),
          ),
        )
      : visible;
    return sortRows(matched, querySort);
  }, [queries, needle, querySort]);

  const filteredAliases = useMemo<Row[]>(() => {
    if (!aliases) return [];
    const visible: AliasListItem[] = aliases.filter((a) => !isLegacyCheckAlias(a.name));
    const matched = needle
      ? visible.filter((a) =>
          [a.name, a.plugin, a.description].some((f) =>
            (f ?? "").toLowerCase().includes(needle),
          ),
        )
      : visible;
    return sortRows(matched, aliasSort);
  }, [aliases, needle, aliasSort]);

  const toggleSort = (
    current: { key: SortKey; dir: SortDir },
    set: (s: { key: SortKey; dir: SortDir }) => void,
    key: SortKey,
  ) => {
    if (current.key === key) {
      set({ key, dir: current.dir === "asc" ? "desc" : "asc" });
    } else {
      set({ key, dir: "asc" });
    }
  };

  const renderTable = (
    rows: Row[],
    sort: { key: SortKey; dir: SortDir },
    setSort: (s: { key: SortKey; dir: SortDir }) => void,
    emptyMessage: string,
  ) => (
    <Table size="small" sx={{ tableLayout: "fixed" }}>
      <TableHead>
        <TableRow>
          <TableCell sx={{ width: 280 }}>
            <TableSortLabel
              active={sort.key === "name"}
              direction={sort.key === "name" ? sort.dir : "asc"}
              onClick={() => toggleSort(sort, setSort, "name")}
            >
              Name
            </TableSortLabel>
          </TableCell>
          <TableCell sx={{ width: 200 }}>
            <TableSortLabel
              active={sort.key === "plugin"}
              direction={sort.key === "plugin" ? sort.dir : "asc"}
              onClick={() => toggleSort(sort, setSort, "plugin")}
            >
              Module
            </TableSortLabel>
          </TableCell>
          <TableCell>
            <TableSortLabel
              active={sort.key === "description"}
              direction={sort.key === "description" ? sort.dir : "asc"}
              onClick={() => toggleSort(sort, setSort, "description")}
            >
              Description
            </TableSortLabel>
          </TableCell>
        </TableRow>
      </TableHead>
      <TableBody>
        {rows.length === 0 && (
          <TableRow>
            <TableCell colSpan={3}>
              <Typography variant="body2" color="text.secondary">
                {emptyMessage}
              </Typography>
            </TableCell>
          </TableRow>
        )}
        {rows.map((row) => (
          <TableRow
            key={row.name}
            hover
            onClick={() => navigate(`/queries/${row.name}`)}
            sx={{ cursor: "pointer" }}
          >
            <TableCell sx={{ fontFamily: "monospace" }}>{row.name}</TableCell>
            <TableCell>
              <Typography variant="body2" color="text.secondary">
                {row.plugin}
              </Typography>
            </TableCell>
            <TableCell>
              <Tooltip title={row.description || ""} placement="top-start" arrow>
                <Typography
                  variant="body2"
                  color="text.secondary"
                  sx={{
                    overflow: "hidden",
                    textOverflow: "ellipsis",
                    whiteSpace: "nowrap",
                  }}
                >
                  {row.description}
                </Typography>
              </Tooltip>
            </TableCell>
          </TableRow>
        ))}
      </TableBody>
    </Table>
  );

  return (
    <Stack direction="column" spacing={3}>
      <Toolbar>
        <Trail title="Queries" />
        <Spacing />
        <FilterField value={filter} onChange={setFilter} placeholder="Filter checks..." />
        {needle && (
          <Typography variant="body2" color="text.secondary">
            {filteredQueries.length + filteredAliases.length}/
            {(queries?.length ?? 0) + (aliases?.length ?? 0)}
          </Typography>
        )}
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>

      <Box>
        <Typography variant="overline" color="text.secondary" sx={{ pl: 1 }}>
          Queries{queries ? ` (${filteredQueries.length})` : ""}
        </Typography>
        {renderTable(
          filteredQueries,
          querySort,
          setQuerySort,
          needle ? `No queries match "${filter}".` : "No queries available.",
        )}
      </Box>

      <Box>
        <Typography variant="overline" color="text.secondary" sx={{ pl: 1 }}>
          Aliases{aliases ? ` (${filteredAliases.length})` : ""}
        </Typography>
        {renderTable(
          filteredAliases,
          aliasSort,
          setAliasSort,
          needle
            ? `No aliases match "${filter}".`
            : aliases
              ? "No aliases configured."
              : "Loading aliases...",
        )}
      </Box>
    </Stack>
  );
}
