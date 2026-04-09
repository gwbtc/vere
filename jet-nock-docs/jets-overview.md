# Jets System Overview

The jet system provides native C implementations of commonly-used Nock functions, enabling dramatic performance improvements while maintaining semantic equivalence with the Nock specification.

## Table of Contents

1. [Architecture](#architecture)
2. [State Management](#state-management)
3. [Data Structures](#data-structures)
4. [Core Functions](#core-functions)
5. [Execution Flow](#execution-flow)
6. [Memory Management](#memory-management)
7. [Internal Functions Reference](#internal-functions-reference)

---

## Architecture

### Three-State Model

The jet system maintains three distinct states:

```
┌─────────────────────────────────────────────────────────────────────┐
│                           HOT STATE                                 │
│  Static driver definitions compiled into binary (read-only)         │
│  Stored in: u3R->jed.hot_p                                         │
│  Key: battery hash (bash)                                           │
│  Value: [registry, jax_l, drivers, label]                          │
└─────────────────────────────────────────────────────────────────────┘
                                 ↓
┌─────────────────────────────────────────────────────────────────────┐
│                          COLD STATE                                 │
│  Persistent mapping of batteries to registries (survives restart)   │
│  Stored in: u3R->jed.cod_p                                         │
│  Key: battery noun                                                  │
│  Value: [bash, registry]                                           │
└─────────────────────────────────────────────────────────────────────┘
                                 ↓
┌─────────────────────────────────────────────────────────────────────┐
│                          WARM STATE                                 │
│  Runtime cache of locations to activations (rebuilt from cold)      │
│  Stored in: u3R->jed.war_p                                         │
│  Key: location                                                      │
│  Value: [jax_l, axis->driver map, label, jit]                      │
└─────────────────────────────────────────────────────────────────────┘
```

### Key Concepts

| Term | Definition |
|------|------------|
| **Battery** | The code portion of a core (head of core cell) |
| **Payload** | The data portion of a core (tail of core cell) |
| **Location** | Describes where a core sits in the hierarchy |
| **Registry** | Maps batteries to locations within a core hierarchy |
| **Activation** | Runtime jet state including axis-to-arm mappings |
| **Bash** | Battery hash (SHA-256 of jammed battery) |

---

## State Management

### Jet State Structure (u3a_jets)

Found in each road at `u3R->jed`:

```c
typedef struct {
  u3p(u3h_root) war_p;    // warm state: location -> activation
  u3p(u3h_root) cod_p;    // cold state: battery -> [bash registry]
  u3p(u3h_root) han_p;    // hank cache: [key tam] -> hank
  u3p(u3h_root) bas_p;    // bash cache: battery -> bash
  u3p(u3h_root) hot_p;    // hot state:  bash -> hot-info (home road only)
} u3a_jets;
```

### State Transitions

```
          %fast hint                 Boot/Ream
              │                          │
              ▼                          ▼
   ┌──────────────────┐       ┌──────────────────┐
   │  u3j_mine()      │       │  u3j_ream()      │
   │  - Parse clue    │       │  - Tap cold      │
   │  - Check spot    │       │  - Free warm     │
   │  - Register cold │       │  - Rebuild warm  │
   │  - Update warm   │       │                  │
   └──────────────────┘       └──────────────────┘
              │                          │
              ▼                          ▼
       Cold + Warm updated         Warm rebuilt
```

---

## Data Structures

### u3j_core - Driver Definition

```c
typedef struct _u3j_core {
  c3_c*             cos_c;    // core name
  c3_l              axe_l;    // axis to parent (0=root, 3=payload)
  struct _u3j_harm* arm_u;    // jetted arms (null-terminated)
  struct _u3j_core* dev_u;    // child cores (null-terminated)
  c3_c**            bas_u;    // battery hashes (null-terminated)
  struct _u3j_hood* huc_u;    // hooks (null-terminated)
  struct _u3j_core* par_u;    // parent (set at boot)
  c3_l              jax_l;    // index in dashboard
} u3j_core;
```

**Tree Structure Example:**
```
u3j_core: "k140" (kernel)
├── arm: "add"  [.86]
├── arm: "sub"  [.87]
├── dev: "ut" (type system)
│   ├── arm: "mint" [.20]
│   └── arm: "nest" [.21]
└── dev: "ap" (parser)
    └── arm: "scan" [.2]
```

### u3j_harm - Arm Definition

```c
typedef struct _u3j_harm {
  c3_c*             fcs_c;    // ".axe" or arm name
  u3_noun           (*fun_f)(u3_noun);  // jet function
  c3_o              ice;      // perfect (don't verify)
  c3_o              tot;      // total (never punts)
  c3_o              liv;      // live (enabled)
  c3_l              axe_l;    // discovered axis
  struct _u3j_core* cop_u;    // containing core
} u3j_harm;
```

### u3j_site - Call Site Cache

```c
typedef struct {
  u3p(u3n_prog) pog_p;    // compiled program
  u3_noun       axe;       // axis being kicked
  u3_weak       bat;       // cached battery
  u3_weak       bas;       // battery hash
  u3_weak       loc;       // location
  c3_o          jet_o;     // have jet?
  c3_o          fon_o;     // owns fink?
  u3_weak       lab;       // label path
  u3j_core*     cop_u;     // jet core
  u3j_harm*     ham_u;     // jet arm
  u3p(u3j_fink) fin_p;     // fine check
} u3j_site;
```

### u3j_fink - Fine Check

```c
typedef struct {
  c3_w    len_w;          // number of fists
  u3_noun sat;            // static noun at end
  u3j_fist fis_u[];       // battery/axis pairs
} u3j_fink;

typedef struct {
  u3_noun bat;            // battery to check
  u3_noun pax;            // axis to next parent
} u3j_fist;
```

**Verification Process:**
```
core ──[fis_u[0].pax]──► parent1 ──[fis_u[1].pax]──► parent2 ──► sat
  │                         │                            │
  │ check battery           │ check battery              │ compare
  ▼                         ▼                            ▼
fis_u[0].bat            fis_u[1].bat                   sat
```

---

## Core Functions

### Initialization

#### `u3j_boot(c3_o nuu_o)`

Initializes the jet system at startup.

```
u3j_boot(nuu_o)
    │
    ├──► _cj_count(dev_u)           // Count cores in tree
    │
    ├──► Allocate ray_u array       // Flat array for all cores
    │
    ├──► u3h_free() if !nuu_o       // Free old hot state
    │
    ├──► u3h_new() → hot_p          // Create new hot state
    │
    └──► _cj_install(dev_u, ...)    // Install all drivers
              │
              ├──► Assign jax_l to each core
              ├──► Build axis-to-arm map
              ├──► Hash batteries
              └──► Populate hot state
```

### Registration

#### `u3j_mine(clu, cor)`

Registers a core for jet acceleration (processes %fast hint).

```
u3j_mine(clu, cor)
    │
    └──► _cj_mile(clu, cor)
              │
              ├──► _cj_je_fsck(clu)           // Parse clue
              │         │
              │         └──► Returns [name axis hooks]
              │
              ├──► _cj_spot(cor, &bas)        // Find existing registration
              │         │
              │         ├──► _cj_find_cold(battery)
              │         │         │
              │         │         └──► Search cod_p by battery
              │         │
              │         └──► If not found: compute bash, search cold
              │
              └──► _cj_mine(cey, cor, bas)    // Register if new
                        │
                        ├──► _cj_minx(cey, cor)     // Build location
                        │
                        ├──► _cj_gust(reg, ...)     // Update registry
                        │
                        ├──► _cj_warm_hump(...)     // Build axis map
                        │
                        └──► Update cod_p and war_p
```

### Execution

#### `u3j_kick(cor, axe)`

Attempts to execute an arm using its jet.

```
u3j_kick(cor, axe)
    │
    └──► _cj_kick(cor, axe)
              │
              ├──► _cj_spot(cor, NULL)        // Find location
              │         │
              │         └──► Returns location or u3_none
              │
              ├──► _cj_find_warm(loc)         // Get activation
              │         │
              │         └──► Returns [jax hap bal jit]
              │
              ├──► u3h_get(hap, axe)          // Find arm index
              │
              └──► _cj_kick_z(cor, cop_u, ham_u, axe)
                        │
                        ├──► Check ham_u->liv (enabled?)
                        ├──► Check ham_u->fun_f (has function?)
                        └──► Call jet: ham_u->fun_f(cor)
```

#### `u3j_site_kick(cor, sit_u)`

Kicks using a cached call site.

```
u3j_site_kick(cor, sit_u)
    │
    └──► _cj_site_kick(cor, sit_u)
              │
              ├──► If loc cached and _cj_fine() passes:
              │         │
              │         └──► _cj_site_kick_hot(loc, cor, sit_u, c3y)
              │
              └──► Else:
                        │
                        ├──► _cj_spot(cor, &bas)      // Find fresh
                        ├──► Update sit_u fields
                        └──► _cj_site_kick_hot(loc, cor, sit_u, c3y)
```

### Hooks

#### `u3j_cook(key_c, cor, tam_c)`

Executes a hook by name with caching.

```
u3j_cook(key_c, cor, tam_c)
    │
    ├──► Build key: [key_c tam_c]
    │
    ├──► _cj_hank_find(key, tam)      // Find/create cache entry
    │         │
    │         └──► han_p hash lookup
    │
    ├──► _cj_hank_fine(han_u, cor, &inn)  // Validate cache
    │
    ├──► If invalid: _cj_hank_fill(han_u, tam, cor)
    │         │
    │         ├──► _cj_look(cor, tam)     // Find hook axis
    │         │         │
    │         │         └──► Walk parent chain looking for hook
    │         │
    │         ├──► Compute inner core
    │         └──► Initialize sit_u
    │
    └──► _cj_site_kick(inn, &han_u->sit_u)
```

---

## Execution Flow

### Complete Kick Flow

```
User Code: (add 2 3)
           │
           ▼
┌──────────────────────────────────────────────────────────────┐
│  Nock 9 [2 fol]                                              │
│  - Evaluate fol to get core                                  │
│  - Kick axis 2 of core                                       │
└──────────────────────────────────────────────────────────────┘
           │
           ▼
┌──────────────────────────────────────────────────────────────┐
│  _n_kick(cor, sit_u)  [in nock.c bytecode interpreter]       │
│  - Check site cache                                          │
│  - Call u3j_site_kick()                                      │
└──────────────────────────────────────────────────────────────┘
           │
           ▼
┌──────────────────────────────────────────────────────────────┐
│  u3j_site_kick(cor, sit_u)  [in jets.c]                      │
│  - Validate cache via _cj_fine()                             │
│  - Find/update location                                      │
│  - Call _cj_site_kick_hot()                                  │
└──────────────────────────────────────────────────────────────┘
           │
           ├────────────────────────────┬──────────────────────┐
           ▼                            ▼                      ▼
┌────────────────────┐    ┌────────────────────┐    ┌──────────────────┐
│  Jet Available     │    │  No Jet            │    │  Jet Punts       │
│  _cj_kick_z()      │    │  Return u3_none    │    │  Return u3_none  │
│  ham_u->fun_f(cor) │    │  → Fall back to    │    │  → Fall back to  │
│  → Return result   │    │     bytecode       │    │     bytecode     │
└────────────────────┘    └────────────────────┘    └──────────────────┘
```

---

## Memory Management

### Ownership Rules

| Structure | Owner | Lifecycle |
|-----------|-------|-----------|
| `u3j_core` | Dashboard | Static, never freed |
| `u3j_harm` | Dashboard | Static, never freed |
| `u3j_site` | `u3n_prog` | Freed with program |
| `u3j_rite` | `u3n_prog` | Freed with program |
| `u3j_fink` | Site/Rite | Freed when owner loses |
| `u3j_hank` | `han_p` | Freed on cache clear |

### GC Marking

```c
u3j_mark()
    │
    ├──► u3h_mark(war_p)      // Mark warm state
    ├──► u3h_mark(cod_p)      // Mark cold state
    ├──► u3h_mark(han_p)      // Mark hank cache
    ├──► u3h_mark(bas_p)      // Mark bash cache
    │
    └──► _cj_mark_hank(...)   // Mark hank internals
              │
              ├──► u3a_mark_noun(hax)
              └──► u3j_site_mark(sit_u)
                        │
                        ├──► u3a_mark_noun(axe)
                        ├──► u3a_mark_noun(bat)
                        ├──► u3a_mark_noun(loc)
                        └──► _cj_fink_mark(fin_p)
```

---

## Internal Functions Reference

### Core Discovery

| Function | Purpose |
|----------|---------|
| `_cj_spot(cor, &bas)` | Find core's location, optionally get bash |
| `_cj_find_cold(bat)` | Search cold state by battery |
| `_cj_find_warm(loc)` | Search warm state by location |
| `_cj_bash(bat)` | Compute battery hash (SHA-256 of jam) |

### Registration

| Function | Purpose |
|----------|---------|
| `_cj_je_fsck(clu)` | Parse clue from %fast hint |
| `_cj_minx(cey, cor)` | Build location from clue |
| `_cj_mine(cey, cor, bas)` | Register core in cold/warm state |
| `_cj_gust(reg, axe, pel, loc)` | Add location to registry |

### Execution

| Function | Purpose |
|----------|---------|
| `_cj_kick(cor, axe)` | Internal kick implementation |
| `_cj_kick_z(cor, cop_u, ham_u, axe)` | Call jet function |
| `_cj_site_kick(cor, sit_u)` | Kick with site cache |
| `_cj_site_kick_hot(loc, cor, sit_u, lok_o)` | Hot path for site kick |

### Validation

| Function | Purpose |
|----------|---------|
| `_cj_fine(cor, fin_p)` | Validate core against fine check |
| `_cj_cast(cor, loc)` | Build fine check for core |
| `_cj_nail(loc, axe, &lab, &cop_u, &ham_u)` | Resolve jet pointers |

### Hooks

| Function | Purpose |
|----------|---------|
| `_cj_look(cor, tam)` | Find hook axis by name |
| `_cj_hank_find(key, tam)` | Find/create hank cache entry |
| `_cj_hank_fill(han_u, tam, cor)` | Populate hank cache |
| `_cj_hank_fine(han_u, cor, &inn)` | Validate hank cache |

### State Management

| Function | Purpose |
|----------|---------|
| `_cj_ream(all)` | Rebuild warm from cold state list |
| `_cj_warm_hump(jax_l, huc)` | Build axis-to-driver map |
| `_cj_install(dev_u, par_u, jax_l)` | Install drivers into hot |
