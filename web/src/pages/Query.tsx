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
  FormControlLabel,
  Switch,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  TextField,
  Tooltip
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
import ExperimentalChip from "../components/atoms/ExperimentalChip.tsx";
import SyntaxArgumentsField, { SyntaxArgumentsFieldHandle } from "../components/atoms/SyntaxArgumentsField.tsx";
import QueryHelpPanel from "../components/QueryHelpPanel.tsx";
import { makeVocabulary } from "../common/queryHelp.ts";
import { contextAt, CursorContext } from "../common/syntax.ts";
import {
  addSamples,
  readSimpleMode,
  removeSamples,
  sampleThresholds,
  splitArguments,
  writeSimpleMode,
} from "../common/sampleThresholds.ts";

/** What simple mode put on the argument line, and for which check. */
interface AppliedSample {
  id: string;
  /** The arguments as written, so switching the mode off can take them out. */
  added: string[];
  /** "none" when the check has nothing to sample. */
  source: "defaults" | "sample" | "none";
}

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
  // Simple mode writes a sample `warning=` and `critical=` onto the argument
  // line: the check's own defaults, spelled out where they can be edited. It
  // is remembered, so with it on every check opens with its sample in place.
  const [simple, setSimple] = useState<boolean>(readSimpleMode);
  const [applied, setApplied] = useState<AppliedSample | undefined>(undefined);

  // The sample depends on the help, which arrives after the page does - and,
  // going from one check to the next, after the id has changed. Written while
  // rendering (React re-renders at once) rather than in an effect, so the line
  // never shows a frame of one check with the thresholds of another.
  const helpReady = !!id && !!help && !helpLoading && help.name.toLowerCase() === id.toLowerCase();
  if (simple && helpReady && applied?.id !== id) {
    const sample = sampleThresholds(id, vocabulary);
    if (sample) {
      const next = addSamples(args, sample);
      setArgs(next.line);
      setApplied({ id, added: next.added, source: sample.source });
    } else {
      setApplied({ id, added: [], source: "none" });
    }
  }

  // One line under the arguments saying what the switch did - or why it did
  // nothing, which would otherwise look like a switch that does not work.
  let simpleNote: string | undefined;
  if (simple && applied && applied.id === id) {
    if (applied.source === "none") {
      simpleNote = "This check has no default thresholds, so there is no sample to add. Its keywords are listed below.";
    } else if (applied.added.length === 0) {
      simpleNote = "The arguments already set warning and critical, so nothing was added.";
    } else if (applied.source === "defaults") {
      simpleNote = "Sample thresholds added: the values this check uses by default. Change the numbers and press Execute.";
    } else {
      simpleNote = "Sample thresholds added. Change the numbers to suit this machine and press Execute.";
    }
  }

  const onSimpleChange = (on: boolean) => {
    writeSimpleMode(on);
    setSimple(on);
    if (!on && applied) {
      setArgs(removeSamples(args, applied.added));
      setApplied(undefined);
    }
  };

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
    const parsed_args = splitArguments(args);
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
          <Stack direction="row" spacing={1} sx={{ alignItems: "center", paddingBottom: 1 }}>
            <Typography sx={{ color: "text.secondary", fontSize: 14 }}>{query?.name}</Typography>
            {query?.experimental && <ExperimentalChip />}
          </Stack>
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
          {simpleNote && (
            <Typography variant="caption" sx={{ display: "block", color: "text.secondary", paddingTop: 1 }}>
              {simpleNote}
            </Typography>
          )}
        </CardContent>
        <CardActions sx={{ justifyContent: "flex-end" }}>
          <Tooltip title="Start from a sample warning and critical threshold instead of an empty line">
            <FormControlLabel
              sx={{ marginRight: "auto", paddingLeft: 1 }}
              control={<Switch size="small" checked={simple} onChange={(e) => onSimpleChange(e.target.checked)} />}
              label="Simple mode"
            />
          </Tooltip>
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
