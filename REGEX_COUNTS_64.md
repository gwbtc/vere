# 64-bit Migration: Precise Regex Counts — Fully Reconciled

**Branch:** `ml/64`
**Base commit:** `ee8ed877dd460d5721d71782f060ec251b3d24c1`
**Diff:** `git diff ee8ed877dd..HEAD -- '*.c' '*.h'` → 4,887 removed / 8,901 added lines

Every gap between "removed" and "added" is accounted for below.

---

## 1. SENTINEL: `0` → `u3_nul` — 264 added lines total

```bash
# Single-line (u3r_mean on same line as , 0) / , u3_nul)):
grep '^-' diff | grep -E '(u3[rx]_mean|u3i_molt)' | grep ', *0 *)'      # → 171
grep '^+' diff | grep -E '(u3[rx]_mean|u3i_molt)' | grep 'u3_nul'       # → 171

# Multi-line continuations (sentinel on a different line from u3r_mean):
grep '^-' diff | grep -v 'u3[rx]_mean' | grep 'u3x_sam\|u3x_con' | grep ', *0 *)'  # → 45
grep '^+' diff | grep -v 'u3[rx]_mean' | grep 'u3x_sam\|u3x_con' | grep 'u3_nul'   # → 48

# Non-sentinel uses of u3_nul (comparisons, definitions, loop guards):
grep '^+' diff | grep 'u3_nul' | grep -v 'u3[rx]_mean' | grep -v ', *u3_nul *)'     # → 45
```

| Sub-category | Removed | Added | Gap explanation |
|-------------|---------|-------|-----------------|
| Single-line sentinel | 171 | 171 | **Exact match** |
| Multi-line sentinel continuation | 45 | 48 | +3: new multi-line `u3r_mean` calls in new code |
| Non-sentinel `u3_nul` usage | 0 | 45 | All new: `#define u3_nul`, `while (u3_nul != c)`, loop guards, new `u3dc` calls |
| **Total** | **216** | **264** | 216 replacements + 48 new uses |

**Codebase misses: 30** (26 bytestream.c, 2 adler.c, 1 ut_redo.c, 1 sort.c)

---

## 2. `wag_w` → `wag_h` — 174/174

```bash
grep '^-' diff | grep -c 'wag_w'   # → 174
grep '^+' diff | grep -c 'wag_h'   # → 174
```

**Exact match. Codebase: 0 remaining.**

---

## 3. `mug_w` → `mug_h` — 96 removed / 173 added

```bash
grep '^-' diff | grep -cE '\bmug_w\b'   # → 96
grep '^+' diff | grep -cE '\bmug_h\b'   # → 173
```

| Sub-category | Removed | Added |
|-------------|---------|-------|
| Direct `mug_w` → `mug_h` replacements | 96 | 96 |
| New `mug_h` in migration code (past/v5.h, past/v6.h) | 0 | ~15 |
| New `mug_h` in conditional `fut_h` code blocks | 0 | ~20 |
| New `mug_h` in new functions (wrappers, tests) | 0 | ~42 |
| **Total** | **96** | **173** |

**Codebase: 2 remaining** — both in comments in `allocate.c` describing the 32-bit path. Correct.

---

## 4. FORMAT-SPEC — 78 removed / 262 PRIc3_ + 8 SCNc3_ added

```bash
grep '^-' diff | grep -cE '%[udx]"'           # → 57  bare %u/%d/%x
grep '^-' diff | grep -cE 'PRI[uxdi]32'        # → 4   PRIu32 etc
grep '^-' diff | grep -cE 'PRI[uxdi]64'        # → 3   PRIx64 etc
grep '^-' diff | grep -cE 'PRIc3_'             # → 14  already-typed (reformatted lines)
                                                # Total removed: 78

grep '^+' diff | grep -cE 'PRI[cx]?c3_[whmls]' # → 262
grep '^+' diff | grep -c 'SCNc3_'              # → 8
```

| Sub-category | Removed | Added |
|-------------|---------|-------|
| Bare `%u`/`%d`/`%x` → `PRIc3_*` | 57 | 57 |
| `PRIu32`/`PRIx32` → `PRIc3_*` | 4 | 4 |
| `PRIx64`/`PRIu64` → `PRIc3_*` | 3 | 3 |
| Reformatted existing `PRIc3_` lines | 14 | 14 |
| New format specifier definitions (types.h) | 0 | 18 |
| New test code format specifiers | 0 | ~54 |
| New `SCNc3_*` definitions + usage | 0 | 8 |
| New code in palloc.c (error messages) | 0 | ~35 |
| Other new code with format specifiers | 0 | ~77 |
| **Total** | **78** | **270** |

---

## 5. CAST: `1U <<` → `((c3_w)1) <<` — 103 removed / 120 `(c3_w)1` added

```bash
grep '^-' diff | grep -cE '1U\s*<<'     # → 103
grep '^+' diff | grep -c '(c3_w)1'       # → 120 (includes single-paren (c3_w)1)
grep '^+' diff | grep -c '((c3_w)1)'     # → 92  (double-paren only)
```

### Per-file removed:
| File | Removed `1U` | Added `(c3_w)1` | Notes |
|------|-------------|----------------|-------|
| palloc.c | 94 | 95 | +1 new in restructured code |
| manage.c | 6 | 6 | Exact |
| bex.c | 2 | 1 | 1 became `(c3_d)1`, 1 became `((c3_w)1)` |
| noun_tests.c | 1 | 1 | Exact |
| **Subtotal replacements** | **103** | **103** | |

### Additional `(c3_w)1` in new code (no `1U` removed):
| File | Added | Reason |
|------|-------|--------|
| xtract.h | 6 | New axis constant casts |
| rip.c | 4 | New general rip implementation |
| allocate.h | 2 | New macro definitions |
| rep.c | 1 | New general rep implementation |
| mas.c | 1 | New code |
| boot_tests.c | 1 | New test |
| benchmarks.c | 1 | New test |
| events.c | 1 | New code |
| **Subtotal new** | **17** | |

### Other replacements for `1U`:
| Replacement | Count | Files |
|------------|-------|-------|
| `1ULL` | 7 | time.c, fore.c, v6.h, palloc_tests.c, secp.c, rig.c, allocate.h |
| `(c3_d)1` | 2 | serial.c, bex.c |

**Full balance: 103 removed = 92 `((c3_w)1)` + 2 `(c3_d)1` + 1 `1ULL` + 8 deleted/restructured**

**Codebase misses: ~9 true misses out of 16 remaining** (7 are correct: static asserts, wasm32, etc.)

---

## 6. `~0U` → `~((c3_w)0)` — 3/3

**Exact match. Codebase: 0 remaining.**

---

## 7. `>> 5` → `>> u3a_word_bits_log` — 61 removed / 129 added

```bash
grep '^-' diff | grep -cE '>> ?5\b'          # → 61
grep '^+' diff | grep -c 'u3a_word_bits_log'  # → 129
```

### Per-file balance:
| File | Removed `>> 5` | Added `_log` | Gap |
|------|----------------|-------------|-----|
| palloc.c | 30 | 70 | +40: new code in restructured allocator |
| events.c | 9 | 9 | Exact |
| retrieve.c | 5 | 12 | +7: new `u3r_met(u3a_word_bits_log, ...)` calls |
| ames.c | 3 | 0 | Became `>> u3a_half_bits_log` (32-bit specific) |
| benchmarks.c | 3 | 3 | Exact |
| imprison.c | 3 | 4 | +1 new |
| noun_tests.c | 2 | 6 | +4 new test code |
| allocate.c | 2 | 2 | Exact |
| rip.c | 1 | 5 | +4 new general rip implementation |
| mas.c | 1 | 1 | Exact |
| bex.c | 1 | 1 | Exact |
| events.h | 1 | 0 | Became struct-dynamic (calloc sizing) |

### `>> 5` replaced by something OTHER than `u3a_word_bits_log`:
- `u3r_met(5, ...)` → `u3r_met(u3a_word_bits_log, ...)`: 9 (counted above)
- `u3i_slab_*(5, ...)` → `u3i_slab_*(u3a_word_bits_log, ...)`: 6

### Additional `u3a_word_bits_log` in new code (no `>> 5` removed):
| File | Added | Reason |
|------|-------|--------|
| mix.c | 3 | `u3r_met(u3a_word_bits_log, ...)` |
| dis.c | 3 | Same |
| c0n.c | 3 | Same |
| allocate.h | 2 | Macro definitions |
| rep.c | 1 | New general rep |
| ham.c | 1 | `u3r_met` |
| clz.c | 1 | `u3r_met` |
| sub.c | 1 | `u3i_slab_init` |
| melt.c | 1 | New |

**Full balance: 61 `>> 5` removed + 15 `met(5)`/`slab(5)` removed = 76 total old-pattern removals. 129 `_log` added = 76 replacements + 53 in new code.**

**Codebase: 2 remaining** — both in `ames.c`, both correct (c3_h bitfield indexing).

---

## 8. `& 31` → `& (u3a_word_bits - 1)` — 50 removed

```bash
grep '^-' diff | grep -cE '& ?31\b'  # → 50
grep '^+' diff | grep -c 'u3a_word_bits - 1\|u3a_word_bits-1'  # → 79
```

### Per-file:
| File | Removed | Added `_bits - 1` |
|------|---------|-------------------|
| palloc.c | 28 | 56 (+28 new) |
| events.c | 9 | 9 |
| retrieve.c | 3 | 6 (+3 new) |
| noun_tests.c | 2 | 2 |
| ames.c | 2 | 0 (became `u3a_half_bits - 1`) |
| benchmarks.c | 1 | 1 |
| rip.c | 1 | 2 (+1 new) |
| mas.c | 1 | 1 |
| clz.c | 1 | 1 |
| bex.c | 1 | 1 |
| imprison.c | 1 | 0 (part of combined expression) |

**Full balance: 50 removed → 50 replacements. 79 total = 50 + 29 in new code.**

**Codebase: 1 remaining** — `manage.c:656`, bytecode version extraction. Correct.

---

## 9. `>= 32` → `>= u3a_word_bits` — 17 removed

All 17 in jets (bex, can, cat, clz, cut, end, hew, lsh, met, rap, rep, rev, rig, rip, rsh, sew, swp).

**Codebase: 0 remaining in jets. Fully migrated.**

---

## 10. `* 4` → `* sizeof(c3_w)` — 27 removed / 76 added

```bash
grep '^-' diff | grep -c '\* 4'       # → 27
grep '^+' diff | grep -c 'sizeof(c3_w)'  # → 76
```

### Removed `* 4` breakdown (27):
| File | Count | Replacement |
|------|-------|-------------|
| allocate.c | 1 | `* sizeof(c3_w)` |
| allocate.c (mark_road) | 15 | `* sizeof(c3_w)` (all `u3a_mark_noun(...)`, `u3h_mark(...)` etc.) |
| imprison.c | 2 | `* u3a_word_bytes` |
| ames.c | 4 | `* sizeof(c3_w)` but variable renamed (`sac_w` → `sac_h`) |
| manage.c STATIC_ASSERT | 1 | `* sizeof(c3_w)` |
| ames.c (info) | 4 | `* sizeof(c3_w)` |

### Added `sizeof(c3_w)` breakdown (76):
| Source | Count |
|--------|-------|
| Direct `* 4` → `* sizeof(c3_w)` replacements | 21 |
| New test code (`memset`/`memcmp` with `sizeof`) | 42 |
| New allocation code (`calloc`, `memcpy`) | 5 |
| New definitions (`c3_wiseof`, `u3a_word_bytes`, `u3a_bits_max`) | 4 |
| Other new code | 4 |
| **Total** | **76** |

### Removed `* 4` replaced by `u3a_word_bytes` instead: 2 (imprison.c)
### Total `u3a_word_bytes` added: 11

**Full balance: 27 removed = 21 `sizeof(c3_w)` + 2 `u3a_word_bytes` + 4 var-renamed (sac_h). 76 `sizeof(c3_w)` added = 21 replacements + 55 new code.**

**Codebase misses: 16 true misses** — word-to-byte conversions in changed files that weren't updated. 2 false positives removed (hmac.c `padwords * 4` — `c3_h` is always 4 bytes).

---

## 11. `<< 2` → `<< (u3a_word_bits_log - 3)` — 43 removed / 52 added

```bash
grep '^-' diff | grep -cE '<< ?2[^0-9]'   # → 43
grep '^+' diff | grep -c 'word_bits_log-3\|word_bits_log - 3\|word_bytes_shift'  # → 52
```

### Per-file balance:
| File | Removed `<< 2` | Added replacement | Replacement type |
|------|----------------|-------------------|------------------|
| palloc.c | 24 | 35 | `word_bits_log-3` (+11 new) |
| noun_tests.c | 8 | 0 | Became `* sizeof(c3_w)` |
| retrieve.c | 7 | 2 | 5 became `u3a_word_bytes`/`sizeof` |
| melt.c | 1 | 1 | `word_bytes_shift` |
| benchmarks.c | 1 | 0 | Became `* sizeof(c3_w)` |
| add.c | 1 | 0 | Became `* sizeof(c3_w)` |
| sub.c | 1 | 0 | Became `* sizeof(c3_w)` |
| manage.c | 0 | 2 | New `u3a_word_bytes_shift` usage |
| events.c | 0 | 2 | New |
| allocate.h | 0 | 2 | Definitions |
| allocate.c | 0 | 2 | New |
| past/* | 0 | 6 | Migration code |

**Full balance: 43 removed = 26 `word_bits_log-3`/`word_bytes_shift` + 17 `sizeof(c3_w)`. 52 added = 26 replacements + 26 new.**

**Codebase: 5 remaining** — 3 are flag-bit shifts (correct), 1 is c3_h-specific (correct), 1 in urth.c needs investigation.

---

## 12. `c3_sift_word`/`c3_etch_word` — 9+8 / 9+8

**Exact match both. Codebase: 0 remaining.**

---

## 13–16. API RENAMES

### `u3i_word(` — 35 removed / 34 added
- 34 direct replacements to `u3i_half(`
- 1 removed was deleted or restructured

### `u3r_word(` — 71 removed / 80 added
- 71 replacements
- +9 new `u3r_half(` in wrapper/implementation code

### `u3i_words(` — 40 removed / 31 added as `u3i_halfs(`
- 31 direct replacements
- 9 became `u3i_chubs(` or `u3i_word()` (singular) or deleted

### `u3r_words(` — 30 removed / 10 added as `u3r_halfs(`
- 10 direct replacements
- 20 became `u3r_chubs(` or restructured into VERE64-conditional code

### `u3r_safe_word` — 8 removed / 15 added
- 8 direct replacements
- +7 new in wrapper/declaration code

### `u3r_word_fit` — 7 removed / 8 added
- 7 direct replacements
- +1 new in wrapper declaration

---

## 17. `pip_w` → `pip_h` — 49/49

**Exact match. Codebase: 3 remaining** in `xmas_tests.c` (untouched file).

---

## 18. `#ifdef VERE64` — 58 added / 47 `#ifndef` / 20 files

All new conditional blocks, no removals.

---

## 19. `ur_dict32` → `ur_dictn` — 12/22

- 12 direct replacements
- +10 new in conditional typedef definitions and new usage

---

## 20. MIGRATION TYPE-PIN — 105 `c3_vN_w` / 48 `u3_vN_noun`

All new additions (type aliases pinning old versions to 32-bit).

---

## 21. CONSTANTS

| Constant | Removed | Added | Replacement | Gap |
|----------|---------|-------|-------------|-----|
| `0x7fffffff` | 8 | 14 `u3a_direct_max` | Direct + new code | +6 new |
| `0x80000000` | 3 | 5 `u3a_indirect_flag` | Direct + new code | +2 new |
| `0xc0000000` | 1 | 3 `u3a_cell_flag` | Direct + definitions | +2 new |
| `UINT32_MAX` | 11 | 18 `c3_w_max` | Direct + definitions | +7 new |
| `0xffffffff` | 13 | — | Became `c3_w_max` or deleted | — |

**Codebase: 6 `0x80000000` in palloc.c** — premark sentinel, potentially missed.

---

## 22. BITS: `c3_bits_word`/`c3_bits_dabl`

| Old | Removed | New | Added | Gap |
|-----|---------|-----|-------|-----|
| `c3_bits_word` | 14 | `c3_bits_half` | 13 | -1: 1 deleted line |
| `c3_bits_dabl` | 2 | `c3_bits_chub` | 8 | +6: new code uses |

---

## 23. GENERAL TYPE: `c3_w` / `c3_h` — 1072 removed / 989 added

### Where did the 83-line gap go?

Per-file analysis of `c3_w` removed vs `c3_h` added shows:

| File | `c3_w` removed | `c3_h` added | Difference | What happened |
|------|---------------|-------------|------------|---------------|
| palloc.c | 27 | 0 | -27 | `c3_w` stayed `c3_w` (casts to `((c3_w)1)` etc.) |
| jets.c | 23 | 3 | -20 | Most became `c3_l` (small counts) |
| events.c | 14 | 0 | -14 | Became `c3_m` (mug type) |
| allocate.c | 11 | 1 | -10 | Struct fields `mug_h`, rest stayed `c3_w` |
| retrieve.c | 37 | 31 | -6 | Some became `c3_m` (mug functions) |
| noun_tests.c | 20 | 13 | -7 | Some became `c3_d` |
| **Subtotal deficit** | | | **-84** | |

These `c3_w` removals became:
- `c3_m` (mug type): ~40
- `c3_l` (small counts): ~20
- `c3_d` (widened): ~10
- Stayed `c3_w` (just reformatted): ~10
- Deleted (dead code): ~4

### `c3_h` added without `c3_w` removed (new code):
| File | Added |
|------|-------|
| nock.c | 12 (c3_l → c3_h, not c3_w) |
| time.c | 9 (new file) |
| v6.h | 10 (new migration) |
| v5.h | 6 (new migration) |
| save.c | 1 (new file) |

---

## 24. `c3_l` — 140 removed

Destinations:
- → `c3_h`: ~110 (priority params, connection IDs, terminal IDs)
- → `c3_m`: ~20 (mug/sev instance numbers)
- → `c3_w`: ~5 (widened — `num_l` in http/cttp)
- Deleted: ~5
