# Jet and Nock Architecture

This document provides a unified view of how the Nock bytecode interpreter and jet acceleration system work together to execute Urbit code efficiently.

## Table of Contents

1. [System Overview](#system-overview)
2. [Execution Model](#execution-model)
3. [Integration Points](#integration-points)
4. [State Management](#state-management)
5. [Performance Optimizations](#performance-optimizations)
6. [Lifecycle](#lifecycle)
7. [Debugging and Tracing](#debugging-and-tracing)

---

## System Overview

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Hoon / Nock Code                               │
└───────────────────────────────────────┬─────────────────────────────────────┘
                                        │
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                            u3n_nock_on(bus, fol)                            │
│                              Main Entry Point                               │
└───────────────────────────────────────┬─────────────────────────────────────┘
                                        │
                    ┌───────────────────┴───────────────────┐
                    │                                       │
                    ▼                                       ▼
┌─────────────────────────────────┐       ┌─────────────────────────────────┐
│       BYTECODE COMPILER         │       │          PROGRAM CACHE          │
│         _n_comp()               │       │        u3R->byc.har_p           │
│                                 │       │                                 │
│  • Recursive descent            │◄─────►│  • Key: [prefix formula]        │
│  • Tail-call optimization       │       │  • Value: u3n_prog pointer      │
│  • Hint processing              │       │  • Per-road caching             │
│  • Site/rite allocation         │       │                                 │
└─────────────────────────────────┘       └─────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          BYTECODE INTERPRETER                               │
│                             _n_burn()                                       │
│                                                                             │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐  │
│  │ Cap Stack   │    │ Burn Frames │    │ Computed    │    │ Jet         │  │
│  │ (operands)  │    │ (call stack)│    │ Goto        │    │ Integration │  │
│  └─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘  │
│                                                                 │           │
└─────────────────────────────────────────────────────────────────┼───────────┘
                                                                  │
                    ┌─────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              JET SYSTEM                                     │
│                                                                             │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐         │
│  │   HOT STATE     │    │   COLD STATE    │    │   WARM STATE    │         │
│  │  (static jets)  │    │ (battery→reg)   │    │ (loc→activation)│         │
│  │  u3R->jed.hot_p │    │ u3R->jed.cod_p  │    │ u3R->jed.war_p  │         │
│  └────────┬────────┘    └────────┬────────┘    └────────┬────────┘         │
│           │                      │                      │                   │
│           └──────────────────────┼──────────────────────┘                   │
│                                  │                                          │
│                                  ▼                                          │
│                        ┌─────────────────┐                                  │
│                        │  Native C Jets  │                                  │
│                        │  ham_u->fun_f() │                                  │
│                        └─────────────────┘                                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Module Responsibilities

| Module | File | Primary Responsibility |
|--------|------|----------------------|
| Nock Interpreter | `nock.c` | Compile and execute Nock formulas |
| Jet System | `jets.c` | Accelerate known cores with C code |
| Program Cache | `nock.c` | Cache compiled bytecode |
| State Management | `jets.c` | Track core registrations |

---

## Execution Model

### Complete Execution Trace

```
User code: (add 2 3)
│
├─► u3n_nock_on(subject, formula)
│   │
│   ├─► _n_find(u3_nul, formula)
│   │   │
│   │   ├─► Cache lookup in u3R->byc.har_p
│   │   │   Hit? Return cached u3n_prog
│   │   │
│   │   └─► Cache miss: _n_bite(formula)
│   │       │
│   │       ├─► _n_comp(&ops, formula, c3y, c3y)
│   │       │   └─► Recursive compilation to op list
│   │       │
│   │       └─► _n_prog_from_ops(ops)
│   │           └─► Assemble to u3n_prog
│   │
│   └─► _n_burn(pog_u, subject, mov, off)
│       │
│       ├─► ... standard operations ...
│       │
│       ├─► KICB (kick opcode for nock 9)
│       │   │
│       │   └─► _n_kick(core, sit_u)
│       │       │
│       │       └─► u3j_site_kick(core, sit_u)
│       │           │
│       │           ├─► _cj_site_kick(core, sit_u)
│       │           │   │
│       │           │   ├─► _cj_fine(core, fin_p)
│       │           │   │   └─► Validate core lineage
│       │           │   │
│       │           │   └─► _cj_site_kick_hot(loc, core, sit_u, c3y)
│       │           │       │
│       │           │       ├─► jet_o? _cj_kick_z()
│       │           │       │   └─► ham_u->fun_f(core)
│       │           │       │       └─► NATIVE C CODE
│       │           │       │
│       │           │       └─► No jet? Return u3_none
│       │           │           └─► Interpreter continues
│       │           │
│       │           └─► Jet result or u3_none
│       │
│       └─► HALT
│           └─► Return result
│
└─► Result: 5
```

### Decision Points

```
                          ┌─────────────────┐
                          │ Nock 9 (kick)   │
                          │ at axis N       │
                          └────────┬────────┘
                                   │
                    ┌──────────────┴──────────────┐
                    │ Site cache valid?           │
                    │ (sit_u->loc != u3_none &&   │
                    │  _cj_fine(cor, fin_p))      │
                    └──────────────┬──────────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              │ Yes                │ No                 │
              ▼                    ▼                    │
    ┌─────────────────┐  ┌─────────────────┐           │
    │ Use cached jet  │  │ _cj_spot()      │           │
    │ info            │  │ Find location   │           │
    └────────┬────────┘  └────────┬────────┘           │
             │                    │                     │
             │                    ▼                     │
             │           ┌─────────────────┐           │
             │           │ Update cache    │           │
             │           │ sit_u->*        │           │
             │           └────────┬────────┘           │
             │                    │                     │
             └────────────┬───────┘                     │
                          │                             │
                          ▼                             │
             ┌─────────────────────┐                    │
             │ sit_u->jet_o?       │◄───────────────────┘
             │ Have jet driver?    │
             └──────────┬──────────┘
                        │
         ┌──────────────┴──────────────┐
         │ Yes                         │ No
         ▼                             ▼
┌─────────────────┐           ┌─────────────────┐
│ _cj_kick_z()    │           │ Return u3_none  │
│ Call jet        │           │ Fall through to │
│ ham_u->fun_f()  │           │ bytecode interp │
└────────┬────────┘           └────────┬────────┘
         │                             │
         ▼                             ▼
┌─────────────────┐           ┌─────────────────┐
│ Jet succeeded?  │           │ Execute arm via │
│                 │           │ _n_burn()       │
└────────┬────────┘           └─────────────────┘
         │
    ┌────┴────┐
    │Yes  │No │
    ▼     ▼   │
  Result  Punt│
         (return u3_none,
          fall through)
```

---

## Integration Points

### Call Sites (u3j_site)

Embedded in `u3n_prog.cal_u.sit_u[]`, created for each Nock 9 operation:

```
u3n_prog
├── byc_u: bytecode containing KICB/TICS opcodes
└── cal_u
    └── sit_u[0]: u3j_site for first kick
        ├── axe: axis being kicked
        ├── pog_p: compiled program for arm (if no jet)
        ├── jet_o: c3y if jet available
        ├── cop_u: u3j_core* for jet driver
        ├── ham_u: u3j_harm* for arm
        └── fin_p: u3j_fink* for verification
```

**Lifecycle:**
1. Created uninitialized during compilation
2. Populated on first execution via `_cj_site_kick()`
3. Validated and possibly updated on subsequent executions
4. Freed when program is freed

### Registration Sites (u3j_rite)

Embedded in `u3n_prog.reg_u.rit_u[]`, created for each `%fast` hint:

```
u3n_prog
├── byc_u: bytecode containing BAST/SALT opcodes
└── reg_u
    └── rit_u[0]: u3j_rite for first %fast
        ├── own_o: ownership flag
        ├── clu: cached clue
        └── fin_p: fine check for verification
```

**Purpose:** Cache registration state to avoid re-mining cores.

### Bytecode ↔ Jets Communication

```c
// In _n_burn(), KICB handler:
do_kicb:
  x = pog[ip_w++];              // Get site index
  sit_u = &(pog_u->cal_u.sit_u[x]);
  top   = _n_peek(off);         // Core on stack
  o     = *top;
  *top = _n_kick(o, sit_u);     // Try jet

  if ( u3_none == *top ) {
    // Jet not available or punted
    // Set up for bytecode interpretation
    pog_u = u3to(u3n_prog, sit_u->pog_p);
    pog   = pog_u->byc_u.ops_y;
    ip_w  = 0;
    _n_push(mov, off, o);       // Push core back
  }
  BURN();
```

---

## State Management

### Per-Road State

Each road has its own:

```c
// In u3a_road:
struct {
  u3p(u3h_root) har_p;    // Bytecode cache
} byc;

u3a_jets jed;             // Jet state
// jed.war_p: warm state
// jed.cod_p: cold state
// jed.han_p: hank cache
// jed.bas_p: bash cache
// jed.hot_p: hot state (home road only)
```

### State Promotion (Reaping)

When a child road completes:

```
Child Road                     Parent Road
┌─────────────┐               ┌─────────────┐
│ byc.har_p   │──u3n_reap()──►│ byc.har_p   │
│ jed.war_p   │──u3j_reap()──►│ jed.war_p   │
│ jed.cod_p   │──────────────►│ jed.cod_p   │
│ jed.han_p   │──────────────►│ jed.han_p   │
│ jed.bas_p   │──────────────►│ jed.bas_p   │
└─────────────┘               └─────────────┘
```

**Process:**
1. `u3n_take()`: Copy bytecode programs
2. `u3n_reap()`: Merge into parent cache
3. `u3j_take()`: Copy jet state
4. `u3j_reap()`: Merge into parent state

### Checkpoint Persistence

Cold state survives restart:

```
u3j_stay() ──► Extract cold state list
                        │
                        ▼
               [battery [bash registry]]
               [battery [bash registry]]
               ...
                        │
                        ▼
u3j_load() ◄── Inject cold state
                        │
                        ▼
u3j_ream() ──► Rebuild warm from cold
```

---

## Performance Optimizations

### 1. Bytecode Compilation

Instead of tree-walking Nock formulas:
- Compile once to flat bytecode
- Cache compiled programs
- Use computed goto for dispatch

### 2. Call Site Caching

Instead of looking up jets on every kick:
- Cache jet information in u3j_site
- Validate with lightweight fine check
- Skip lookup if cache valid

### 3. Fine Checks

Instead of full core comparison:
- Store battery/axis pairs to root
- Compare batteries only (not payloads)
- Single noun comparison at root

### 4. Hank Cache

Instead of searching for hooks on every call:
- Cache hook axis and site data
- Key by [caller-key hook-name]

### 5. Tail-Call Optimization

For Nock 9 in tail position:
- Use TICB/TICS instead of KICB/KICS
- Don't push burn frame
- Replace current program directly

### 6. Literal Deduplication

Compile-time optimization:
- Small literals inlined in bytecode
- Large literals stored once in lit_u.non[]
- Indexed access via LIBK/LISK opcodes

---

## Lifecycle

### Boot Sequence

```
1. u3j_boot(c3y)
   └─► Initialize jet dashboard
       ├─► Count cores in tree
       ├─► Allocate ray_u array
       ├─► Create hot state table
       └─► Install all drivers

2. Load pill / snapshot
   └─► If snapshot exists:
       ├─► u3j_load(cold_state)
       │   └─► Populate cod_p
       └─► u3j_ream()
           ├─► Free old warm state
           ├─► Rebuild from cold
           └─► Ream call sites

3. Normal execution begins
```

### Runtime Registration

```
%fast hint encountered
        │
        ▼
u3j_rite_mine(rit_u, clu, cor)
        │
        ├─► Check rit_u cache
        │   └─► If valid: done
        │
        └─► _cj_mile(clu, cor)
            ├─► _cj_je_fsck(clu)     Parse clue
            ├─► _cj_spot(cor, &bas)  Find existing
            │   └─► If found: validate
            └─► _cj_mine(cey, cor, bas)
                ├─► Build location
                ├─► Update cold state
                └─► Update warm state
```

### Shutdown / Checkpoint

```
1. u3j_stay()
   └─► Extract cold state

2. Serialize cold state to disk

3. u3j_free()
   ├─► Free hank cache entries
   ├─► Free hash tables
   └─► (ray_u is static)

4. u3n_free()
   ├─► Free all programs
   └─► Free bytecode cache
```

---

## Debugging and Tracing

### Verbose Bytecode

Compile with `VERBOSE_BYTECODE`:
```c
#define BURN() fprintf(stderr, "%s ", opcode_names[pog[ip_w]]); \
               goto *lab[pog[ip_w++]]
```

### Bytecode Inspection

The `%xray` hint prints compiled bytecode:
```hoon
~>  %xray  (add 2 3)
```

Output:
```
{[LITB 2] SWAP [LITB 3] [KICB 0] HALT}
```

### Jet Tracing

Enable `glu_o` tracing to see jet activity:
- Jet entry/exit timing
- Cache hits/misses
- Punt events

### Memory Diagnostics

The `%meme` hint shows memory usage:
```hoon
~>  %meme.(0 'label')  expr
```

The `%cash` hint shows cache statistics:
```hoon
~>  %cash  expr
```

### Profile Points

- `noc_o`: Nock interpreter timing
- `glu_o`: Jet glue timing

---

## Summary

The Nock bytecode interpreter and jet system form a tightly integrated execution engine:

1. **Compilation**: Nock formulas are compiled to bytecode once and cached
2. **Interpretation**: A stack-based interpreter executes bytecode
3. **Acceleration**: Known cores are accelerated by native C jets
4. **Caching**: Multiple levels of caching avoid redundant work
5. **Validation**: Fine checks ensure jets match expected cores
6. **Persistence**: Cold state survives restarts for fast warm-up

The key insight is that jets are not a separate execution path but are integrated directly into the bytecode interpreter at kick points. This allows seamless fallback to interpretation when jets aren't available or punt.
