# 64-bit Migration: Potentially Missed Changes

**Branch:** `ml/64`
**Base commit:** `ee8ed877dd460d5721d71782f060ec251b3d24c1`

This file documents locations found by regex search that match patterns of changes made elsewhere on this branch, but were **not themselves changed**. These are potential gaps in the migration.

---

## Regex Commands Used

Each section below lists the regex command, what it was looking for, and what it found.

---

## 1. SENTINEL: `0` → `u3_nul` in variadic calls

**What was changed:** ~170 call sites across the branch had their trailing `, 0)` sentinel in `u3r_mean`/`u3x_mean`/`u3i_molt` calls changed to `, u3_nul)`.

**Regex to find remaining old-style sentinels:**
```bash
# Find u3r_mean/u3x_mean calls, then check next few lines for , 0) without u3_nul
rg -n 'u3[rx]_mean\(|u3i_molt\(' --type c pkg/noun/ pkg/vere/ | while read line; do
  file=$(echo "$line" | cut -d: -f1)
  lineno=$(echo "$line" | cut -d: -f2)
  context=$(sed -n "${lineno},$((lineno+5))p" "$file" | tr '\n' ' ')
  if echo "$context" | grep -q ', *0 *)'; then
    if ! echo "$context" | grep -q 'u3_nul'; then
      echo "MISSED: $file:$lineno"
    fi
  fi
done
```

**Diff coverage:** 171 removed lines matched the old pattern.

### Files NOT changed at all (true misses)

| File | Line(s) | Current Code | Why It Needs Changing |
|------|---------|--------------|----------------------|
| `pkg/noun/jets/e/bytestream.c` | 15, 166, 300, 351, 412, 474, 529, 617, 722, 735, 769, 820, 832, 866, 921, 935, 978, 1029, 1042, 1080, 1093, 1137, 1150, 1199, 1212, 1244 | `u3x_mean(..., 0)` | **26 sentinel calls** — file was entirely untouched by this branch. Every `u3x_mean` call needs `u3_nul`. |
| `pkg/noun/jets/e/adler.c` | 11, 124 | `u3r_mean(..., 0)` and `u3x_mean(..., 0)` | **2 sentinel calls** — file untouched. |
| `pkg/noun/jets/f/ut_redo.c` | 13 | `u3r_mean(cor, ..., 0)` | **1 sentinel call** — file untouched. |

### Files changed but specific location missed (partial misses)

| File | Line | Current Code | Why It Needs Changing |
|------|------|--------------|----------------------|
| `pkg/noun/jets/b/sort.c` | 111 | `u3r_mean(cor, ..., 0)` | File was changed (static_assert update) but this sentinel was missed. |

**Total missed: 30 sentinel call sites.**

---

## 2. `wag_w` → `wag_h`

**Regex:**
```bash
rg 'wag_w' --type c -n pkg/
```

**Result:** 0 remaining hits. **Fully migrated.**

---

## 3. `mug_w` → `mug_h`

**Regex:**
```bash
rg '\bmug_w\b' --type c -n pkg/
```

**Result:** All remaining hits are in `pkg/past/` (versioned migration code frozen at 32-bit) or in comments in `pkg/noun/allocate.c` explaining the 32-bit path. These are **correct** — `past/` code is intentionally pinned, and comments describing the old format should reference `mug_w`.

**No missed changes.**

---

## 4. SIZEOF-FIX: `* 4` that should be `* sizeof(c3_w)`

**Regex:**
```bash
rg '\* 4\b' --type c -n pkg/noun/ pkg/vere/ | grep -v '_test' | grep -v '\.h:' | grep -v 'past/'
```

**Result: 18 hits** in files that were changed by this branch but where `* 4` was NOT updated at that specific location.

### Potentially missed `* 4` → `* sizeof(c3_w)`

| File | Line | Current Code | Why It Might Need Changing |
|------|------|--------------|---------------------------|
| `pkg/noun/allocate.c` | 1467 | `c3_z byt_z = ((c3_z)wor_w * 4)` | Converts words to bytes — should be `* sizeof(c3_w)` |
| `pkg/noun/jets.c` | 2340 | `u3h_mark(u3R->jed.war_p) * 4` | Mark returns word count — should be `* sizeof(c3_w)` |
| `pkg/noun/jets.c` | 2344 | `u3h_mark(u3R->jed.cod_p) * 4` | Same pattern |
| `pkg/noun/jets.c` | 2348 | `u3h_mark(u3R->jed.han_p) * 4` | Same pattern |
| `pkg/noun/jets.c` | 2352 | `u3h_mark(u3R->jed.bas_p) * 4` | Same pattern |
| `pkg/noun/jets.c` | 2370 | `u3h_mark(u3R->jed.hot_p) * 4` | Same pattern |
| `pkg/noun/nock.c` | 3197 | `qua_u[0]->siz_w * 4` | Word-to-byte conversion |
| `pkg/noun/nock.c` | 3201 | `u3h_mark(har_p) * 4` | Same pattern |
| `pkg/vere/auto.c` | 466 | `_mark_ova(car_u->ext_u) * 4` | Mark returns word count |
| `pkg/noun/vortex.c` | 371 | `u3a_mark_noun(arv_u->roc) * 4` | Mark returns word count |
| `pkg/noun/vortex.c` | 375 | `u3a_mark_noun(arv_u->yot) * 4` | Mark returns word count |
| ~~`pkg/noun/jets/e/hmac.c`~~ | ~~44~~ | ~~`memset(innpad, 0x36, padwords * 4)`~~ | **FALSE POSITIVE** — `padwords` counts `c3_h` entries (always 4 bytes). Correct as-is. |
| ~~`pkg/noun/jets/e/hmac.c`~~ | ~~45~~ | ~~`memset(outpad, 0x5c, padwords * 4)`~~ | **FALSE POSITIVE** — same reason |
| `pkg/vere/io/ames.c` | 2796 | `u3h_count(...) * 4` | Word-to-byte conversion |
| `pkg/vere/io/ames.c` | 2799 | `u3h_count(...) * 4` | Same |
| `pkg/vere/io/ames.c` | 2836 | `u3h_count(...) * 4` | Same |
| `pkg/vere/io/ames.c` | 2839 | `u3h_count(...) * 4` | Same |
| `pkg/vere/lord.c` | 980 | `siz_w * 4` | Word-to-byte conversion |

**Total: 16 true misses, 2 false positives** (hmac.c `padwords * 4` is correct — `c3_h` is always 4 bytes).

---

## 5. SIZEOF-FIX: `<< 2` that should be `<< u3a_word_bytes_shift`

**Regex:**
```bash
rg '<< 2[^0-9]' --type c -n pkg/noun/ pkg/vere/ | grep -v '_test' | grep -v 'past/'
```

**Result: 2 hits.**

| File | Line | Current Code | Assessment |
|------|------|--------------|------------|
| `pkg/noun/retrieve.c` | 1885 | `gal_h << 2` | This is inside `u3r_mug_halfs` — always operating on `c3_h` (32-bit). The `<< 2` converts half-word count to bytes. **Correct as-is** — `c3_h` is always 4 bytes. |
| `pkg/noun/urth.c` | 48 | `((c3_d)vat_u->len_w) << 2` | `len_w` here refers to the ur atom's word count which is always 32-bit words. **Likely correct as-is** but should be verified — depends on whether `buf_w` contents are c3_h or c3_w. |

---

## 6. CAST-FIX: `1U <<` that should be `((c3_w)1) <<`

**Regex:**
```bash
rg '1U\s*<<' --type c -n pkg/ | grep -v past/
```

**Result: 16 hits** outside `past/`. Analysis:

### Potentially missed

| File | Line | Current Code | Assessment |
|------|------|--------------|------------|
| `pkg/vere/disk.c` | 1628 | `1U << (u3a_page + 2)` | Should be `((c3_w)1) <<` — page size is word-dependent. **But `+ 2` should be `+ u3a_word_bytes_shift` too.** |
| `pkg/noun/allocate.c` | 333 | `(1U << u3a_page)` | Word-dependent page alloc. Should be `((c3_w)1)`. |
| `pkg/noun/allocate.c` | 359 | `(1U << u3a_page)` | Same. |
| `pkg/noun/manage.c` | 520 | `u3a_walloc(1U << u3a_page)` | Same. |
| `pkg/noun/manage.c` | 556 | `1U << u3a_page` | Same. |
| `pkg/noun/manage.c` | 605 | `1U << (12 + ...)` | Page-related shift. |
| `pkg/vere/melt.c` | 125 | `1U << 31` | Tail frame marker — depends on word size. **Missed.** |
| `pkg/vere/melt.c` | 129 | `(1U << 31) - 1` | Mask for above. **Missed.** |
| `pkg/noun/trace.h` | 19 | `1U << (u3a_page +2)` | `TRACE_PSIZE` — `+2` should be `+u3a_word_bytes_shift`. |

### Correct as-is (false positives)

| File | Line | Current Code | Why correct |
|------|------|--------------|-------------|
| `pkg/noun/allocate.h` | 165 | `(1U << u3a_min_log)` | Static assert — compile-time check. |
| `pkg/noun/manage.c` | 571 | `(1U << u3a_page)` | Static assert. |
| `pkg/noun/manage.c` | 576 | `(1U << 5)` | Bytecode version check — always 5 bits. |
| `pkg/noun/jets/e/urwasm.c` | 874, 905 | `(1U << 31)` | WASM is 32-bit — c3_h max. Correct. |
| `pkg/noun/jets/e/urwasm.c` | 1327 | `(1U << 16)` | 16-bit range check. Correct. |

**Total: ~9 potentially missed `1U <<` conversions.**

---

## 7. MACRO-PARAMETERIZE: Remaining hardcoded `>> 5` and `& 31`

**Regex:**
```bash
rg '>> ?5\b' --type c -n pkg/noun/ pkg/vere/ | grep -v '_test' | grep -v 'past/'
rg '& ?31\b' --type c -n pkg/noun/ pkg/vere/ | grep -v '_test' | grep -v 'past/'
```

**Results:**

| File | Line | Current Code | Assessment |
|------|------|--------------|------------|
| `pkg/vere/io/ames.c` | 69 | `256 >> 5` | Array size for 256-bit bitfield using c3_h (always 32-bit). **Correct** — should use `u3a_half_bits_log` which is already what the code does semantically. This is for a fixed-size 256-entry galaxy array divided into 32-bit words. |
| `pkg/vere/io/ames.c` | 2440 | `imp_y >> 5` | Galaxy index into 32-bit bitfield. Uses `c3_h` entries. **Correct as-is** — this indexes into `log_h[]` which is always `c3_h`. |
| `pkg/noun/manage.c` | 656 | `(pam_d >> 6) & 31` | Extracts 5-bit bytecode version from page magic. **Correct as-is** — this is a fixed protocol field, not word-size-dependent. |

**No missed changes in this category.**

---

## 8. MACRO-PARAMETERIZE: Remaining `u3r_met(5, ...)` calls

**Regex:**
```bash
rg 'u3r_met\(5\b' --type c -n pkg/ | grep -v past/ | grep -v '_test'
```

**Result: 4 hits.** All appear to be checking "does this fit in a single 32-bit word" — which is the purpose of `u3r_met(5, ...)` in the old code. In 64-bit mode, `u3r_met(u3a_word_bits_log, ...)` would check "fits in a 64-bit word" which is different semantics.

| File | Line | Current Code | Assessment |
|------|------|--------------|------------|
| `pkg/noun/retrieve.c` | 1328 | `u3r_met(5, a) > 1` in `u3r_half_fit` | **Correct** — `u3r_half_fit` specifically checks if it fits in 32 bits. |
| `pkg/noun/retrieve.c` | 1504 | `u3r_met(5, dat)` in safe_half | **Correct** — same reason. |
| `pkg/noun/jets/e/scr.c` | 45-46 | `u3r_met(5, r) > 1` | Checking if scrypt params fit in 32-bit `c3_w`. **Should be `u3r_met(u3a_word_bits_log, ...)` since the variable is `c3_w`.** But `r_w` is used as a 32-bit param to urcrypt — **so hardcoded 5 is actually correct.** |
| `pkg/noun/imprison.c` | 465 | `u3i_slab_bare(&sab_u, 5, a_w)` | Inside `u3i_halfs` — always operates on 32-bit halfs. **Correct.** |

**No missed changes — all are intentionally 32-bit.**

---

## 9. Old-style constants: `0x80000000` in palloc.c

**Regex:**
```bash
rg '0x80000000' --type c -n pkg/noun/palloc.c
```

**Result: 6 hits** at lines 1528, 1539, 1618, 1629, 1709, 1761.

These are used as a "premark" sentinel in the page marking buffer (`u3a_Mark.buf_w`). The buffer stores `c3_w` values, so `0x80000000` may be too small if `c3_w` is 64-bit.

| File | Line(s) | Current Code | Assessment |
|------|---------|--------------|------------|
| `pkg/noun/palloc.c` | 1528, 1539, 1618, 1629, 1709, 1761 | `0x80000000` as premark sentinel | **Potentially missed** — if `buf_w` elements are `c3_w` (64-bit in VERE64), this sentinel could collide with valid mark counts. Should use `u3a_indirect_flag` or `((c3_w)1 << (u3a_word_bits - 1))`. |

---

## 10. UINT32_MAX remaining usage

**Regex:**
```bash
rg 'UINT32_MAX' --type c -n pkg/ | grep -v past/ | grep -v '_test'
```

**Result: Multiple hits.** Most are overflow guards (`u3_assert(UINT32_MAX >= ...)`) that are **intentionally checking the 32-bit boundary** regardless of word size. These are correct.

| File | Line | Assessment |
|------|------|------------|
| `pkg/vere/ward.c:185` | `u3_assert(UINT32_MAX >= met_w)` | Correct — bounds check |
| `pkg/vere/io/term.c:538,555` | `u3_assert(UINT32_MAX >= ...)` | Correct — bounds check |
| `pkg/vere/io/ames.c:31,515` | `_CZAR_GONE UINT32_MAX`, assert | Correct — sentinel for c3_h |
| `pkg/vere/io/mesa/bitset.c:8` | `u3_assert(UINT32_MAX >= len_d)` | Correct — ensures c3_h bounds |
| `pkg/noun/jets/g/plot.c:48` | `UINT32_MAX` overflow check | **Could use `c3_w_max`** but semantically checks 32-bit overflow for the result type. |
| `pkg/noun/jets/e/fein_ob.c:64` | `UINT32_MAX < met_w` | Correct — Feistel cipher is 32-bit |
| `pkg/noun/jets/e/urwasm.c:730,1162,1238,1492` | `UINT32_MAX <= met_w` | Correct — WASM is 32-bit |
| `pkg/noun/jets/e/fynd_ob.c:68` | `UINT32_MAX < met_w` | Correct — inverse Feistel is 32-bit |
| `pkg/noun/jets/b/find.c:8` | Commented out | N/A |
| `pkg/noun/jets/b/lent.c:9` | Commented out | N/A |

**No missed changes — all remaining UINT32_MAX usage is intentional.**

---

## 11. `pip_w` remaining

**Regex:**
```bash
rg '\bpip_w\b' --type c -n pkg/ | grep -v past/
```

**Result: 3 hits in `pkg/vere/xmas_tests.c`** (lines 20, 27, 29). This file was NOT changed in the diff.

| File | Line(s) | Current Code | Assessment |
|------|---------|--------------|------------|
| `pkg/vere/xmas_tests.c` | 20, 27, 29 | `lan_u.pip_w`, `nal_u.pip_w` | **Missed** — should be `pip_h` to match `u3_lane` struct change. |

---

## 12. `c3_sift_word` / `c3_etch_word` remaining

**Regex:**
```bash
rg 'c3_sift_word|c3_etch_word' --type c -n pkg/
```

**Result: 0 hits. Fully migrated.**

---

## 13. Unchanged files with `u3r_safe_word` calls

**Regex:**
```bash
rg 'u3r_safe_word' --type c -l pkg/ | while read f; do
  changed=$(git diff --name-only ee8ed877dd..HEAD -- "$f" | wc -l)
  [ "$changed" = "0" ] && echo "$f"
done
```

**Note:** `u3r_safe_word` is now a wrapper that dispatches to `u3r_safe_half` (32-bit) or `u3r_safe_chub` (64-bit). So existing call sites are functionally correct. However, files that were entirely untouched may have other issues (sentinel, types, etc).

| File | `u3r_safe_word` count | Other issues |
|------|-----------------------|--------------|
| `pkg/noun/jets/e/bytestream.c` | 28 | Also 26 sentinel misses, 1 `u3r_word` call |
| `pkg/noun/jets/e/adler.c` | 1 | Also 2 sentinel misses |
| `pkg/noun/jets/e/zlib.c` | 2 | No sentinel issues (no u3r_mean calls) |

---

## 14. Unchanged files with `u3r_word_fit` calls

**Regex:**
```bash
rg 'u3r_word_fit' --type c -l pkg/ | while read f; do
  changed=$(git diff --name-only ee8ed877dd..HEAD -- "$f" | wc -l)
  [ "$changed" = "0" ] && echo "$f"
done
```

`u3r_word_fit` is also a wrapper now, so call sites are functionally correct. But:

| File | Calls | Assessment |
|------|-------|------------|
| `pkg/noun/jets/e/chacha.c` | 2 | **File entirely untouched.** Has `u3r_word_fit` and `u3r_safe_chub` calls. No sentinel issues. Functionally correct via wrappers. |

---

## Summary

| Category | Missed Count | Severity |
|----------|-------------|----------|
| **SENTINEL** (`0` → `u3_nul`) | **30 call sites** (3 files untouched + 1 partial) | **High** — will cause vararg misinterpretation in 64-bit mode |
| **SIZEOF** (`* 4` → `* sizeof(c3_w)`) | **16 locations** (2 hmac.c false positives removed) | **High** — will compute wrong byte counts in 64-bit mode |
| **CAST** (`1U <<` → `((c3_w)1) <<`) | **~9 locations** | **Medium-High** — shift results will be truncated to 32 bits |
| **`0x80000000` sentinel** in palloc.c | **6 locations** | **Medium** — may collide with valid values if c3_w is 64-bit |
| **`pip_w`** in xmas_tests.c | **3 locations** | **Low** — test file, won't compile if struct changed |
| **`melt.c` bit markers** | **2 locations** | **High** — `1U << 31` tail frame marker is word-size-dependent |

### Entirely untouched files that need changes

1. **`pkg/noun/jets/e/bytestream.c`** — 26 sentinel + other issues (highest priority)
2. **`pkg/noun/jets/e/adler.c`** — 2 sentinel issues
3. **`pkg/noun/jets/f/ut_redo.c`** — 1 sentinel issue
4. **`pkg/vere/xmas_tests.c`** — 3 `pip_w` issues
5. **`pkg/noun/jets/e/zlib.c`** — no critical issues (wrappers handle it)
6. **`pkg/noun/jets/e/chacha.c`** — no critical issues (wrappers handle it)
