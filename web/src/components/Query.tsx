import Stack from "@mui/material/Stack";
import { nsclientApi, QueryExecutionResult, useExecuteQueryMutation, useGetQueryQuery } from "../api/api.ts";
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
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

export default function Query() {
  const { id } = useParams();
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
    setResult(await executeQuery({ query: id || "", args: args.split(" ") }).unwrap());
  };
  const doExecuteHelp = async () => {
    setResult(await executeQuery({ query: id || "", args: ["help"] }).unwrap());
  };
  const doClear = () => {
    setResult(undefined);
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
          <Typography variant="body2" component="div">
            {query?.description}
          </Typography>
        </CardContent>
        <CardActions>
          <Stack direction="row" spacing={1}>
            <TextField label="Command" variant="outlined" size="small" value={query?.name || ""} disabled={true} />
            <TextField
              label="Arguments"
              variant="outlined"
              size="small"
              value={args}
              fullWidth
              onChange={(e) => setArgs(e.target.value)}
            />
            <Button onClick={doExecuteQuery}>Execute</Button>
            <Button onClick={doExecuteHelp}>Get&nbsp;Help</Button>
          </Stack>
        </CardActions>
      </Card>
      {result && (
        <Stack>
          {result.lines.map((line) => (
            <Accordion key={line.message}>
              <AccordionSummary expandIcon={<ExpandMoreIcon />}>
                <Typography component="pre">{line.message}</Typography>
              </AccordionSummary>
              <AccordionDetails>
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
          <Button onClick={doClear}>Clear</Button>
        </Stack>
      )}
    </Stack>
  );
}
