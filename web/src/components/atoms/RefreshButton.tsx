import { CircularProgress, IconButton } from "@mui/material";
import RefreshIcon from "@mui/icons-material/Refresh";

interface Props {
  onRefresh: () => void;
  isFetching?: boolean;
}

export function RefreshButton({ onRefresh, isFetching = false }: Props) {
  return (
    <>
      {!isFetching && (
        <IconButton onClick={onRefresh}>
          <RefreshIcon />
        </IconButton>
      )}
      {isFetching && <CircularProgress size="2em" />}
    </>
  );
}
