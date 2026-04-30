# Manual test cases for `check_files` bug fixes and `check_single_file`

These tests are designed to be run from a Windows shell (PowerShell or
`cmd.exe`) against a built NSClient++ on the same host. Each section gives:

* **Setup** — exact commands to create the test fixtures (run once per bug).
* **Before fix** — the result you should see when running against an
  *unpatched* NSClient++ (i.e. a build from before this PR).
* **After fix** — the result you should see when running against a
  *patched* NSClient++ (i.e. a build from this PR).
* **Cleanup** — commands to delete the test fixtures.

All commands assume:

* NSClient++ is installed at `C:\Program Files\NSClient++\` (adjust as needed).
* You run the checks via `nscp` against a locally running service:

  ```
  "C:\Program Files\NSClient++\nscp.exe" client --query <command> -- <args...>
  ```

  Or directly via the test runner without a service:

  ```
  "C:\Program Files\NSClient++\nscp.exe" test
  > <command> <args...>
  ```

* Test fixtures live under `C:\nscp_manual_tests\`. Pick a different
  directory if that path is not writable for you and adjust the commands.

> **Tip** — for a quick A/B comparison, keep two `nscp.exe` binaries side
> by side (e.g. `nscp_old.exe` and `nscp_new.exe`) and run each test with
> both. The "Before" result should match the buggy binary; the "After"
> result should match the patched binary.

---

## #730 — `max-depth=0` returned "no files found"

### Setup

```powershell
New-Item -ItemType Directory -Force -Path C:\nscp_manual_tests\depth0\sub | Out-Null
"hello" | Out-File -Encoding ASCII C:\nscp_manual_tests\depth0\top1.txt
"hello" | Out-File -Encoding ASCII C:\nscp_manual_tests\depth0\top2.txt
"hello" | Out-File -Encoding ASCII C:\nscp_manual_tests\depth0\sub\nested.txt
```

### Test

```
check_files path=C:\nscp_manual_tests\depth0 max-depth=0 "filter=type='file'" "warning=count > 5" "crit=count > 10"
```

### Expected

| Build  | Status    | Message contains                                        |
| ------ | --------- | ------------------------------------------------------- |
| Before | OK / WARN | `No files found` (zero files counted)                   |
| After  | OK        | 2 files matched (`top1.txt`, `top2.txt`); `nested.txt` is **not** counted |

Also re-run with `max-depth=1` on both builds — that result must be
unchanged (still 2 top-level files, no recursion). This proves the fix
is not a breaking change for `max-depth ≥ 1`.

### Cleanup

```powershell
Remove-Item -Recurse -Force C:\nscp_manual_tests\depth0
```

---

## #598 — paths with non-ACP characters silently failed

This bug only reproduces on hosts whose system codepage cannot encode the
characters in the path. The path used by the original report contains an
`é`, which is encodable on most Western Windows installs, so to reliably
reproduce the bug pick a character outside your active codepage. The
example below uses CJK characters which are not in code pages 1252 / 1250.

### Setup

```powershell
$dir = "C:\nscp_manual_tests\日本語"
New-Item -ItemType Directory -Force -Path $dir | Out-Null
"hello" | Out-File -Encoding UTF8 (Join-Path $dir "file.txt")
```

If you cannot create that directory because of how PowerShell renders the
literal, copy/paste the path from Explorer's address bar.

### Test

```
check_files "path=C:\nscp_manual_tests\日本語" "filter=type='file'" "warn=count > 5" "crit=count > 10"
```

### Expected

| Build  | Status  | Message contains                                                |
| ------ | ------- | --------------------------------------------------------------- |
| Before | UNKNOWN | `Invalid file specified` / `File was NOT found` for the path    |
| After  | OK      | 1 file matched (`file.txt`)                                     |

### Cleanup

```powershell
Remove-Item -Recurse -Force "C:\nscp_manual_tests\日本語"
```

---

## #613 — missing path returned OK / "No files found"

### Test (no setup needed — the path must **not** exist)

```
check_files path=C:\nscp_manual_tests\does_not_exist_47b1f0e5 "filter=type='file'" "warn=count > 5" "crit=count > 10"
```

### Expected

| Build  | Status  | Message contains                                                  |
| ------ | ------- | ----------------------------------------------------------------- |
| Before | OK      | `No files found` (or whatever `empty-state` is configured to be)  |
| After  | UNKNOWN | `Path was not found: C:\nscp_manual_tests\does_not_exist_47b1f0e5` |

### Variant: multiple paths, one missing

```
check_files path=C:\Windows\System32 path=C:\nscp_manual_tests\does_not_exist_47b1f0e5 "filter=type='file'" "warn=count > 0" "crit=count > 100000"
```

| Build  | Status  | Message contains                                                              |
| ------ | ------- | ----------------------------------------------------------------------------- |
| Before | WARN/CRIT | Counts files in `System32`; the missing path is silently ignored             |
| After  | UNKNOWN | `Path was not found: C:\nscp_manual_tests\does_not_exist_47b1f0e5`            |

---

## #605 — files counted twice via NTFS junctions

### Setup

You need an elevated shell to create a junction.

```powershell
New-Item -ItemType Directory -Force -Path C:\nscp_manual_tests\junction\real | Out-Null
1..3 | ForEach-Object {
    "content $_" | Out-File -Encoding ASCII "C:\nscp_manual_tests\junction\real\file$_.txt"
}
# Junction pointing back into the same tree
cmd /c mklink /J C:\nscp_manual_tests\junction\link C:\nscp_manual_tests\junction\real
```

After the setup you should have:

```
C:\nscp_manual_tests\junction\real\file1.txt
C:\nscp_manual_tests\junction\real\file2.txt
C:\nscp_manual_tests\junction\real\file3.txt
C:\nscp_manual_tests\junction\link\  (junction → real)
```

### Test

```
check_files path=C:\nscp_manual_tests\junction "filter=type='file'" "warn=count > 100" "crit=count > 200" "top-syntax=%(count) files"
```

### Expected

| Build  | Status | Message contains       |
| ------ | ------ | ---------------------- |
| Before | OK     | `6 files` (each real file counted twice — once via `real\`, once via `link\`) |
| After  | OK     | `3 files` (junction `link\` is skipped during recursion)                     |

### Cleanup

```powershell
cmd /c rmdir C:\nscp_manual_tests\junction\link
Remove-Item -Recurse -Force C:\nscp_manual_tests\junction
```

---

## #717 — legacy `CheckFiles` returned UNKNOWN for 0 matching files

### Setup

```powershell
New-Item -ItemType Directory -Force -Path C:\nscp_manual_tests\empty | Out-Null
```

(The directory exists but contains no files.)

### Test (note the **legacy** capital-camel-case command name)

```
CheckFiles path=C:\nscp_manual_tests\empty pattern=*.log MaxWarn=10 MaxCrit=20
```

### Expected

| Build  | Status  | Message contains                  |
| ------ | ------- | --------------------------------- |
| Before | UNKNOWN | `No files found` (modern default) |
| After  | OK      | `0 < 10` (legacy semantics)       |

### Cleanup

```powershell
Remove-Item -Recurse -Force C:\nscp_manual_tests\empty
```

---

## New: `check_single_file` smoke tests

These tests exercise the new command in isolation. There is no "before"
behaviour because the command is new — they are simply acceptance tests
to confirm it works.

### Setup

```powershell
New-Item -ItemType Directory -Force -Path C:\nscp_manual_tests\single | Out-Null
"a few bytes of text"             | Out-File -Encoding ASCII C:\nscp_manual_tests\single\small.txt
"x" * 200000                      | Out-File -Encoding ASCII C:\nscp_manual_tests\single\big.txt
```

### Test 1 — happy path, file under threshold

```
check_single_file file=C:\nscp_manual_tests\single\small.txt "warn=size > 1M" "crit=size > 10M"
```

Expected: **OK**, message contains the file name and a small size.

### Test 2 — file over warning threshold

```
check_single_file file=C:\nscp_manual_tests\single\big.txt "warn=size > 100k" "crit=size > 10M"
```

Expected: **WARNING**, message contains `big.txt` and `size > 100k`.

### Test 3 — file does not exist (regression for #613 in the new command)

```
check_single_file file=C:\nscp_manual_tests\single\missing.txt "warn=size > 1M"
```

Expected: **UNKNOWN**, message `File not found: C:\nscp_manual_tests\single\missing.txt`.

### Test 4 — no `file` argument

```
check_single_file "warn=size > 1M"
```

Expected: **UNKNOWN**, message `No file specified (use file=<path>)`.

### Test 5 — non-ACP path (regression for #598 in the new command)

```
"data" | Out-File -Encoding UTF8 "C:\nscp_manual_tests\single\日本語.txt"
check_single_file "file=C:\nscp_manual_tests\single\日本語.txt" "warn=size > 1M"
```

Expected: **OK** with the file's properties; the non-ACP filename must
not cause "File not found".

### Cleanup

```powershell
Remove-Item -Recurse -Force C:\nscp_manual_tests\single
```

---

## Bulk cleanup

When all tests are done:

```powershell
Remove-Item -Recurse -Force C:\nscp_manual_tests
```
