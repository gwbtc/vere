# Nock Bytecode Interpreter Overview

The Nock interpreter compiles Nock formulas into bytecode programs for efficient execution. This document covers the compilation process, bytecode format, and interpreter operation.

## Table of Contents

1. [Architecture](#architecture)
2. [Bytecode Format](#bytecode-format)
3. [Compilation](#compilation)
4. [Interpreter](#interpreter)
5. [Hints](#hints)
6. [Memory Management](#memory-management)
7. [Internal Functions Reference](#internal-functions-reference)

---

## Architecture

### Execution Pipeline

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ Nock        │     │ Bytecode    │     │ Bytecode    │     │ Result      │
│ Formula     │────►│ Compiler    │────►│ Interpreter │────►│             │
│             │     │ (_n_comp)   │     │ (_n_burn)   │     │             │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
      │                   │                   │
      │                   ▼                   ▼
      │           ┌─────────────┐     ┌─────────────┐
      │           │ u3n_prog    │     │ Jets        │
      │           │ (cached)    │     │ (u3j_*)     │
      │           └─────────────┘     └─────────────┘
      │                   ▲
      │                   │
      └───────────────────┘
            Cache Lookup
            (_n_find)
```

### Key Components

| Component | Location | Purpose |
|-----------|----------|---------|
| Compiler | `_n_comp()` | Compiles Nock to bytecode |
| Assembler | `_n_prog_from_ops()` | Converts op list to u3n_prog |
| Interpreter | `_n_burn()` | Executes bytecode |
| Cache | `u3R->byc.har_p` | Caches compiled programs |

---

## Bytecode Format

### Opcode Categories

The bytecode uses ~95 opcodes organized into categories:

#### Stack Operations

| Opcode | Description | Stack Effect |
|--------|-------------|--------------|
| `HALT` | End program | - |
| `BAIL` | Raise exit | - |
| `COPY` | Duplicate top | [a] → [a a] |
| `SWAP` | Swap top two | [a b] → [b a] |
| `TOSS` | Discard top | [a] → [] |

#### Autocons (Nock [a b])

| Opcode | Description | Stack Effect |
|--------|-------------|--------------|
| `AUTO` | Build cell | [bus hed tel] → [bus [hed tel]] |
| `AULT` | Build cell (lose) | [hed tel] → [[hed tel]] |
| `SNOC` | Cons | [hed tel] → [[hed tel] tel] |
| `SNOL` | Cons (lose) | [hed tel] → [[hed tel]] |

#### Nock 0 (Fragment)

| Opcode | Arg Size | Description |
|--------|----------|-------------|
| `HEAD` | 0 | Get head (axis 2) |
| `HELD` | 0 | Get head (lose subject) |
| `TAIL` | 0 | Get tail (axis 3) |
| `TALL` | 0 | Get tail (lose subject) |
| `FABK` | 1 byte | Fragment, keep subject |
| `FASK` | 2 bytes | Fragment, keep subject |
| `FIBK` | 1 byte | Fragment (indexed), keep |
| `FISK` | 2 bytes | Fragment (indexed), keep |
| `FABL` | 1 byte | Fragment, lose subject |
| `FASL` | 2 bytes | Fragment, lose subject |
| `FIBL` | 1 byte | Fragment (indexed), lose |
| `FISL` | 2 bytes | Fragment (indexed), lose |

#### Nock 1 (Constant)

| Opcode | Arg Size | Description |
|--------|----------|-------------|
| `LIT0` | 0 | Push 0 |
| `LIT1` | 0 | Push 1 |
| `LITB` | 1 byte | Push byte literal |
| `LITS` | 2 bytes | Push short literal |
| `LIBK` | 1 byte | Push indexed literal, keep |
| `LISK` | 2 bytes | Push indexed literal, keep |
| `LIL0` | 0 | Push 0, lose subject |
| `LIL1` | 0 | Push 1, lose subject |
| `LILB` | 1 byte | Push byte, lose |
| `LILS` | 2 bytes | Push short, lose |
| `LIBL` | 1 byte | Push indexed, lose |
| `LISL` | 2 bytes | Push indexed, lose |

#### Nock 2 (Eval)

| Opcode | Description |
|--------|-------------|
| `NOCK` | Eval: [fol old bus] → [result old] |
| `NOLK` | Eval: [fol bus] → [result] |
| `NOCT` | Tail-call: [fol bus] → jump |

#### Nock 3/4/5 (Operators)

| Opcode | Description | Stack Effect |
|--------|-------------|--------------|
| `DEEP` | Cell test | [x] → [?.x] |
| `BUMP` | Increment | [x] → [+(x)] |
| `SAM0` | Equals 0 | [x] → [=(x 0)] |
| `SAM1` | Equals 1 | [x] → [=(x 1)] |
| `SAMB` | Equals byte | [x] → [=(x b)] |
| `SAMS` | Equals short | [x] → [=(x s)] |
| `SANB` | Equals indexed | [x] → [=(x lit[i])] |
| `SANS` | Equals indexed | [x] → [=(x lit[i])] |
| `SAME` | Equals | [a b bus] → [=(a b) bus] |
| `SALM` | Equals (lose) | [a b] → [=(a b)] |

#### Nock 6 (If)

| Opcode | Arg | Description |
|--------|-----|-------------|
| `SBIP` | distance | Unconditional skip |
| `SIPS` | distance | Unconditional skip (short) |
| `SWIP` | distance | Unconditional skip (word) |
| `SBIN` | distance | Skip if no (c3n) |
| `SINS` | distance | Skip if no (short) |
| `SWIN` | distance | Skip if no (word) |

#### Nock 9 (Kick)

| Opcode | Arg | Description |
|--------|-----|-------------|
| `KICB` | site index | Kick (byte index) |
| `KICS` | site index | Kick (short index) |
| `TICB` | site index | Tail kick (byte) |
| `TICS` | site index | Tail kick (short) |

#### Nock 10 (Edit)

| Opcode | Description |
|--------|-------------|
| `KUTH` | Edit head, keep |
| `KUTT` | Edit tail, keep |
| `KUSM` | Edit sample, keep |
| `KUTB/S` | Edit at axis, keep |
| `MUTH` | Edit head, lose |
| `MUTT` | Edit tail, lose |
| `MUSM` | Edit sample, lose |
| `MUTB/S` | Edit at axis, lose |
| `KITB/S` | Edit indexed axis, keep |
| `MITB/S` | Edit indexed axis, lose |

#### Nock 11 (Hints)

| Opcode | Description |
|--------|-------------|
| `HILB/S` | Static hint fore |
| `HINB/S` | Dynamic hint fore |
| `HILK/L` | Static hint hind |
| `HINK/L` | Dynamic hint hind |

#### Nock 12 (Scry)

| Opcode | Description |
|--------|-------------|
| `WISH` | Scry, keep subject |
| `WILS` | Scry, lose subject |

#### Special

| Opcode | Description |
|--------|-------------|
| `BUSH/SUSH` | Push trace frame |
| `DROP` | Pop trace frame |
| `HECK` | Check live |
| `SLOG` | Side-effect log |
| `BAST/SALT` | Register core (%fast) |
| `SKIB/SLIB` | Memo lookup |
| `SAVE` | Memo save |

### Opcode Encoding

```
Byte 0: Opcode (0-94)

For opcodes with arguments:
  1-byte arg:  [opcode] [arg]
  2-byte arg:  [opcode] [lo] [hi]
  4-byte arg:  [opcode] [b0] [b1] [b2] [b3]
```

---

## Compilation

### `_n_comp()` Function

Recursively compiles a Nock formula to bytecode operations.

**Signature:**
```c
static c3_w
_n_comp(u3_noun* ops, u3_noun fol, c3_o los_o, c3_o tel_o)
```

**Parameters:**
- `ops`: Pointer to operation list (reversed)
- `fol`: Formula to compile (RETAIN)
- `los_o`: "Lose" flag - should we drop the subject?
- `tel_o`: "Tail" flag - is this tail position?

**Return:** Number of operations emitted

### Compilation by Nock Operator

#### Nock 0 (Fragment)
```
[0 axis] compiles to:
  axis=0: BAIL
  axis=1: COPY or nothing (if los_o)
  axis=2: HEAD or HELD
  axis=3: TAIL or TALL
  else:   FABx or FABx (with axis argument)
```

#### Nock 1 (Constant)
```
[1 constant] compiles to:
  constant=0: LIT0 or LIL0
  constant=1: LIT1 or LIL1
  else:       LITx or LILx (with constant or index)
```

#### Nock 2 (Eval)
```
[2 subject-formula formula-formula] compiles to:
  compile(subject-formula)  →  [bus hed]
  SWAP                      →  [hed bus]
  compile(formula-formula)  →  [hed bus fol]
  NOCK/NOLK/NOCT           →  result
```

#### Nock 6 (If)
```
[6 test yes no] compiles to:
  compile(test)             →  [bus loobean]
  SBIN yep_w                   if no, skip yes branch
  compile(yes)                 yes branch
  SBIP nop_w                   skip no branch
  compile(no)                  no branch
```

#### Nock 9 (Kick)
```
[9 axis formula] compiles to:
  compile(formula)          →  [bus cor]
  KICx or TICx (axis)          kick with site index
```

### Assembly: `_n_prog_from_ops()`

Converts the reversed operation list into a `u3n_prog`:

1. **Count**: Calculate sizes for bytecode, literals, memos, sites, rites
2. **Allocate**: Create u3n_prog with embedded arrays
3. **Emit**: Write bytecode, collecting literals/sites/rites
4. **Reverse**: Bytecode was built reversed, flip it

```
Operation list (reversed):
  [COPY, LIT0, ADD, HALT]

After assembly:
  u3n_prog:
    byc_u.ops_y = [HALT, ADD, LIT0, COPY, 0]
    lit_u.non   = []
    cal_u.sit_u = []
    ...
```

---

## Interpreter

### `_n_burn()` Function

The main interpreter loop using computed goto for dispatch.

**Signature:**
```c
static u3_noun
_n_burn(u3n_prog* pog_u, u3_noun bus, c3_ys mov, c3_ys off)
```

**Parameters:**
- `pog_u`: Program to execute
- `bus`: Subject (TRANSFER)
- `mov`: Stack direction (-1 north, 1 south)
- `off`: Stack offset (0 north, -1 south)

### Stack Machine

The interpreter uses the cap stack as its operand stack:

```
North road (grows down):   South road (grows up):
  u3R->cap_p                 u3R->cap_p
      │                          │
      ▼                          │
  [top]                      [bottom]
  [next]                     [...]
  [...]                      [next]
  [bottom]                   [top]
      │                          │
      │                          ▼
```

**Stack Helpers:**
```c
_n_push(mov, off, noun)  // Push noun onto stack
_n_pop(mov)              // Remove top
_n_pep(mov, off)         // Pop and return top
_n_peek(off)             // Get pointer to top
_n_peet(mov, off)        // Get pointer to second
_n_swap(mov, off)        // Swap top two
_n_toss(mov, off)        // Pop and free top
```

### Call Stack (Burn Frames)

For non-tail calls, the interpreter pushes burn frames:

```c
typedef struct __attribute__((__packed__)) {
  u3n_prog* pog_u;  // Saved program
  c3_w     ip_w;    // Saved instruction pointer
} burnframe;
```

**Call sequence:**
```
NOCK opcode:
  1. Pop formula from operand stack
  2. Push burnframe (current pog_u, ip_w)
  3. Look up/compile new program
  4. Set pog_u, ip_w = 0
  5. Continue execution

HALT opcode:
  1. Pop result from operand stack
  2. If burn frames remain:
     - Restore pog_u, ip_w from frame
     - Push result back
     - Continue
  3. Else: return result
```

### Dispatch Loop

```c
#define BURN() goto *lab[pog[ip_w++]]

static void* lab[] = {
  [HALT] = &&do_halt,
  [BAIL] = &&do_bail,
  [COPY] = &&do_copy,
  // ... 90+ opcodes
};

// Main loop:
BURN();  // Jump to first opcode handler
{
  do_halt:
    x = _n_pep(mov, off);
    if (empty == u3R->cap_p) return x;
    // else restore frame and continue
    BURN();

  do_copy:
    top = _n_peek(off);
    _n_push(mov, off, u3k(*top));
    BURN();

  // ... handlers for all opcodes
}
```

---

## Hints

### Hint Processing

Hints (Nock 11) are processed in two phases:

1. **Fore**: Before evaluating the hinted formula
2. **Hind**: After evaluating, with the result

### Static Hints (`_n_hilt_fore/hind`)

For hints with atomic clues (no formula):

| Hint | Fore Action | Hind Action |
|------|-------------|-------------|
| `%cash` | Count cax entries | Report delta |
| `%loop` | Check/set loop flag | Clear loop flag |
| `%bout` | Record start time | Report elapsed |
| `%nara` | Log stack trace | - |
| `%hela` | Log full trace | - |
| `%xray` | Log bytecode | - |
| `%meme` | Log memory | - |

### Dynamic Hints (`_n_hint_fore/hind`)

For hints with formula clues:

| Hint | Fore Action | Hind Action |
|------|-------------|-------------|
| `%jinx` | Set timer | Pop timer |
| `%spin` | Push to sstack | Pop from sstack |
| `%bout` | Record [pri tank time] | Log timing report |
| `%loop` | Check/set loop flag | Clear flag |
| `%nara` | Log trace with label | - |
| `%hela` | Log trace with label | - |
| `%xray` | Log bytecode | - |
| `%meme` | Log memory | - |

### Special Hints

#### `%fast` (Registration)

Processed via `BAST/SALT` opcodes:
```
1. Evaluate clue formula
2. Call u3j_rite_mine(rit_u, clu, cor)
3. Continue with result
```

#### `%memo` (Memoization)

Processed via `SKIB/SLIB` and `SAVE` opcodes:
```
SKIB/SLIB:
  1. Build cache key from subject + formula
  2. Look up in u3z cache
  3. If hit: skip to after SAVE, push result
  4. If miss: push [cid, key], continue

SAVE:
  1. Pop result
  2. Pop [cid, key]
  3. Store result in cache
  4. Push result
```

---

## Memory Management

### Program Allocation

Programs are allocated with `u3a_malloc`, not on the loom:

```c
u3n_prog* _n_prog_new(len_w, cal_w, reg_w, lit_w, mem_w) {
  // Calculate total size
  c3_w siz_w = sizeof(u3n_prog)
             + len_w + pad_w        // bytecode + padding
             + lit_w * sizeof(u3_noun)
             + mem_w * sizeof(u3n_memo)
             + cal_w * sizeof(u3j_site)
             + reg_w * sizeof(u3j_rite);

  return u3a_malloc(siz_w);
}
```

### Memory Layout

```
u3n_prog structure:
┌─────────────────────────┐
│ byc_u (own_o, len_w,    │
│        ops_y pointer)   │
│ lit_u (len_w, non ptr)  │
│ mem_u (len_w, sot_u ptr)│
│ cal_u (len_w, sit_u ptr)│
│ reg_u (len_w, rit_u ptr)│
├─────────────────────────┤ ← _n_prog_dat()
│ bytecode bytes          │
│ ... (padding)           │
├─────────────────────────┤
│ literals array          │
│ (u3_noun pointers)      │
├─────────────────────────┤
│ memo slots              │
│ (u3n_memo structs)      │
├─────────────────────────┤
│ call sites              │
│ (u3j_site structs)      │
├─────────────────────────┤
│ registration sites      │
│ (u3j_rite structs)      │
└─────────────────────────┘
```

### Cache Management

```c
// Find or compile program
u3n_prog* _n_find(u3_noun pre, u3_noun fol) {
  u3_noun key = u3nc(u3k(pre), u3k(fol));

  // Check current road
  u3_weak pog = u3h_git(u3R->byc.har_p, key);
  if (u3_none != pog) {
    return _cn_to_prog(pog);
  }

  // Check parent roads
  u3a_road* rod_u = u3R;
  while (rod_u->par_p) {
    rod_u = u3to(u3a_road, rod_u->par_p);
    pog = u3h_git(rod_u->byc.har_p, key);
    if (u3_none != pog) {
      // Copy to current road
      u3n_prog* old = _n_prog_old(_cn_to_prog(pog));
      u3h_put(u3R->byc.har_p, key, _cn_of_prog(old));
      return old;
    }
  }

  // Compile fresh
  u3n_prog* gop = _n_bite(fol);
  u3h_put(u3R->byc.har_p, key, _cn_of_prog(gop));
  return gop;
}
```

### GC Marking

```c
u3n_mark() {
  // Walk cache, mark each program
  u3h_walk_with(har_p, _n_bam, &tot_w);
}

_n_prog_mark(pog_u) {
  // Mark program struct
  tot_w = u3a_mark_mptr(pog_u);

  // Mark literals
  for (i = 0; i < pog_u->lit_u.len_w; i++) {
    tot_w += u3a_mark_noun(pog_u->lit_u.non[i]);
  }

  // Mark memo keys
  for (i = 0; i < pog_u->mem_u.len_w; i++) {
    tot_w += u3a_mark_noun(pog_u->mem_u.sot_u[i].key);
  }

  // Mark call sites
  for (i = 0; i < pog_u->cal_u.len_w; i++) {
    tot_w += u3j_site_mark(&pog_u->cal_u.sit_u[i]);
  }

  // Mark registration sites
  for (i = 0; i < pog_u->reg_u.len_w; i++) {
    tot_w += u3j_rite_mark(&pog_u->reg_u.rit_u[i]);
  }

  return tot_w;
}
```

---

## Internal Functions Reference

### Compilation

| Function | Purpose |
|----------|---------|
| `_n_comp(ops, fol, los_o, tel_o)` | Compile formula to op list |
| `_n_bint(ops, hif, nef, los_o, tel_o)` | Compile hint |
| `_n_emit(ops, op)` | Add operation to list |
| `_n_apen(dst, src)` | Append operation lists |
| `_n_bite(fol)` | Compile formula to program |
| `_n_prog_from_ops(ops)` | Assemble ops to program |
| `_n_formulaic(fol)` | Check if formula is valid |

### Assembly

| Function | Purpose |
|----------|---------|
| `_n_prog_new(...)` | Allocate program structure |
| `_n_prog_old(pog_u)` | Copy program (share bytecode) |
| `_n_prog_dat(pog_u)` | Get data section pointer |
| `_cn_of_prog(pog_u)` | Convert pointer to cache value |
| `_cn_to_prog(pog_w)` | Convert cache value to pointer |

### Execution

| Function | Purpose |
|----------|---------|
| `_n_burn(pog_u, bus, mov, off)` | Main interpreter |
| `_n_burn_out(bus, pog_u)` | Execute (determines direction) |
| `_n_burn_on(bus, fol)` | Compile and execute |
| `_n_kick(cor, sit_u)` | Kick via jet site |
| `_n_kale(a)` | Bail if not cell |

### Stack Operations

| Function | Purpose |
|----------|---------|
| `_n_push(mov, off, a)` | Push noun |
| `_n_pop(mov)` | Remove top |
| `_n_pep(mov, off)` | Pop and return |
| `_n_peek(off)` | Pointer to top |
| `_n_peet(mov, off)` | Pointer to second |
| `_n_swap(mov, off)` | Swap top two |
| `_n_toss(mov, off)` | Pop and free |

### Hints

| Function | Purpose |
|----------|---------|
| `_n_hilt_fore(hin, bus, out)` | Static hint before eval |
| `_n_hilt_hind(tok, pro)` | Static hint after eval |
| `_n_hint_fore(hin, bus, clu)` | Dynamic hint before eval |
| `_n_hint_hind(tok, pro)` | Dynamic hint after eval |

### Memory

| Function | Purpose |
|----------|---------|
| `_n_find(pre, fol)` | Find/compile program |
| `_cn_prog_free(pog_u)` | Free program and contents |
| `_n_prog_mark(pog_u)` | Mark for GC |
| `_cn_take_prog_dat(dst, src)` | Take references |
| `_cn_merge_prog_dat(dst, src)` | Merge references |
