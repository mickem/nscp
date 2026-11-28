// Runs a check now and shows what it answered, next to the button.
import { Alert, Box, Button, Popover } from "@mui/material";
import { useState } from "react";
import { useExecuteQueryMutation } from "../../api/api.ts";

interface Props {
  query: string;
  args: string[];
  label?: string;
  disabled?: boolean;
  // Said with the result, e.g. where the query came from.
  note?: string;
  // Open the answer above the button (it sits at the bottom of the window).
  above?: boolean;
}

const STATUS = ["OK", "WARNING", "CRITICAL", "UNKNOWN"];

export default function RunCheckButton({
  query,
  args,
  label = "Run",
  disabled,
  note,
  above,
}: Props) {
  const [executeQuery, { isLoading }] = useExecuteQueryMutation();
  const [result, setResult] = useState<{ ok: boolean; text: string } | null>(null);
  const [anchor, setAnchor] = useState<HTMLButtonElement | null>(null);

  const run = async () => {
    setResult(null);
    try {
      const r = await executeQuery({ query, args }).unwrap();
      const text = r.lines.map((l) => l.message).join(" ") || "(no output)";
      setResult({ ok: r.result === 0, text: `${STATUS[r.result] ?? r.result}: ${text}` });
    } catch (e) {
      setResult({ ok: false, text: e instanceof Error ? e.message : JSON.stringify(e) });
    }
  };

  return (
    <>
      <Button
        ref={setAnchor}
        size="small"
        onClick={() => void run()}
        disabled={disabled}
        loading={isLoading}
      >
        {label}
      </Button>
      <Popover
        open={result !== null}
        anchorEl={anchor}
        onClose={() => setResult(null)}
        anchorOrigin={{ vertical: above ? "top" : "bottom", horizontal: "right" }}
        transformOrigin={{ vertical: above ? "bottom" : "top", horizontal: "right" }}
      >
        {result && (
          <Box sx={{ maxWidth: 560 }}>
            <Alert severity={result.ok ? "success" : "warning"} onClose={() => setResult(null)}>
              {result.text}
              {note && (
                <Box component="div" sx={{ mt: 0.5, opacity: 0.8, fontSize: 12 }}>
                  {note}
                </Box>
              )}
            </Alert>
          </Box>
        )}
      </Popover>
    </>
  );
}
