// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

import { forwardRef, useCallback, useEffect, useImperativeHandle, useRef, useState } from "react";
import { ClickAwayListener, MenuItem, MenuList, Paper, Popper, TextField, Typography } from "@mui/material";
import { Theme } from "@mui/material/styles";
import { contextAt, CursorContext, filterCandidates, highlight, Span, TokenKind } from "../../common/syntax.ts";
import { firstLine, HelpEntry, QueryVocabulary } from "../../common/queryHelp.ts";

// The colour of each token kind, mirroring the interactive prompt's palette in
// modules/CommandClient/console_editor.cpp: inside a filter expression each
// kind takes the dim shade of whatever it is the expression-level counterpart
// of, so that on a line where half the characters sit inside an expression the
// command's own options are still the brightest thing on it. The one
// exception, deliberately, is a keyword that does not exist: wrong is wrong at
// either level, and it gets the full red.
const colorFor = (theme: Theme, kind: TokenKind): string => {
  switch (kind) {
    case "option":
      return theme.palette.warning.light;
    case "quoted":
      return theme.palette.secondary.light;
    case "punctuation":
      return theme.palette.text.disabled;
    case "keyword":
      return theme.palette.success.main;
    case "unknownKeyword":
      return theme.palette.error.light;
    case "function":
      return theme.palette.info.main;
    case "expressionOp":
      return theme.palette.warning.dark;
    case "number":
      return theme.palette.info.light;
    default:
      return theme.palette.text.primary;
  }
};

// Both layers must lay text out identically to the pixel, or the caret drifts
// away from the characters it is between. Everything that can move a glyph is
// pinned here and used by both.
const layout = {
  margin: 0,
  border: 0,
  padding: 0,
  fontFamily: '"Roboto Mono", "SFMono-Regular", Menlo, Consolas, monospace',
  fontSize: "0.875rem",
  // MUI's own input line, `1.4375em` at the default 1rem, spelled in rem so it
  // does not shrink with the smaller monospace font - which keeps the box the
  // height of the ordinary field standing next to it.
  lineHeight: "1.4375rem",
  letterSpacing: "normal",
  whiteSpace: "pre-wrap" as const,
  wordBreak: "break-word" as const,
  overflowWrap: "anywhere" as const,
  tabSize: 4,
};

interface SyntaxInputProps extends React.TextareaHTMLAttributes<HTMLTextAreaElement> {
  spans: Span[];
  /** Drawn in the coloured layer while the line is empty (see below). */
  hint?: string;
  /** MUI hands its styled input an ownerState; it must not reach the DOM. */
  ownerState?: unknown;
  type?: string;
}

/**
 * The input element MUI's TextField renders in place of its own textarea: a
 * coloured, read-only copy of the text with a transparent textarea over it.
 * The textarea keeps the caret, the selection, the undo stack and every
 * keyboard behaviour a real input has - only its glyphs are invisible, and the
 * layer underneath draws them in colour instead.
 */
// MUI renders its input through a styled `input`, so the component standing in
// for it is handed things a textarea has no use for: the `ownerState` emotion
// passes down, and the `type` of the input that would otherwise have been
// rendered. Neither may reach the DOM.
function textareaPropsOf(props: SyntaxInputProps): React.TextareaHTMLAttributes<HTMLTextAreaElement> {
  const rest: Record<string, unknown> = { ...props };
  // `placeholder` goes too: the textarea's own text is transparent, and a
  // browser draws the placeholder in a colour of its choosing, so the hint is
  // drawn in the coloured layer instead where it is certain to be visible.
  for (const key of ["spans", "hint", "className", "style", "ownerState", "type", "placeholder"]) delete rest[key];
  return rest as React.TextareaHTMLAttributes<HTMLTextAreaElement>;
}

const SyntaxInput = forwardRef<HTMLTextAreaElement, SyntaxInputProps>(function SyntaxInput(props, ref) {
  const { spans, hint, className, style } = props;
  const textarea = textareaPropsOf(props);
  return (
    <div className={className} style={{ position: "relative", width: "100%", ...style }}>
      <pre aria-hidden style={{ ...layout, minHeight: layout.lineHeight }}>
        {spans.length === 0 && hint ? <span style={{ color: "var(--nscp-token-punctuation)" }}>{hint}</span> : null}
        {spans.map((span, i) => (
          <span key={i} style={{ color: `var(--nscp-token-${span.kind})` }}>
            {span.text}
          </span>
        ))}
        {/* A trailing newline keeps the last (possibly empty) line of the
            coloured copy as tall as the textarea's, so the box does not jump
            as the text wraps onto a new line. */}
        {"\n"}
      </pre>
      <textarea
        {...textarea}
        ref={ref}
        spellCheck={false}
        autoComplete="off"
        autoCorrect="off"
        autoCapitalize="off"
        style={{
          ...layout,
          position: "absolute",
          inset: 0,
          width: "100%",
          height: "100%",
          resize: "none",
          overflow: "hidden",
          background: "transparent",
          color: "transparent",
          caretColor: "var(--nscp-token-plain)",
          outline: "none",
        }}
      />
    </div>
  );
});

interface Props {
  label?: string;
  value: string;
  onChange: (value: string) => void;
  /** The check's own options and filter keywords; empty until they are fetched. */
  vocabulary: QueryVocabulary;
  /** Called whenever the caret moves, so the page can describe what it is on. */
  onContextChange?: (context: CursorContext) => void;
  /** Ctrl+Enter, or Enter with no completion open. */
  onSubmit?: () => void;
  placeholder?: string;
  disabled?: boolean;
}

/** What the help panel needs to be able to do to the line it describes. */
export interface SyntaxArgumentsFieldHandle {
  /** Put `entry` into the line, wherever the caret makes sense for it. */
  insert: (entry: HelpEntry) => void;
}

/**
 * The argument line of a check, highlighted and completed the way the
 * interactive prompt (`nscp test`) highlights and completes it: the check's
 * own options in one colour, the filter keywords it offers in another, and a
 * name it does not offer in red - before the check is ever run.
 *
 * Completion offers the check's parameters where an option name goes and its
 * filter keywords where a keyword goes. Ctrl+Space asks for it explicitly.
 */
const SyntaxArgumentsField = forwardRef<SyntaxArgumentsFieldHandle, Props>(function SyntaxArgumentsField(
  { label = "Arguments", value, onChange, vocabulary, onContextChange, onSubmit, placeholder, disabled },
  handleRef,
) {
  const inputRef = useRef<HTMLTextAreaElement | null>(null);
  const anchorRef = useRef<HTMLDivElement | null>(null);
  const [caret, setCaret] = useState(0);
  const [open, setOpen] = useState(false);
  const [selected, setSelected] = useState(0);

  const context = contextAt(value, caret);
  const candidates: HelpEntry[] =
    context.target === "option" ? vocabulary.options : context.target === "keyword" ? vocabulary.keywords : [];
  const matches = filterCandidates(candidates, context.word, (c) => c.name);

  useEffect(() => {
    if (selected >= matches.length) setSelected(0);
  }, [matches.length, selected]);

  // The page is told where the caret is from the handlers that move it rather
  // than from an effect: the context is a pure function of the line and the
  // caret, so there is nothing to synchronise after the fact, and reporting it
  // from a render would cost the page a second render per keystroke.
  const moveCaret = useCallback(
    (line: string, at: number) => {
      setCaret(at);
      onContextChange?.(contextAt(line, at));
    },
    [onContextChange],
  );

  const syncCaret = useCallback(() => {
    const el = inputRef.current;
    if (el) moveCaret(el.value, el.selectionStart ?? 0);
  }, [moveCaret]);

  // Replaces [from, to) with `insert` and leaves the caret after it.
  const replace = (from: number, to: number, insert: string) => {
    const next = value.slice(0, from) + insert + value.slice(to);
    const at = from + insert.length;
    onChange(next);
    setOpen(false);
    // The value is not in the DOM until React has re-rendered it, so place the
    // caret afterwards - otherwise the browser puts it at the end of the line.
    requestAnimationFrame(() => {
      const el = inputRef.current;
      if (!el) return;
      el.focus();
      el.setSelectionRange(at, at);
      moveCaret(next, at);
    });
  };

  const apply = (entry: HelpEntry) => {
    // `filter=` completes to the whole `name=` only where there is no `=`
    // already; on `fil|=free` the user has typed the separator themselves and
    // wants the name corrected, not doubled.
    const followedByEquals = value[context.wordEnd] === "=";
    replace(context.wordStart, context.wordEnd, followedByEquals ? entry.name : entry.insert);
  };

  useImperativeHandle(handleRef, () => ({
    insert: (entry: HelpEntry) => {
      const fits = entry.kind === "option" ? context.target === "option" : context.target === "keyword";
      if (fits) {
        apply(entry);
      } else if (entry.kind === "option") {
        // Picked from the panel while the caret sits in somebody else's value:
        // an option only means anything as a new argument, so start one.
        const separator = value === "" || value.endsWith(" ") ? "" : " ";
        replace(value.length, value.length, separator + entry.insert);
      } else {
        // A keyword, on the other hand, is only ever part of a value - put it
        // where the caret is and let the user place it.
        replace(caret, caret, entry.insert);
      }
    },
  }));

  const onKeyDown = (event: React.KeyboardEvent<HTMLDivElement>) => {
    if (event.key === " " && event.ctrlKey) {
      event.preventDefault();
      setOpen(matches.length > 0);
      return;
    }
    if (open && matches.length > 0) {
      if (event.key === "ArrowDown") {
        event.preventDefault();
        setSelected((s) => (s + 1) % matches.length);
        return;
      }
      if (event.key === "ArrowUp") {
        event.preventDefault();
        setSelected((s) => (s + matches.length - 1) % matches.length);
        return;
      }
      if (event.key === "Enter" || event.key === "Tab") {
        event.preventDefault();
        // `selected` is clamped by an effect, which has not necessarily run
        // yet on the render where the list just got shorter.
        apply(matches[selected] ?? matches[0]);
        return;
      }
      if (event.key === "Escape") {
        event.preventDefault();
        setOpen(false);
        return;
      }
    }
    if (event.key === "Enter") {
      // A check's arguments are one line; Enter runs it, as it does at the
      // prompt, rather than quietly adding a line break the transport would
      // have to carry.
      event.preventDefault();
      onSubmit?.();
    }
  };

  const handleChange = (event: React.ChangeEvent<HTMLInputElement | HTMLTextAreaElement>) => {
    onChange(event.target.value);
    const at = event.target.selectionStart ?? event.target.value.length;
    moveCaret(event.target.value, at);
    // Offer completions as soon as there is something to match on, and get out
    // of the way the moment there is not.
    setOpen(at > 0 && /[A-Za-z0-9_-]/.test(event.target.value[at - 1] ?? ""));
  };

  const spans = highlight(value, vocabulary.vocabulary);

  return (
    <div ref={anchorRef} style={{ width: "100%" }}>
      <TextField
        label={label}
        variant="outlined"
        size="small"
        fullWidth
        multiline
        disabled={disabled}
        placeholder={placeholder}
        value={value}
        onChange={handleChange}
        onKeyDown={onKeyDown}
        onBlur={() => setOpen(false)}
        inputRef={inputRef}
        sx={(theme) => ({
          // The two layers read their colours from here, so a span does not
          // need the theme and the palette stays in one place.
          "--nscp-token-plain": colorFor(theme, "plain"),
          "--nscp-token-option": colorFor(theme, "option"),
          "--nscp-token-value": colorFor(theme, "value"),
          "--nscp-token-quoted": colorFor(theme, "quoted"),
          "--nscp-token-punctuation": colorFor(theme, "punctuation"),
          "--nscp-token-keyword": colorFor(theme, "keyword"),
          "--nscp-token-unknownKeyword": colorFor(theme, "unknownKeyword"),
          "--nscp-token-function": colorFor(theme, "function"),
          "--nscp-token-expressionOp": colorFor(theme, "expressionOp"),
          "--nscp-token-number": colorFor(theme, "number"),
        })}
        slotProps={{
          // The hint below occupies the line an unshrunk label would sit on, so
          // the label stays up whether or not the field has focus - and the
          // outline has to keep the gap for it, which it otherwise only opens
          // for a field that is focused or has a value.
          inputLabel: { shrink: true },
          input: {
            notched: true,
            inputComponent: SyntaxInput as never,
            // The caret handlers belong on the textarea, not on the field:
            // TextField forwards an unrecognised prop to the wrapper div,
            // where a selection event never fires and the page would never
            // learn that the caret had moved.
            inputProps: { spans, hint: placeholder, "aria-label": label, onSelect: syncCaret, onClick: syncCaret, onKeyUp: syncCaret },
          },
        }}
      />
      <Popper
        open={open && matches.length > 0}
        anchorEl={anchorRef.current}
        placement="bottom-start"
        style={{ zIndex: 1300, width: anchorRef.current?.clientWidth }}
      >
        <ClickAwayListener onClickAway={() => setOpen(false)}>
          <Paper elevation={6} sx={{ maxHeight: 280, overflow: "auto" }}>
            <MenuList dense>
              {matches.slice(0, 50).map((entry, i) => (
                <MenuItem
                  key={`${entry.kind}:${entry.name}`}
                  selected={i === selected}
                  // The field must keep the focus: a blur would close the list
                  // before the click ever lands on it.
                  onMouseDown={(event) => event.preventDefault()}
                  onClick={() => apply(entry)}
                >
                  <Typography component="span" sx={{ fontFamily: "monospace", mr: 2 }}>
                    {entry.name}
                  </Typography>
                  <Typography component="span" variant="caption" color="text.secondary" noWrap>
                    {firstLine(entry.description)}
                  </Typography>
                </MenuItem>
              ))}
            </MenuList>
          </Paper>
        </ClickAwayListener>
      </Popper>
    </div>
  );
});

export default SyntaxArgumentsField;
