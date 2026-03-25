# PROMPT: Classify and Verify All 64-bit Migration Changes

## Context

This branch (`ml/64`) implements a 64-bit version of the vere runtime. It introduces a compile-time flag `VERE64` that controls whether the runtime uses 64-bit or 32-bit word sizes. Key architectural changes:

- In `pkg/c3/types.h`: `c3_w` (the "word" type) is conditionally `uint64_t` (VERE64) or `uint32_t` (32-bit). A new type `c3_h` (`uint32_t` always) was introduced for values that must remain 32-bit in both modes. `c3_l` ("little") is similarly conditional.
- In `pkg/noun/allocate.h`: Preprocessor macros define word sizes, bit masks, alignment constants, and noun reference encoding constants conditionally based on `VERE64`.
- Print format specifiers (`PRIc3_w`, `PRIc3_h`, etc.) are conditionally defined.
- Various new helper functions and macros were added throughout to support the dual-width architecture.

The base commit (last upstream merge) is: `ee8ed877dd460d5721d71782f060ec251b3d24c1`

There are **279 changed files** with **~9,251 additions** and **~4,665 deletions** (~13,916 total changed lines).

## Task

### Phase 1: Generate the Full Diff and Classify Every Change

1. **Generate the diff** between the base commit and HEAD:
   ```
   git diff ee8ed877dd460d5721d71782f060ec251b3d24c1..HEAD
   ```
   This is a large diff (~14K lines). Process it file-by-file.

2. **For every single hunk** in the diff, classify the change. A single git-diff hunk may contain multiple distinct changes — identify and classify each one separately.

3. **Come up with your own classification taxonomy.** You must derive the categories from what you actually see in the diff. Likely categories will include things like (but are not limited to — discover these yourself):
   - Type width changes (e.g., `c3_w` → `c3_h` where a value must stay 32-bit)
   - Type width changes (e.g., `c3_d` → `c3_w` where a value should be word-sized)
   - Format specifier changes (e.g., `PRIu32` → `PRIc3_w`, or `"%u"` → `"%" PRIc3_w`)
   - Literal suffix changes (e.g., `0x80000000` → `0x80000000ULL` or to a named constant)
   - Sizeof/alignment changes
   - New `#ifdef VERE64` / `#ifndef VERE64` conditional blocks
   - New helper functions or macros
   - Cast changes or additions
   - Bit manipulation changes (e.g., shifting by different amounts in 64-bit mode)
   - Build system changes (build.zig, CI workflows)
   - Test changes / new tests
   - Migration / compatibility code
   - Comments and documentation changes
   - Bug fixes unrelated to 64-bit (that happened to land on this branch)
   - ...and whatever else you find

4. **Write the full classification to a file called `CLASSIFICATIONS_64.md`** with the following structure:

   ```markdown
   # 64-bit Migration: Change Classifications

   ## Classification Taxonomy
   [List and describe each category you identified]

   ## Classified Changes

   ### [filename]
   | Line(s) | Category | Description |
   |---------|----------|-------------|
   | L42-L45 | type-narrowing-w-to-h | Changed `c3_w` to `c3_h` for event number |
   | L102    | format-specifier | Changed `PRIu32` to `PRIc3_w` |
   ...
   ```

   Be exhaustive. Every changed line must appear in this file.

### Phase 2: Build Regex/Search Patterns for Each Classification

For each classification category, construct one or more regex patterns (or `grep`/`rg` search queries) that would identify all instances of that type of change in the diff. The goal is to **mechanically reproduce** the list of changes for each category.

Strategy:
- For type changes like `c3_w` → `c3_h`: search the diff for lines that remove `c3_w` and add `c3_h` in the same hunk context
- For format specifier changes: search for added/removed lines containing format macros
- For new conditional blocks: search for added `#ifdef VERE64` or `#ifndef VERE64`
- Consider searching both the diff output AND the current source files as appropriate
- You may use `git diff ee8ed877dd..HEAD` piped through grep, or search the current codebase, or both

Write each pattern and its purpose to a section in `CLASSIFICATIONS_64.md` under each category.

### Phase 3: Run the Patterns and Verify

1. **Run each regex/search pattern** against the diff (or codebase as appropriate).

2. **Compare the results** against the manually classified list from Phase 1.

3. **Iterate on the regex** until the pattern-matched list is an **exact match** for the manually classified list for that category. This may take multiple rounds of refinement.

4. **If the regex catches additional locations** that were NOT in the diff (i.e., places in the codebase that look like they should have been changed but weren't), **document these separately** in a file called `MISSED_CHANGES_64.md`** with the following structure:

   ```markdown
   # 64-bit Migration: Potentially Missed Changes

   ## Summary
   [Overview of what was found]

   ## Potentially Missed Changes

   ### Category: [classification name]
   | File | Line(s) | Current Code | Why It Might Need Changing |
   |------|---------|--------------|---------------------------|
   | pkg/foo/bar.c | L55 | `c3_w event_num` | Similar to other event number fields that were changed to `c3_h` |
   ...
   ```

   This file is critically important — it identifies gaps in the migration.

### Phase 4: Summary

Add a summary section to `CLASSIFICATIONS_64.md` with:
- Total number of changes per category
- Total number of potentially missed changes per category
- Confidence assessment for each category's regex coverage
- Any patterns or observations about the migration strategy

## Important Notes

- **Be exhaustive.** Every single diff hunk must be accounted for.
- **Be precise with line numbers.** Reference the current file line numbers (HEAD), not the base commit line numbers.
- **Separate concerns within hunks.** A single hunk that changes both a type and a format specifier should have two entries.
- **The "missed changes" file is arguably the most important output.** The whole point of this exercise is to find things that should have been changed but weren't.
- **Do not modify any source code.** This is a read-only analysis task.
- **Use subagents liberally.** The diff is large. Process files in parallel where possible.
- **When searching current source files for missed changes**, focus on patterns like:
  - Remaining `c3_w` usage that matches patterns of things that were changed to `c3_h`
  - Remaining hardcoded 32-bit constants where similar constants were changed
  - Remaining raw format specifiers where similar ones were changed to typed macros
  - Missing `#ifdef VERE64` guards where similar code was guarded

---

## What Was Produced (Results)

This prompt was executed against the `ml/64` branch. Three output files were produced:

### `CLASSIFICATIONS_64.md`
Full taxonomy of ~40 category codes and exhaustive per-file classification of all changes. Key categories:
- **SENTINEL** (171 single-line + 45 multi-line = 216 replacements of `, 0)` → `, u3_nul)` + 48 new uses)
- **TYPE-W-TO-H** (~1,072 removed `c3_w` lines, ~989 `c3_h` additions — remainder became `c3_m`/`c3_l`/`c3_d`)
- **FORMAT-SPEC** (78 old specifiers replaced, 192 new `PRIc3_*` in new code)
- **CAST-FIX** (103 `1U <<` → 92 `((c3_w)1)` + 2 `(c3_d)1` + 1 `1ULL` + 8 deleted)
- **MACRO-PARAMETERIZE** (61 `>> 5` + 50 `& 31` + 17 `>= 32` + 9 `met(5)` + 6 `slab(5)` replaced)
- **SIZEOF-FIX** (27 `* 4` + 43 `<< 2` replaced)
- **MIGRATION-TYPE-PIN** (105 `c3_vN_w` + 48 `u3_vN_noun` — version-pinned types in past/ code)
- **IFDEF-VERE64** (58 `#ifdef` + 47 `#ifndef` across 20 files)

### `REGEX_COUNTS_64.md`
Precise diff-line counts for every category with exact regex commands. Every gap between "removed" and "added" counts is fully reconciled:
- Gaps explained by new code (tests, definitions, restructured functions)
- Gaps explained by alternative replacements (`* 4` → `u3a_word_bytes` instead of `sizeof(c3_w)`)
- Gaps explained by deleted code (scow.c simplification, dead code removal)

### `MISSED_CHANGES_64.md`
Codebase-wide search for remaining instances of old patterns. **Revised findings:**

| Category | Missed | Severity |
|----------|--------|----------|
| **SENTINEL** (`0` → `u3_nul`) | **30** (3 untouched files + 1 partial) | **High** |
| **`* 4`** → `sizeof(c3_w)` | **16** (2 hmac.c false positives removed — `c3_h` is always 4 bytes) | **High** |
| **`1U <<`** → `((c3_w)1) <<` | **~9** | **Medium-High** |
| **`0x80000000`** sentinel in palloc.c | **6** | **Medium** |
| **`pip_w`** in xmas_tests.c | **3** | **Low** |

Fully migrated (0 remaining): `wag_w`, `>= 32` in jets, `c3_sift/etch_word`, `~0U`

### Key Lessons Learned

1. **Regex against the diff vs codebase must both be run.** The diff regex captures what *was* changed; the codebase regex finds what *wasn't*. Both are needed for completeness.

2. **Removed ≠ Added is normal and must be reconciled.** When a pattern like `* 4` is removed 27 times but `sizeof(c3_w)` appears 76 times, the gap must be explained line-by-line: direct replacements, new code using the correct pattern, alternative replacements (`u3a_word_bytes`), etc.

3. **False positives in miss detection require type-level analysis.** The hmac.c `padwords * 4` looked like a miss (same `* 4` pattern) but `padwords` counts `c3_h` entries which are always 4 bytes — not a miss. Similarly, `>> 5` in ames.c indexes into `c3_h` bitfields and is correct. Each "miss" must be verified against the actual data type.

4. **Multi-line calls inflate sentinel counts.** The `, 0)` sentinel in `u3r_mean` calls often appears on a continuation line without `u3r_mean` itself. A naive single-line grep undercounts by ~25%.

5. **Untouched files are the biggest risk.** `bytestream.c` (26 sentinels + 28 `u3r_safe_word`), `adler.c`, `ut_redo.c`, `xmas_tests.c`, `zlib.c`, and `chacha.c` were never touched by this branch at all.

6. **Wrapper functions mask some issues.** `u3r_word`, `u3i_word`, `u3r_safe_word`, `u3r_word_fit` are now wrappers dispatching to half/chub variants. Call sites using wrappers are *functionally correct* but may not be *semantically correct* (using word-width for data that's always 32-bit). This is a lower priority.
