import Stack from "@mui/material/Stack";
import { nsclientApi, QueryExecutionResult, useExecuteQueryMutation, useGetQueryQuery } from "../api/api.ts";
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  Box,
  Card,
  CardActions,
  CardContent,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  TextField,
} from "@mui/material";
import { useParams } from "react-router";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import { useState } from "react";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import Typography from "@mui/material/Typography";
import Button from "@mui/material/Button";
import { QueryResultChip } from "./atoms/QueryResultChip.tsx";

const CMD_REGEXP = /\\?.|^$/g;

export default function Query() {
  const { id } = useParams();
  const [busy, setBusy] = useState<boolean>(false);
  const dispatch = useAppDispatch();
  const { data: query } = useGetQueryQuery(id || "");
  const [executeQuery] = useExecuteQueryMutation();
  const [args, setArgs] = useState<string>("");
  const [result, setResult] = useState<QueryExecutionResult | undefined>(undefined);

  const val = (value: number) => {
    if (value === undefined) {
      return "";
    }

    return Math.round(value * 100) / 100;
  };

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Logs"]));
  };

  const doExecuteQuery = async () => {
    setBusy(true);
    const parsed_args =
      args.match(CMD_REGEXP)?.reduce(
        (p, c) => {
          if (c === '"') {
            p.quote ^= 1;
          } else if (!p.quote && c === " ") {
            p.a.push("");
          } else {
            p.a[p.a.length - 1] += c.replace(/\\(.)/, "$1");
          }
          return p;
        },
        { a: [""], quote: 0 },
      ).a || [];
    setResult(await executeQuery({ query: id || "", args: parsed_args }).unwrap());
    setBusy(false);
  };
  const doExecuteHelp = async () => {
    setResult(await executeQuery({ query: id || "", args: ["help"] }).unwrap());
  };
  const doClear = () => {
    setResult(undefined);
  };
  const truncate = (text: string, length = 120) => {
    if (text.length <= length) {
      return text;
    }
    return text.substring(0, length) + "...";
  };

  return (
    <Stack direction="column" spacing={3}>
      <Toolbar>
        <Spacing />
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      <Card>
        <CardContent>
          <Typography gutterBottom sx={{ color: "text.secondary", fontSize: 14 }}>
            {query?.name}
          </Typography>
          <Typography variant="body2">{query?.description}</Typography>
          <Stack direction="row" spacing={1} width={1} sx={{ paddingTop: 3 }}>
            <TextField label="Command" variant="outlined" size="small" value={query?.name || ""} disabled={true} />
            <TextField
              label="Arguments"
              variant="outlined"
              size="small"
              value={args}
              fullWidth
              onChange={(e) => setArgs(e.target.value)}
            />
          </Stack>
        </CardContent>
        <CardActions sx={{ justifyContent: "flex-end" }}>
          <Button onClick={doExecuteQuery} color="success" loading={busy}>
            Execute
          </Button>
          <Button onClick={doExecuteHelp}>Get Help</Button>
          <Button onClick={doClear} color="error" disabled={result === undefined}>
            Clear Result
          </Button>
        </CardActions>
      </Card>
      {result && (
        <Stack>
          {result.lines.map((line, id) => (
            <Accordion key={id}>
              <AccordionSummary expandIcon={<ExpandMoreIcon />}>
                <QueryResultChip result={result.result} />
                <Typography sx={{ paddingLeft: 1 }}>{truncate(line.message)}</Typography>
              </AccordionSummary>
              <AccordionDetails>
                <Box sx={{ overflow: "auto", backgroundColor: "black", color: "white" }}>
                  <Typography component="pre">{line.message}</Typography>
                </Box>
                <TableContainer>
                  <Table>
                    <TableHead>
                      <TableRow>
                        <TableCell></TableCell>
                        <TableCell align="right">Value</TableCell>
                        <TableCell align="right">Warning</TableCell>
                        <TableCell align="right">Critical</TableCell>
                        <TableCell align="right">Min</TableCell>
                        <TableCell align="right">Max</TableCell>
                      </TableRow>
                    </TableHead>
                    <TableBody>
                      {Object.keys(line.perf).map((k) => (
                        <TableRow
                          key={k}
                          sx={{
                            "&:last-child td, &:last-child th": { border: 0 },
                          }}
                        >
                          <TableCell component="th" scope="row">
                            {k}
                          </TableCell>
                          <TableCell align="right">
                            {val(line.perf[k].value)} {line.perf[k].unit}
                          </TableCell>
                          <TableCell align="right">{val(line.perf[k].warning)}</TableCell>
                          <TableCell align="right">{val(line.perf[k].critical)}</TableCell>
                          <TableCell align="right">{val(line.perf[k].minimum)}</TableCell>
                          <TableCell align="right">{val(line.perf[k].maximum)}</TableCell>
                        </TableRow>
                      ))}
                    </TableBody>
                  </Table>
                </TableContainer>
              </AccordionDetails>
            </Accordion>
          ))}
        </Stack>
      )}
    </Stack>
  );
}
