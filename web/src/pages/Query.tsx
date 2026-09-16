import Stack from "@mui/material/Stack";
import {
  nsclientApi,
  QueryExecutionResult,
  useExecuteQueryMutation,
  useGetQueryHelpQuery,
  useGetQueryQuery,
} from "../api/api.ts";
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  Box,
  Card,
  CardActions,
  CardContent, Chip,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  TextField
} from "@mui/material";
import { useNavigate, useParams } from "react-router";
import { Toolbar } from "../components/atoms/Toolbar.tsx";
import { Spacing } from "../components/atoms/Spacing.tsx";
import { RefreshButton } from "../components/atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import { useMemo, useRef, useState } from "react";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import Typography from "@mui/material/Typography";
import Button from "@mui/material/Button";
import { QueryResultChip } from "../components/atoms/QueryResultChip.tsx";
import Trail from "../components/atoms/Trail.tsx";
import SyntaxArgumentsField, { SyntaxArgumentsFieldHandle } from "../components/atoms/SyntaxArgumentsField.tsx";
import QueryHelpPanel from "../components/QueryHelpPanel.tsx";
import { makeVocabulary } from "../common/queryHelp.ts";
import { contextAt, CursorContext } from "../common/syntax.ts";

const CMD_REGEXP = /\\?.|^$/g;

export default function Query() {
  const { id } = useParams();
  const [busy, setBusy] = useState<boolean>(false);
  const dispatch = useAppDispatch();
  const navigate = useNavigate();
  const { data: query } = useGetQueryQuery(id || "");
  // The check's own options and filter keywords, so the argument line can be
  // highlighted and completed against what this check actually accepts rather
  // than against a generic guess at the grammar.
  const { data: help, isFetching: helpLoading } = useGetQueryHelpQuery(id || "", { skip: !id });
  const [executeQuery] = useExecuteQueryMutation();
  const [args, setArgs] = useState<string>("");
  const [context, setContext] = useState<CursorContext>(() => contextAt("", 0));
  const argumentsRef = useRef<SyntaxArgumentsFieldHandle>(null);
  const [result, setResult] = useState<QueryExecutionResult | undefined>(undefined);
  const vocabulary = useMemo(() => makeVocabulary(help), [help]);

  // `warning` / `critical` can now be a Nagios range string like "4:5"
  // (issue #748); `value` / `minimum` / `maximum` are always numeric.
  // The union widening here means non-numeric thresholds render verbatim
  // instead of becoming NaN through Math.round.
  const val = (value: number | string | undefined) => {
    if (value === undefined) {
      return "";
    }
    if (typeof value === "string") {
      return value;
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
        <Trail trail={[ { link: '/queries', title: 'Queries'}]} title={query?.name}/>
        <Spacing />
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      <Card>
        <CardContent>
          <Typography gutterBottom sx={{ color: "text.secondary", fontSize: 14 }}>
            {query?.name}
          </Typography>
          <Typography variant="body2">{query?.description}</Typography>
          <Typography variant="body2">Check provided by the <Chip label={query?.plugin} size="small" onClick={() => navigate("/modules/" + query?.plugin)}/> module.</Typography>
          <Stack direction="row" spacing={1} sx={{ width: 1, paddingTop: 3, alignItems: "flex-start" }}>
            <TextField label="Command" variant="outlined" size="small" value={query?.name || ""} disabled={true} />
            <SyntaxArgumentsField
              ref={argumentsRef}
              value={args}
              onChange={setArgs}
              vocabulary={vocabulary}
              onContextChange={setContext}
              onSubmit={doExecuteQuery}
              placeholder={"filter=free < 10% \"warning=free < 20%\""}
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
      <Card>
        <CardContent>
          <QueryHelpPanel
            queryName={query?.name || id || ""}
            vocabulary={vocabulary}
            context={context}
            loading={helpLoading}
            onInsert={(entry) => argumentsRef.current?.insert(entry)}
          />
        </CardContent>
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
