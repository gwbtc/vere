# 64-bit Migration: Change Classifications

**Branch:** `msl/64`
**Base commit:** `ee8ed877dd460d5721d71782f060ec251b3d24c1`
**Files changed:** 264
**Lines changed:** ~10,329 additions, ~4,874 deletions

## Classification Taxonomy

| Code | Description |
|------|-------------|
| `TYPE-W-TO-H` | Changed `c3_w` to `c3_h` — narrows to always-32-bit for values that must remain 32-bit (mug hashes, IPv4 addresses, loop counters for bounded structures, LMDB return codes, terminal dimensions, etc.) |
| `TYPE-L-TO-H` | Changed `c3_l` to `c3_h` — narrows always-32-bit loom-pointer type to always-32-bit half |
| `TYPE-L-TO-M` | Changed `c3_l` to `c3_m` — narrows to always-32-bit mug hash type |
| `TYPE-WS-TO-HS` | Changed `c3_ws` to `c3_hs` — signed half-word |
| `TYPE-W-TO-D` | Changed `c3_w` to `c3_d` — widens to always-64-bit for values that may exceed 32-bit range |
| `TYPE-L-TO-W` | Changed `c3_l` to `c3_w` — widens from always-32-bit to conditional word |
| `TYPE-D-TO-W` | Changed `c3_d` to `c3_w` — d was always 64-bit, w is now 64-bit in VERE64 mode |
| `TYPE-NEW` | New type definitions (c3_h, c3_q, conditional c3_w/c3_l) |
| `TYPE-CONDITIONAL` | Types conditionally defined based on VERE64 flag |
| `FORMAT-SPEC` | Changed format specifiers (`%u`/`%d`/`%x` → `PRIc3_w`/`PRIc3_h`/`PRIxc3_w` etc.) |
| `FORMAT-SPEC-NEW` | New format specifier macro definitions |
| `FORMAT-SPEC-CONDITIONAL` | Format specifiers conditionally defined based on VERE64 |
| `SENTINEL` | Changed varargs sentinel `0` → `u3_nul` in `u3r_mean`/`u3x_mean` calls (64-bit safety) |
| `API-RENAME` | Core API function renamed (`u3i_word`→`u3i_half`, `u3r_word`→`u3r_half`, `u3i_words`→`u3i_halfs`, `u3r_words`→`u3r_halfs`, `u3r_safe_word`→`u3r_safe_half`, `c3_sift_word`→`c3_sift_half`, `c3_etch_word`→`c3_etch_half`, `u3v_wish_n`→`u3v_wish_w`) |
| `API-WRAPPER` | New wrapper function that dispatches to half/chub variant via `#ifdef VERE64` |
| `API-CALL-UPDATE` | Call sites updated to use renamed/new APIs |
| `MACRO-NEW` | New preprocessor macros introduced |
| `MACRO-CONDITIONAL` | Macros conditionally defined based on VERE64 |
| `MACRO-RENAME` | Existing macros renamed (e.g., `c3_lz_w`→`c3_lz_h`) |
| `MACRO-PARAMETERIZE` | Hardcoded constants replaced with symbolic macros (`31`→`u3a_word_bits-1`, `5`→`u3a_word_bits_log`, `32`→`u3a_word_bits`, `0x7fffffff`→`u3a_direct_max`, `0xffffffff`→`c3_w_max`, `0x80000000`→`u3a_indirect_flag`) |
| `SIZEOF-FIX` | Hardcoded sizes replaced with portable expressions (`*4`→`*sizeof(c3_w)`, `<<2`→`<<u3a_word_bytes_shift`) |
| `CAST-FIX` | Added/changed casts for width safety (`1U`→`((c3_w)1)`, `(c3_w)`→`(c3_h)`, added `(c3_w)` to variadic args) |
| `LITERAL-FIX` | Changed integer literals for 64-bit safety (added `ULL` suffix, `(c3_d)1 <<`) |
| `IFDEF-VERE64` | New `#ifdef VERE64` / `#ifndef VERE64` conditional compilation blocks |
| `STRUCT-FIELD` | Changed struct field types or names (covered by TYPE-* but specifically struct definitions) |
| `STRUCT-LAYOUT` | Struct alignment or layout attribute changed (`__attribute__((aligned(4)))`) |
| `STRUCT-DYNAMIC` | Changed fixed-size array to dynamically allocated pointer |
| `FUNC-NEW` | New function added |
| `FUNC-SIGNATURE` | Changed function parameter or return types |
| `FUNC-BODY-LOGIC` | Logic changes in function bodies for 64-bit support |
| `FUNC-RENAME` | Function renamed |
| `BIT-MANIP` | Changes to bit shifting/masking for word-size adaptation |
| `CONSTANT-NEW` | New named constants |
| `CONSTANT-CONDITIONAL` | Constants conditionally defined based on VERE64 |
| `CONSTANT-CHANGE` | Changed constant values or expressions |
| `BUILD-SYSTEM` | Build configuration changes (build.zig, pkg/*/build.zig) |
| `CI-WORKFLOW` | CI/CD pipeline changes (.github/workflows/) |
| `LONGJMP-CHANGE` | Changes to setjmp/longjmp mechanism for VERE64 compatibility |
| `NETWORK-PROTOCOL` | Wire protocol serialization changes (`c3_sift_word`→`c3_sift_half`, `c3_etch_word`→`c3_etch_half`) |
| `MIGRATION-NEW` | New migration version files or substantial new migration code |
| `MIGRATION-TYPE-PIN` | Versioned type aliases (`c3_vN_w`, `u3_vN_noun`, `u3vNp()`) to freeze past snapshot formats at 32-bit |
| `UR-CONDITIONAL` | New conditional typedefs/macros in ur/ package (`ur_dictn_t`, `ur_bsrn_any`, `ur_serial_size_t`) |
| `TEST-NEW` | New test functions |
| `TEST-UPDATE` | Updates to existing tests for 64-bit |
| `TEST-IFDEF` | Conditional test code based on VERE64 |
| `COMMENT` | Comment additions or modifications |
| `WHITESPACE` | Whitespace-only changes |
| `DELETED-CODE` | Removed functions, dead code, or simplified implementations |
| `DEBUG-CODE` | Added debug/diagnostic code (break functions, commented-out tracing) |
| `DEFAULT-CONFIG` | Changed default configuration values |
| `NEWLINE-FIX` | Changed `\n` to `\r\n` for terminal output |
| `OVERFLOW-GUARD` | Added assertions or overflow checks |
| `STATIC-ASSERT` | Commented out or updated static assertions for 64-bit compatibility |

---

## Classified Changes

### pkg/c3/types.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 14-15 | TYPE-NEW | New 128-bit types `c3_q` (unsigned) and `c3_qs` (signed) |
| 18-19 | TYPE-NEW | New `c3_h` (uint32_t) and `c3_hs` (int32_t) always-32-bit types |
| 28 | COMMENT | `c3_g` comment changed to "u3a_word_bits log" |
| 30-38 | TYPE-CONDITIONAL | `c3_w`, `c3_ws`, `c3_l` conditionally defined based on VERE64 |
| 79-95 | FORMAT-SPEC-CONDITIONAL | `PRIc3_w`, `PRIc3_ws`, `PRIxc3_w`, `PRIXc3_w` conditionally defined; `SCNc3_w` introduced |
| 85-86,93-94 | FORMAT-SPEC-NEW | `PRIc3_h`, `PRIc3_hs` format specifiers (always 32-bit) |
| 97-115 | FORMAT-SPEC-CONDITIONAL | `PRIc3_l`, `PRIc3_ls`, `PRIxc3_l`, `PRIXc3_l` conditionally defined |
| 102-105,111-114 | FORMAT-SPEC-NEW | `PRIxc3_h`, `PRIXc3_h`, `PRIc3_ls_new` |
| 117-120 | FORMAT-SPEC-NEW | `PRIc3_m`, `PRIc3_ms`, `PRIxc3_m`, `PRIXc3_m` for mote type |

### pkg/c3/defs.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 52-56 | MACRO-CONDITIONAL | `c3_wiseof()` conditional: divides by 4 (32-bit) or 8 (64-bit) |
| 61-67 | MACRO-RENAME | `c3_lz_w`/`c3_tz_w`/`c3_pc_w` renamed to `c3_lz_h`/`c3_tz_h`/`c3_pc_h` (always 32-bit) |
| 84-85 | MACRO-RENAME | `c3_bits_word` → `c3_bits_half`; `c3_bits_dabl` → `c3_bits_chub` |
| 87-97 | MACRO-CONDITIONAL | `c3_bits_word`, `c3_lz_w`, `c3_tz_w`, `c3_pc_w` conditionally alias to half/chub |
| 121 | FUNC-SIGNATURE | `c3_rand()` parameter `c3_w*` → `c3_h*` |
| 153-157 | FUNC-RENAME | `c3_sift_word()` → `c3_sift_half()`, returns `c3_h` |
| 180-186 | FUNC-RENAME | `c3_etch_word()` → `c3_etch_half()`, takes `c3_h` |
| 268-283 | FUNC-RENAME | `c3_align_w()` → `c3_align_h()`, `c3_h` types |
| 291-298 | IFDEF-VERE64, FUNC-NEW | New `c3_align_w()` that dispatches to `c3_align_h`/`c3_align_d` |
| 309-310 | CONSTANT-NEW | `c3_h_max` (0xffffffff), `c3_d_max` |
| 312-316 | CONSTANT-CONDITIONAL | `c3_w_max` conditionally `c3_h_max` or `c3_d_max` |

### pkg/c3/defs.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 5-6 | FUNC-RENAME | `c3_sift_word` → `c3_sift_half`, return `c3_h` |
| 13 | FUNC-RENAME | `c3_etch_word` → `c3_etch_half`, param `c3_h` |
| 17 | FUNC-RENAME | `c3_align_w` → `c3_align_h` |

### pkg/c3/portable.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 177-181 | CONSTANT-CONDITIONAL | `U3_OS_LoomBits` consolidated: 30 (32-bit) vs 46 (64-bit) |

### pkg/c3/platform/windows/compat.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 200-201,227 | IFDEF-VERE64 | 64-bit `mmap` support via split `dwMaxSizeHigh`/`dwMaxSizeLow` |

### pkg/noun/allocate.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 16-20 | MACRO-CONDITIONAL | `u3a_vits`: 2 (32-bit) vs 0 (64-bit) |
| 22 | MACRO-NEW | `u3a_word_bytes` = `sizeof(c3_w)` |
| 24-30 | CONSTANT-NEW | 32-bit constants: `u3a_half_bits`, `u3a_half_bits_log`, `u3a_32_indirect_mask`, etc. |
| 32-38 | CONSTANT-NEW | 64-bit constants: `u3a_chub_bits`, `u3a_chub_bits_log`, `u3a_64_indirect_mask`, etc. |
| 40-58 | MACRO-CONDITIONAL | `u3a_word_bits`, `u3a_indirect_mask`, `u3a_direct_max`, etc. mapped conditionally |
| 62 | CAST-FIX | `u3a_walign` uses `(c3_w)1` |
| 70 | CAST-FIX | `u3a_bits_max` uses `(c3_w)8` |
| 74-78 | MACRO-CONDITIONAL | `u3a_page`: 12 (32-bit) vs 11 (64-bit) |
| 102-107 | MACRO-CONDITIONAL | `u3a_minimum`: 4 (32-bit) vs 2 (64-bit) |
| 111-115 | MACRO-CONDITIONAL | `u3a_min_log`: 2 (32-bit) vs 1 (64-bit) |
| 137 | STRUCT-LAYOUT | `u3a_noun` gains `__attribute__((aligned(4)))` |
| 139 | STRUCT-FIELD | `mug_w` → `mug_h` (c3_h) |
| 140-142 | IFDEF-VERE64, STRUCT-FIELD | Conditional `fut_h` field on `u3a_noun` |
| 145,147-150 | STRUCT-LAYOUT, STRUCT-FIELD | Same pattern on `u3a_atom` |
| 155,157-160 | STRUCT-LAYOUT, STRUCT-FIELD | Same pattern on `u3a_cell` |
| 221-228 | IFDEF-VERE64 | `u3a_road.esc` conditionally includes `why_w` for VERE64 |
| 313 | MACRO-PARAMETERIZE | `u3a_is_cat` shifts by `(u3a_word_bits - 1)` |
| 317 | MACRO-PARAMETERIZE | `u3a_is_dog` shifts by `(u3a_word_bits - 1)` |
| 321 | MACRO-PARAMETERIZE | `u3a_is_pug` shifts by `(u3a_word_bits - 2)` |
| 325 | MACRO-PARAMETERIZE | `u3a_is_pom` shifts by `(u3a_word_bits - 2)` |
| 522 | MACRO-PARAMETERIZE | `u3a_to_off` uses `u3a_indirect_mask` |
| 534-545 | MACRO-PARAMETERIZE, COMMENT | `u3a_to_pug`/`u3a_to_pom` use `u3a_indirect_flag`/`u3a_cell_flag` |
| 873 | FUNC-SIGNATURE | `u3a_print_quac` param `den_w` → `den_h` (c3_h) |

### pkg/noun/allocate.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 111,148 | MACRO-PARAMETERIZE | Bit calculations use `(u3a_word_bits-1)` and `u3a_word_bits_log` |
| 191-192 | TYPE-W-TO-H | `_box_count` vars → `c3_ws` |
| 220-221 | FORMAT-SPEC, COMMENT | Format specifiers + XX comment |
| 260-261 | SIZEOF-FIX | `u3a_pile_prep` uses `u3a_word_bytes`/`u3a_word_bytes_shift` |
| 318 | SIZEOF-FIX | `u3a_malloc` uses `u3a_word_bytes`/`u3a_word_bytes_shift` |
| 408-442 | STRUCT-FIELD | `_me_wash_north/south`: `mug_w` → `mug_h` |
| 480 | MACRO-PARAMETERIZE | `_me_gain_use` uses `u3a_direct_max` |
| 521-577 | STRUCT-FIELD, IFDEF-VERE64 | `_ca_take_atom/cell`: `mug_h`, conditional `fut_h` copy and pointer borrowing |
| 617-685 | STRUCT-FIELD, IFDEF-VERE64 | `_ca_take_next_north/south`: conditional noun reconstruction from `mug_h`/`fut_h` |
| 825 | STRUCT-FIELD | `u3a_left`: `mug_h` |
| 1085,1142,1213 | TYPE-W-TO-H | `u3C.wag_w` → `u3C.wag_h` |
| 1126 | FORMAT-SPEC | `u3a_luse` uses `PRIc3_ws`/`PRIxc3_w` |
| 1233,1235 | FUNC-BODY-LOGIC, MACRO-PARAMETERIZE | `u3a_mark_noun`: calls `u3a_mark_ptr`, uses `c3_w_max` |
| 1417-1525 | FORMAT-SPEC | Multiple `PRIc3_w` format specifier changes |
| 1434,1613,1694-1785 | SIZEOF-FIX | `sizeof(c3_w)` instead of hardcoded 4 |
| 1562,1838,1871 | TYPE-W-TO-H | `u3C.wag_w` → `u3C.wag_h` |
| 1663-1679 | FUNC-SIGNATURE | `u3a_print_quac` param `den_h` |

### pkg/noun/types.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 11 | MACRO-PARAMETERIZE | `u3_none` uses `c3_w_max` |
| 14,17 | CAST-FIX | `u3_nul`/`u3_blip` cast to `(c3_w)0` |

### pkg/noun/version.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 17,25 | TYPE-W-TO-H | `u3n_version`/`u3e_version` → `c3_h` |

### pkg/noun/options.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 17 | STRUCT-FIELD | `u3o_config.wag_w` → `wag_h` (c3_h) |

### pkg/noun/imprison.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 17-45 | IFDEF-VERE64 | `_addcarry_w` macro/fallback: VERE64 uses `_addcarry_u64` / `c3_q` |
| 48-51 | MACRO-PARAMETERIZE | `_ci_slab_size`: `(u3a_word_bits - 1)` and `u3a_word_bits_log` |
| 72 | STRUCT-FIELD | `mug_w` → `mug_h` |
| 146 | SIZEOF-FIX | `memset` uses `u3a_word_bytes` |
| 189 | MACRO-PARAMETERIZE | `u3i_slab_from`: `u3a_word_bits_log` |
| 236 | SIZEOF-FIX | `u3i_slab_grow`: `u3a_word_bytes` |
| 339-365 | FUNC-RENAME | `u3i_word()` → `u3i_half()` |
| 371-411 | IFDEF-VERE64 | `u3i_chub()`: split path for 32/64-bit |
| 445-470 | FUNC-RENAME | `u3i_words()` → `u3i_halfs()` |
| 495-524 | IFDEF-VERE64 | `u3i_chubs()`: conditional unpack logic |
| 526-556 | API-WRAPPER | New `u3i_word()`/`u3i_words()` wrappers |
| 569 | MACRO-PARAMETERIZE | `u3a_direct_max` |
| 615,645,765 | STRUCT-FIELD | `mug_h` |
| 754-758 | MACRO-PARAMETERIZE | `u3i_edit`: `u3a_word_bits_log` |

### pkg/noun/imprison.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 79-80 | FUNC-RENAME | `u3i_word` → `u3i_half` |
| 87-90 | FUNC-NEW | New `u3i_word()` declaration |
| 101-102 | FUNC-RENAME | `u3i_words` → `u3i_halfs` |
| 110-115 | FUNC-NEW | New `u3i_words()` declaration |

### pkg/noun/retrieve.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 71-72 | MACRO-PARAMETERIZE | `_frag_deep`: `u3a_word_bits` |
| 303 | STRUCT-FIELD | `mug_w` → `mug_h` |
| 1001-1038 | MACRO-PARAMETERIZE | `u3r_met`/`u3r_bit`: `u3a_word_bits_log`, `u3a_word_bits-1` |
| 1061-1112 | MACRO-PARAMETERIZE | `u3r_byte`/`u3r_bytes`: `u3a_word_bytes`, `u3a_word_bytes_shift` |
| 1181-1186 | MACRO-PARAMETERIZE | `u3r_mp`: `u3a_word_bits_log`, `c3_w_max` |
| 1204-1238 | IFDEF-VERE64 | `u3r_short`: restructured for word shorts |
| 1244-1289 | FUNC-NEW | `u3r_half()` |
| 1294-1320 | IFDEF-VERE64 | `u3r_chub()`: conditional read |
| 1325-1335 | API-WRAPPER | `u3r_word()` wrapper |
| 1340-1380 | FUNC-RENAME, FUNC-NEW, API-WRAPPER | `u3r_half_fit`, `u3r_chub_fit`, `u3r_word_fit` wrapper |
| 1385-1493 | FUNC-RENAME, IFDEF-VERE64, API-WRAPPER | `u3r_halfs`, `u3r_chubs` rewrite, `u3r_words` wrapper |
| 1501-1537 | FUNC-RENAME, API-WRAPPER | `u3r_safe_half`, `u3r_safe_word` wrapper |
| 1553-1694 | MACRO-PARAMETERIZE | `u3r_chop_bits`/`u3r_chop_words`: all `u3a_word_bits`/`u3a_word_bits_log` |
| 1773-1943 | TYPE-L-TO-M, TYPE-W-TO-H, FUNC-RENAME | `u3r_mug_*` family: return types, `u3r_mug_halfs`/`u3r_mug_chubs`, wrapper |
| 1949-2032 | TYPE-W-TO-H | `_cr_mug_next`/`u3r_mug`: `mug_h` |

### pkg/noun/retrieve.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 125-166 | FUNC-SIGNATURE | All `u3r_mug_*` signatures updated |
| 473-568 | FUNC-NEW, FUNC-RENAME, FUNC-SIGNATURE | `u3r_half`, `u3r_chub`, `u3r_half_fit`, `u3r_chub_fit`, `u3r_halfs`, wrappers |

### pkg/noun/serial.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 99 | MACRO-PARAMETERIZE | `UINT32_MAX` → `c3_w_max` |
| 109-131 | IFDEF-VERE64 | `_cs_jam_fib_mat`: conditional word count |
| 243 | MACRO-PARAMETERIZE | `u3a_direct_max` |
| 508-701 | UR-CONDITIONAL | `ur_dict32` → `ur_dictn` throughout |
| 559-565 | MACRO-PARAMETERIZE | `u3a_word_bits-1`, `c3_w_max`, `ur_bsrn_any` |
| 1168,1263 | COMMENT | XX: 64 what do |
| 1420 | SIZEOF-FIX | `sizeof(c3_w)`, `c3_w_max` |

### pkg/noun/events.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 107 | MACRO-PARAMETERIZE | `_ce_len`: `u3a_word_bytes_shift` |
| 151-1030 | FORMAT-SPEC | ~30 format specifier changes to `PRIc3_w` |
| 234-924 | MACRO-PARAMETERIZE | All `>>5`/`&31` → `>>u3a_word_bits_log`/`&(u3a_word_bits-1)` |
| 299 | MACRO-PARAMETERIZE | `u3a_page+u3a_word_bytes_shift` |
| 309 | MACRO-PARAMETERIZE | `UINT32_MAX` → `c3_w_max` |
| 497-553 | TYPE-W-TO-H | `has_w` → `c3_m` for checksums |
| 1011-1494 | TYPE-W-TO-H | `u3C.wag_w` → `u3C.wag_h` (~10 occurrences) |
| 1414,1522,1561 | STRUCT-DYNAMIC | `dit_w` dynamically allocated/freed |
| 1552 | SIZEOF-FIX | `u3e_foul`: computed size |

### pkg/noun/events.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 23 | TYPE-W-TO-H | `has_w` → `c3_m` |
| 51 | STRUCT-DYNAMIC | `dit_w` → `c3_w*` pointer |

### pkg/noun/hashtable.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 11-1407 | TYPE-W-TO-H | Pervasive `c3_w` → `c3_h` for all internal locals, params, and struct fields (map, len, mug, inx, etc.) — ~100+ individual changes |
| 192 | MACRO-PARAMETERIZE | `u3a_half_bits_log` |
| 234 | DEBUG-CODE | `_hbreak()` debug function |

### pkg/noun/hashtable.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 38 | STRUCT-FIELD | `map_w` → `map_h` |
| 48-49 | STRUCT-FIELD | `mug_w`→`mug_h`, `inx_w`→`inx_h` |
| 58 | STRUCT-FIELD | `len_w` → `len_h` |
| 77-96 | IFDEF-VERE64 | Slot macros: conditional bit positions for 32/64-bit |
| 157,162 | FUNC-SIGNATURE | `u3h_trim_to`/`u3h_trim_with`: `c3_h` param |

### pkg/noun/jets.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 27-96 | TYPE-W-TO-H | Many locals/loop vars `c3_w` → `c3_l`/`c3_h` |
| 112-1857 | TYPE-W-TO-H | `u3C.wag_w` → `u3C.wag_h` (~10 occurrences) |
| 280-283 | MACRO-PARAMETERIZE | `u3a_half_bits` |
| 397-2166 | FORMAT-SPEC | Many format specifier changes |
| 802 | FUNC-SIGNATURE | `u3j_boot` return `c3_w` → `c3_l` |
| 838 | DEBUG-CODE | `_jbreak()` debug function |

### pkg/noun/jets.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 99 | STRUCT-FIELD | `len_w` → `len_l` |
| 147 | FUNC-SIGNATURE | `u3j_boot` return `c3_l` |

### pkg/noun/manage.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 56,460-462 | FUNC-SIGNATURE | `u3m_signal`: `c3_l` → `c3_m` |
| 112-115 | IFDEF-VERE64 | `stdatomic.h` include, `_Atomic c3_w u3_Sighow` |
| 149-377 | TYPE-L-TO-M, TYPE-W-TO-H | Signal handling: `sig_l`→`sig_m`, `off_w`→`off_h` |
| 337-1774 | TYPE-W-TO-H | `u3C.wag_w` → `u3C.wag_h` |
| 573 | SIZEOF-FIX | `c3_wiseof * sizeof(c3_w)` |
| 599-752 | FORMAT-SPEC | Many `PRIc3_w` changes |
| 1006-1016 | LONGJMP-CHANGE | VERE64 stores `how` in `u3R->esc.why_w` |
| 1070-1103 | CAST-FIX | `((c3_w)1)` in page alignment |
| 1499-1562 | IFDEF-VERE64, LONGJMP-CHANGE | `u3m_soft_top`/`u3m_soft_run`/`u3m_soft_esc`: setjmp pattern |
| 2600,2633 | MACRO-PARAMETERIZE | `u3a_page + u3a_word_bytes_shift` |
| 2856-2862 | IFDEF-VERE64 | `u3m_time_sec_out`: removes 2106 overflow check |

### pkg/noun/manage.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 50-51 | FUNC-SIGNATURE | `u3m_bail`: `c3_m` → `u3_noun` |
| 96 | FUNC-SIGNATURE | `u3m_signal`: `c3_l` → `c3_m` |

### pkg/noun/nock.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 90,2693 | TYPE-W-TO-H | `u3C.wag_w` → `u3C.wag_h` |
| 468-849 | TYPE-W-TO-H | Multiple `c3_l` → `c3_h` for bytecode offsets |
| 557 | SIZEOF-FIX | `sizeof(c3_h)` for SWIP/SWIN |

### pkg/noun/nock.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 20-46 | STRUCT-FIELD | Multiple `c3_w`/`c3_l` → `c3_h` in `u3n_prog` |

### pkg/noun/palloc.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 55-2574 | CAST-FIX | Pervasive `1U` → `((c3_w)1)` (~60+ occurrences) |
| 55-2574 | MACRO-PARAMETERIZE | `>>5`/`&31` → `>>u3a_word_bits_log`/`&(u3a_word_bits-1)` (~40+ occurrences) |
| 55-2574 | SIZEOF-FIX | `<<2` → `<<(u3a_word_bits_log-3)` for ASAN/memset/memcpy sizes (~30+ occurrences) |
| 55-2574 | FORMAT-SPEC | ~30+ format specifier changes |
| 55-2574 | BIT-MANIP | `(1U<<31)` → `(((c3_w)1)<<(u3a_word_bits-1))`, `~0U` → `~((c3_w)0)` |
| 1160-1797 | TYPE-W-TO-H | `u3C.wag_w` → `u3C.wag_h` |
| 190-220 | OVERFLOW-GUARD | New assertions in `_extend_heap` |

### pkg/noun/xtract.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 15-38 | CAST-FIX | All `u3x_*` axis constants wrapped with `(c3_w)` cast |

### pkg/noun/trace.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 28 | TYPE-WS-TO-HS | `_nock_pid_i` |
| 216-264 | TYPE-W-TO-H | `u3C.wag_w` → `u3C.wag_h`, `old_wag` |
| 226-261 | TYPE-L-TO-M | `mot_l` → `mot_m` |
| 317-488 | FORMAT-SPEC | Multiple `PRIc3_w` changes |
| 566,611 | TYPE-W-TO-H | `u3C.wag_w` → `u3C.wag_h` |
| 645-722 | TYPE-W-TO-H | Slog functions: `pri_l` → `pri_h` |
| 734-977 | TYPE-W-TO-H | Bar graph / report functions: all `c3_w` → `c3_h` |
| 1028 | FUNC-SIGNATURE | `u3t_etch_meme`: `c3_l` → `c3_w` |
| 1140-1240 | TYPE-W-TO-H | Sstack: `off_w`→`off_h`, `fow_w`→`fow_h` |

### pkg/noun/trace.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 37-39 | STRUCT-FIELD | `u3t_spin`: `off_w`→`off_h`, `fow_w`→`fow_h` |
| 43,55 | TYPE-W-TO-H | `u3C.wag_w` → `u3C.wag_h` in macros |
| 166-203 | FUNC-SIGNATURE | All slog/trace declarations: `c3_l`→`c3_h` |

### pkg/noun/urth.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 31-40 | IFDEF-VERE64 | `_cu_atom_to_ref`: skip 2-word case |
| 124-128 | IFDEF-VERE64 | `_cu_from_loom_next`: 64-bit direct atom check |
| 252-311 | UR-CONDITIONAL | `ur_dict32` → `ur_dictn` throughout |
| 281 | MACRO-PARAMETERIZE | `u3a_direct_max` |
| 284-291 | API-CALL-UPDATE | `u3i_words` → `u3i_halfs`, `c3_h wor_h[2]` |
| 421-424 | TYPE-W-TO-H | `u3C.wag_w` → `u3C.wag_h` |

### pkg/noun/vortex.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 128-130 | FUNC-RENAME | `u3v_wish_n` → `u3v_wish_w` |
| 210 | MACRO-PARAMETERIZE | `u3a_indirect_flag` |

### pkg/noun/vortex.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 56-58 | FUNC-RENAME | `u3v_wish_n` → `u3v_wish_w` |

### pkg/noun/ship.c
| Line(s) | Category | Description |
|---------|----------|-------------|
| 89 | TYPE-W-TO-H | `u3_ship_rank` return `c3_l` → `c3_h` |
| 105 | TYPE-W-TO-H | `u3_ship_duke` return `c3_w` → `c3_h` |

### pkg/noun/ship.h
| Line(s) | Category | Description |
|---------|----------|-------------|
| 27,48 | FUNC-SIGNATURE | Return types → `c3_h` |

### Jets — Sentinel changes (`0` → `u3_nul`)

The following ~140 jet files each had their trailing `0` sentinel in `u3r_mean`/`u3x_mean` calls changed to `u3_nul`:

**Group A:** add.c, div.c, gte.c, gth.c, lte.c, lth.c, max.c, min.c, mod.c, mul.c, sub.c
**Group B:** bind.c, clap.c, find.c, levy.c, lien.c, mate.c, murn.c, reap.c, reel.c, roll.c, scag.c, skid.c, skim.c, skip.c, slag.c, snag.c, turn.c, weld.c
**Group C:** aor.c, c0n.c, can.c, cat.c, cut.c, dis.c, dor.c, dvr.c, end.c, gor.c, lsh.c, met.c, mix.c, mor.c, muk.c, peg.c, po.c(×4), pow.c, rap.c, rep.c, rev.c, rsh.c, sew.c, swp.c
**Group D:** All by_*.c (14 files), all in_*.c (11 files)
**Group E:** aes_cbc.c(×6), aes_ecb.c(×6), aes_siv.c(×6), argon2.c, base.c, blake.c(×3), ed_*.c(×18), hmac.c, keccak.c, mice.c, mink.c, parse.c(×20+), rd.c(×10), rh.c(×10), ripe.c, rq.c(×10), rs.c(×10), rub.c, scot.c, scow.c, sha1.c, shax.c, slaw.c
**Group F:** cell.c, comb.c, cons.c, core.c, face.c, fine.c, fitz.c, flan.c, flor.c, fork.c, help.c, hint.c, look.c, loot.c, ut_crop.c, ut_fish.c, ut_fuse.c, ut_mint.c, ut_mull.c, ut_nest.c(×2), ut_rest.c
**Group I:** lagoon.c(×29)

### Jets — Other changes

| File | Line(s) | Category | Description |
|------|---------|----------|-------------|
| jets/a/add.c | 14-62 | IFDEF-VERE64 | `_addcarry_w` with `_addcarry_u64` intrinsic, `c3_q` fallback, pointer cast macro |
| jets/a/add.c | 62 | SIZEOF-FIX | `memcpy` uses `sizeof(c3_w)` |
| jets/a/sub.c | 13-56 | IFDEF-VERE64 | `_subborrow_w` with `_subborrow_u64`, `c3_q` fallback |
| jets/a/sub.c | 56 | SIZEOF-FIX | `memcpy` uses `sizeof(c3_w)` |
| jets/a/sub.c | 95 | MACRO-PARAMETERIZE | `u3a_word_bits_log` in slab init |
| jets/a/mul.c | 13-32 | IFDEF-VERE64 | VERE64 fast-path with `c3_bits_chub` overflow |
| jets/b/find.c | 8-9 | STATIC-ASSERT | Commented out `STATIC_ASSERT` |
| jets/b/find.c | 19-32 | TYPE-W-TO-H | `i_n`/`t_n` → `i_w`/`t_w` |
| jets/b/lent.c | 9-10 | STATIC-ASSERT | Commented out `STATIC_ASSERT` |
| jets/b/sort.c | 8-14 | STATIC-ASSERT | Updated static_assert to use `c3_w_max`/`u3a_cells` |
| jets/d/in_wyt.c | 8-9 | STATIC-ASSERT | Commented out `STATIC_ASSERT` |
| jets/c/bex.c | 15-37 | MACRO-PARAMETERIZE, CAST-FIX | `u3a_word_bits-1`, `((c3_w)1)` |
| jets/c/c0n.c | 14-24 | MACRO-PARAMETERIZE | `u3a_word_bits_log` |
| jets/c/can.c | 13 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/cat.c | 14 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/clz.c | 11-53 | MACRO-PARAMETERIZE | `u3a_word_bits`, `u3a_word_bits_log` |
| jets/c/ctz.c | 21 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/cut.c | 16 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/dis.c | 13-23 | MACRO-PARAMETERIZE | `u3a_word_bits_log` |
| jets/c/end.c | 13 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/ham.c | 11 | MACRO-PARAMETERIZE | `u3a_word_bits_log` |
| jets/c/hew.c | 55 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/lsh.c | 14 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/mas.c | 42 | MACRO-PARAMETERIZE | `u3a_word_bits_log`, `u3a_word_bits-1` |
| jets/c/met.c | 16 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/met.c | 22 | API-CALL-UPDATE | `u3i_words(1,&met_w)` → `u3i_word(met_w)` |
| jets/c/mix.c | 14-24 | MACRO-PARAMETERIZE | `u3a_word_bits_log` |
| jets/c/muk.c | 8-36 | TYPE-W-TO-H, API-CALL-UPDATE | `c3_h`, `u3r_half`/`u3i_halfs` |
| jets/c/rap.c | 13 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/rep.c | 12-115 | MACRO-PARAMETERIZE, FUNC-BODY-LOGIC | Word bits constants + implement general rep |
| jets/c/rev.c | 15 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/rig.c | 21-36 | LITERAL-FIX, MACRO-PARAMETERIZE | `1ULL`, `u3a_word_bits` |
| jets/c/rip.c | 13-201 | MACRO-PARAMETERIZE, FUNC-BODY-LOGIC | Word bits constants + implement general rip |
| jets/c/rsh.c | 14 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/sew.c | 22 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/swp.c | 13 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/c/xeb.c | 15 | API-CALL-UPDATE | `u3i_words(1,&met_w)` → `u3i_word(met_w)` |
| jets/e/argon2.c | 56-130 | TYPE-W-TO-H, API-CALL-UPDATE | `c3_h`, `u3r_half_fit` |
| jets/e/crc.c | 27 | COMMENT | XX: little endian |
| jets/e/fein_ob.c | 11-75 | TYPE-W-TO-H, API-CALL-UPDATE | Comprehensive `c3_h`, `u3r_half`/`u3i_half` |
| jets/e/fl.c | 48-361 | TYPE-WS-TO-HS | `c3_ws` → `c3_hs` for mpz_sgn |
| jets/e/fynd_ob.c | 11-80 | TYPE-W-TO-H, API-CALL-UPDATE | Comprehensive `c3_h`, `u3r_half`/`u3i_half` |
| jets/e/hmac.c | 44-75 | TYPE-W-TO-H, API-CALL-UPDATE, CAST-FIX | `c3_h`, `u3i_halfs`, `(c3_w)50` |
| jets/e/jam.c | 12,26 | FORMAT-SPEC | Enable debug, `PRIc3_w` |
| jets/e/leer.c | 18 | COMMENT | XX: 64 what? |
| jets/e/loss.c | 99,113 | API-CALL-UPDATE | `u3i_word()` |
| jets/e/parse.c | many | SENTINEL | 20+ sentinel changes |
| jets/e/rd.c | 32 | TYPE-W-TO-H | `_set_rounding` param → `c3_y` |
| jets/e/rh.c | 33-372 | TYPE-W-TO-H, API-CALL-UPDATE | `c3_y`, `u3r_half` throughout |
| jets/e/rq.c | 16-425 | IFDEF-VERE64, MACRO-PARAMETERIZE | Word count `n` conditional for 128-bit floats |
| jets/e/rs.c | 13-372 | TYPE-W-TO-H, API-CALL-UPDATE | `c3_h` for float32 union, `u3r_half`/`u3i_halfs` |
| jets/e/rub.c | 20 | API-CALL-UPDATE | `u3i_word()` |
| jets/e/scow.c | 11-228 | DELETED-CODE | Major simplification removing ~220 lines |
| jets/e/scr.c | 52 | TYPE-W-TO-H, API-CALL-UPDATE | `c3_h`, `u3r_half` |
| jets/e/secp.c | 40-73 | IFDEF-VERE64 | 64-bit word layout of secp256k1 constant |
| jets/e/urwasm.c | 204-2715 | TYPE-W-TO-H | Pervasive wasm32 narrowing (~100+ changes) |
| jets/g/plot.c | 85,208 | MACRO-PARAMETERIZE | `u3a_word_bits` |
| jets/g/plot.c | 327 | API-CALL-UPDATE | `u3i_half` |
| jets/i/lagoon.c | 22-746 | TYPE-W-TO-H, API-CALL-UPDATE | Float types, `u3i_half` |
| jets/i/lagoon.c | 583,656 | TYPE-W-TO-D | `min_idx`/`max_idx` → `c3_d` |

### pkg/vere/ runtime files

| File | Line(s) | Category | Description |
|------|---------|----------|-------------|
| vere.h | many | STRUCT-FIELD, FUNC-SIGNATURE | ~50 struct field and function signature changes (TYPE-W-TO-H, TYPE-L-TO-H) |
| auto.c | many | TYPE-W-TO-H, TYPE-L-TO-W, FORMAT-SPEC | `dep_h`, `try_h`, `cod_w`, format specifiers |
| dawn.c | ~2 lines | TYPE-W-TO-H, API-CALL-UPDATE | `c3_h eny_w[16]`, `u3i_halfs` |
| disk.c | many | TYPE-W-TO-H, TYPE-L-TO-H, TYPE-W-TO-D | `ver_h`, `mug_h`, `len_d`, `lif_h` |
| foil.c | few | COMMENT, DELETED-CODE | Comment update, removed `_foil_path()` |
| king.c | many | TYPE-W-TO-H | `sag_h`, `u3C.wag_h`, path lengths |
| lord.c | many | TYPE-W-TO-H, API-CALL-UPDATE, FORMAT-SPEC | `dep_h`, `wag_h`, `u3r_safe_half`, `u3i_half` |
| main.c | many | TYPE-W-TO-H, FORMAT-SPEC, FUNC-NEW, DEFAULT-CONFIG | `wag_h`, `_main_readn()`, `lut_y=34`, `lom_y=32` |
| mars.c | many | TYPE-W-TO-H, TYPE-L-TO-H, SIZEOF-FIX, API-CALL-UPDATE | `mug_h`, `sizeof(c3_w)`, `u3r_safe_half` |
| mars.h | 3 | STRUCT-FIELD | `mug_h`, `mas_h`, `fag_h` |
| mdns.h | 1 | FUNC-SIGNATURE | mDNS callback `c3_w` → `c3_h` |
| melt.c | few | STRUCT-FIELD, SIZEOF-FIX | `mug_h`, `u3a_word_bits_log-3` |
| newt.c | few | TYPE-W-TO-H, API-CALL-UPDATE | `c3_h`, `u3i_half` |
| pier.c | many | TYPE-W-TO-H, TYPE-L-TO-H, FUNC-SIGNATURE | `dep_h`, `wag_h`, `pri_h`, `tab_h` |
| save.c | all | FUNC-NEW | New autosave timer file |
| time.c | all | FUNC-NEW | New time conversion file |
| ward.c | many | TYPE-W-TO-H, FUNC-SIGNATURE, OVERFLOW-GUARD | `mil_h`, `u3_mcut_*` narrowed |
| io/ames.c | many | TYPE-W-TO-H, TYPE-L-TO-M, NETWORK-PROTOCOL, API-CALL-UPDATE | `pip_h`, `sev_m`, `c3_sift_half`, `u3i_half` |
| io/ames/stun.c | many | TYPE-W-TO-H, NETWORK-PROTOCOL | `c3_sift_half`, all locals narrowed |
| io/ames/stun.h | 3 | FUNC-SIGNATURE | All signatures `c3_h` |
| io/behn.c | few | TYPE-L-TO-M, TYPE-W-TO-H | `sev_m`, `try_h` |
| io/conn.c | many | TYPE-L-TO-H, API-CALL-UPDATE, FORMAT-SPEC | `coq_h`, `nex_h`, `u3i_half` |
| io/cttp.c | many | TYPE-L-TO-W, TYPE-L-TO-H, API-CALL-UPDATE, FORMAT-SPEC | `num_l`→`c3_w`, `sev_h`, `u3r_half` |
| io/fore.c | few | TYPE-W-TO-H, LITERAL-FIX, API-CALL-UPDATE | `c3_h eny_w`, `ULL`, `u3i_halfs` |
| io/http.c | many | TYPE-L-TO-W, TYPE-W-TO-H, CONSTANT-CHANGE, FORMAT-SPEC | `sev_l`→`c3_w`, `c3_w_max`, `ULL` |
| io/lick.c | few | TYPE-L-TO-H, TYPE-W-TO-H | `coq_h`, `nex_h`, `len_h` |
| io/lss.c | many | TYPE-W-TO-H, TYPE-W-TO-D | All LSS functions narrowed; `lss_proof_size` widened |
| io/lss.h | many | TYPE-W-TO-H, TYPE-W-TO-D, FUNC-SIGNATURE | Struct fields and function signatures |
| io/mesa.c | many | TYPE-W-TO-H | Pervasive narrowing (~100+ changes) |
| io/mesa/bitset.c | many | TYPE-W-TO-H, OVERFLOW-GUARD | All narrowed, init widened with assert |
| io/mesa/bitset.h | few | STRUCT-FIELD, FUNC-SIGNATURE | `len_h`, all signatures |
| io/mesa/mesa.h | many | STRUCT-FIELD | Protocol struct fields narrowed |
| io/mesa/pact.c | many | TYPE-W-TO-H, NETWORK-PROTOCOL | `c3_sift_half`/`c3_etch_half` |
| io/term.c | many | TYPE-W-TO-H, TYPE-L-TO-H, NEWLINE-FIX, API-CALL-UPDATE | Terminal completely narrowed, `\r\n` fixes |
| io/unix.c | many | TYPE-W-TO-H, TYPE-L-TO-H, FORMAT-SPEC | File/mount operations narrowed |
| platform/darwin/ptty.c | few | TYPE-W-TO-H, FUNC-SIGNATURE | `c3_h`, window size |
| platform/linux/ptty.c | few | TYPE-W-TO-H, FUNC-SIGNATURE | `c3_h`, window size |
| platform/windows/ptty.c | few | FUNC-SIGNATURE | Window size |
| db/lmdb.c | many | TYPE-W-TO-H | All `ret_w`/`ops_w` → `ret_h`/`ops_h` |

### Test files

| File | Line(s) | Category | Description |
|------|---------|----------|-------------|
| hashtable_tests.c | few | TYPE-W-TO-H, FORMAT-SPEC | `c3_h`, `PRIc3_w` |
| jets_tests.c | few | FORMAT-SPEC | `PRIxc3_w` for fein/fynd/mas tests |
| nock_tests.c | 31 | TYPE-W-TO-H | Loop counter `c3_h` |
| palloc_tests.c | many | STRUCT-FIELD, TEST-NEW, TEST-IFDEF, FORMAT-SPEC | `rut_p`, new `_test_palloc`/`_test_palloc_64` |
| retrieve_tests.c | many | TEST-NEW, TEST-IFDEF, API-CALL-UPDATE | 7 new test functions, `u3r_halfs`/`u3i_halfs` |
| serial_tests.c | many | TEST-UPDATE | 13 jam/cue test cases commented out |
| ames_tests.c | few | STRUCT-FIELD | `pip_h`, `len_h` |
| benchmarks.c | many | FORMAT-SPEC, TYPE-W-TO-D, MACRO-PARAMETERIZE, SIZEOF-FIX | `PRIc3_w`, `c3_d` for cue lengths |
| boot_tests.c | few | TYPE-W-TO-H, MACRO-PARAMETERIZE, TEST-UPDATE | `wag_h`, `u3a_direct_max` |
| hamt_test.c | few | TYPE-W-TO-H | `c3_h`, `wag_h` |
| newt_tests.c | many | TYPE-W-TO-D, TEST-NEW | `c3_d` lengths, 5 new test functions |
| noun_tests.c | many | TEST-NEW, TEST-IFDEF, MACRO-PARAMETERIZE, API-CALL-UPDATE, SENTINEL, FORMAT-SPEC | New imprison/word tests, boundary tests |

### Migration/past files

| File | Line(s) | Category | Description |
|------|---------|----------|-------------|
| past/build.zig | few | BUILD-SYSTEM | Added v5.c, commented v6 files |
| past/migrate.h | 2 | BUILD-SYSTEM | v6 declaration commented out |
| past/migrate_v2.c | 1 | MIGRATION-TYPE-PIN | `c3_align` → `c3_v2_align` |
| past/migrate_v3.c | few | MIGRATION-TYPE-PIN | `c3_w*`→`c3_v3_w*`, `c3_wiseof`→`c3_v3_wiseof` |
| past/migrate_v5.c | many | MIGRATION-TYPE-PIN | All types versioned to v4/v5 |
| past/migrate_v6.c | all | MIGRATION-NEW | New v5→v6 migration |
| past/v1.c | many | MIGRATION-TYPE-PIN | `c3_w`→`c3_v1_w`, `u3p`→`u3v1p`, `u3_noun`→`u3_v1_noun` |
| past/v1.h | 6-20 | MIGRATION-TYPE-PIN | Versioned type alias definitions |
| past/v2.c | few | MIGRATION-TYPE-PIN | `c3_w`→`c3_v2_w`, etc. |
| past/v2.h | many | MIGRATION-TYPE-PIN | Versioned types + struct retyping |
| past/v3.c | few | MIGRATION-TYPE-PIN | `c3_w`→`c3_v3_w`, etc. |
| past/v3.h | 6-20 | MIGRATION-TYPE-PIN | Versioned type aliases |
| past/v4.c | many | MIGRATION-TYPE-PIN | Comprehensive versioning |
| past/v4.h | many | MIGRATION-TYPE-PIN | Complete rewrite with versioned types |
| past/v5.c | all | MIGRATION-NEW | New v5 allocator snapshot frozen at 32-bit |
| past/v5.h | all | MIGRATION-NEW | Self-contained v5 type definitions |
| past/v6.h | all | MIGRATION-NEW | New v6 header aliasing current code |

### ur/ package

| File | Line(s) | Category | Description |
|------|---------|----------|-------------|
| ur/bitstream.h | 239-248 | UR-CONDITIONAL | `ur_bsrn_any`/`ur_bswn` macros |
| ur/defs.h | 27-31 | CONSTANT-NEW | `ur_direct_max` |
| ur/hashcons.h | 68-73 | UR-CONDITIONAL | `ur_dictn_t` typedef |
| ur/hashcons.h | 159-171 | UR-CONDITIONAL | `ur_dictn_grow`/`get`/`put`/`wipe` macros |
| ur/serial.c | many | UR-CONDITIONAL | `ur_serial_size_t` throughout |
| ur/serial.h | 23-27 | UR-CONDITIONAL | `ur_serial_size_t` typedef |
| ur/serial.h | 46-106 | UR-CONDITIONAL | Declaration updates |

### Build system and CI

| File | Line(s) | Category | Description |
|------|---------|----------|-------------|
| build.zig | 110 | BUILD-SYSTEM | `vere32` field added |
| build.zig | 197-201 | BUILD-SYSTEM | `-Dvere32` build option |
| build.zig | 243 | BUILD-SYSTEM | `vere32` propagated |
| build.zig | 368-369 | BUILD-SYSTEM | `-DVERE64` define emitted when `!cfg.vere32` |
| build.zig | 448-583 | BUILD-SYSTEM | Past dependency commented out |
| pkg/noun/build.zig | 173 | BUILD-SYSTEM | `-Wconversion` commented out |
| .github/workflows/shared.yml | 40-41 | CI-WORKFLOW | Build matrix with bits: [64, 32] |
| .github/workflows/shared.yml | 77-81 | CI-WORKFLOW | Memory overcommit step |
| .github/workflows/shared.yml | 89-219 | CI-WORKFLOW | `-Dvere32` conditional, binary prefix, upload targets |

---

## Summary

| Category | Approx. Count | Notes |
|----------|--------------|-------|
| SENTINEL | ~180+ call sites | Largest single category — `0` → `u3_nul` in variadic calls |
| TYPE-W-TO-H | ~300+ individual changes | Pervasive narrowing across all packages |
| FORMAT-SPEC | ~120+ | `%u`/`%d`/`%x` → `PRIc3_w`/`PRIc3_h` |
| MACRO-PARAMETERIZE | ~80+ | `31`/`32`/`5` → `u3a_word_bits-1`/`u3a_word_bits`/`u3a_word_bits_log` |
| SIZEOF-FIX | ~40+ | `*4`/`<<2` → `sizeof(c3_w)`/`u3a_word_bytes_shift` |
| CAST-FIX | ~70+ | `1U` → `((c3_w)1)`, axis casts |
| IFDEF-VERE64 | ~30+ blocks | Conditional compilation for dual-width |
| API-RENAME / API-WRAPPER | ~15 functions | `u3i_word`/`u3r_word`/etc. split into half+wrapper |
| API-CALL-UPDATE | ~60+ call sites | Callers updated to new APIs |
| MIGRATION-TYPE-PIN | ~200+ | Versioned types in past/ |
| TEST-NEW | ~15 new test functions | Boundary-value tests for 32/64 |
| BUILD-SYSTEM / CI | ~20 | Build matrix, flag propagation |
