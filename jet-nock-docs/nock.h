/// @file nock.h
/// @brief Nock Bytecode Interpreter Header - Defines structures and functions for Nock execution
///
/// This module implements a bytecode-compiled interpreter for Nock, the functional
/// assembly language underlying Urbit. Instead of interpreting Nock formulas directly,
/// they are compiled to an internal bytecode format for faster execution.
///
/// ## Architecture Overview
///
/// The Nock execution system has three layers:
/// 1. **Formula Compilation**: Nock formulas are compiled to bytecode programs (u3n_prog)
/// 2. **Bytecode Execution**: Programs are executed by a stack-based interpreter (_n_burn)
/// 3. **Jet Integration**: The interpreter integrates with jets for accelerated execution
///
/// ## Bytecode Cache
///
/// Compiled programs are cached in `u3R->byc.har_p`, keyed by (prefix, formula) pairs.
/// The prefix allows different compilation contexts (e.g., different locations) to
/// have separate cached programs.
///
/// ## Stack Machine
///
/// The bytecode interpreter uses the cap stack (u3R->cap_p) as its operand stack.
/// Operations push and pop nouns, with the direction depending on road orientation
/// (north vs south).
///
/// ## Memory Management
///
/// - Programs are allocated with u3a_malloc (not on the loom)
/// - Literals within programs reference loom nouns (must be marked for GC)
/// - Call sites (u3j_site) and registration sites (u3j_rite) are embedded in programs

#ifndef U3_NOCK_H
#define U3_NOCK_H

#include <stdio.h>

#include "c3/c3.h"
#include "jets.h"
#include "types.h"
#include "zave.h"

  /** Data structures.
  ***
  **/

  /// @struct u3n_memo
  /// @brief Memo hint slot - stores data for %memo hint caching
  ///
  /// Each memo slot corresponds to one %memo hint in the compiled formula.
  /// The memo cache prevents recomputation of pure functions.
  ///
  /// ## Fields
  /// - `sip_l`: Skip distance - bytes to skip if cache hits
  /// - `key`: Cache key (the formula being memoized)
  /// - `cid`: Cache ID - determines which cache to use (ephemeral vs persistent)
  ///
  typedef struct {
    c3_l    sip_l;
    u3_noun key;
    u3z_cid cid;
  } u3n_memo;

  /// @struct u3n_prog
  /// @brief Compiled Nock program - bytecode and associated data
  ///
  /// A u3n_prog contains everything needed to execute a compiled Nock formula:
  /// - Bytecode instructions
  /// - Literal values referenced by the bytecode
  /// - Memo slots for caching
  /// - Call sites for nock 9 (kicks)
  /// - Registration sites for %fast hints
  ///
  /// ## Memory Layout
  /// The structure uses a flexible array pattern. After the fixed fields,
  /// memory is laid out as:
  /// ```
  /// [bytecode | padding | literals | memo slots | call sites | reg sites]
  /// ```
  /// Padding ensures proper alignment for each section.
  ///
  /// ## Ownership
  /// - `byc_u.own_o`: If yes, this program owns its bytecode (should free)
  /// - If no, bytecode is shared with a parent program (for road promotion)
  ///
  /// ## Fields
  /// - `byc_u`: Bytecode array
  ///   - `own_o`: Ownership flag
  ///   - `len_w`: Length in bytes
  ///   - `ops_y`: Pointer to bytecode
  /// - `lit_u`: Literals array
  ///   - `len_w`: Number of literals
  ///   - `non`: Array of noun pointers
  /// - `mem_u`: Memo slots
  ///   - `len_w`: Number of memo slots
  ///   - `sot_u`: Array of u3n_memo
  /// - `cal_u`: Call sites (nock 9)
  ///   - `len_w`: Number of call sites
  ///   - `sit_u`: Array of u3j_site
  /// - `reg_u`: Registration sites (%fast)
  ///   - `len_w`: Number of registration sites
  ///   - `rit_u`: Array of u3j_rite
  ///
  typedef struct _u3n_prog {
    struct {
      c3_o      own_o;                // program owns ops_y?
      c3_w      len_w;                // length of bytecode (bytes)
      c3_y*     ops_y;                // actual array of bytes
    } byc_u;                          // bytecode
    struct {
      c3_w      len_w;                // number of literals
      u3_noun*  non;                  // array of literals
    } lit_u;                          // literals
    struct {
      c3_w      len_w;                // number of memo slots
      u3n_memo* sot_u;                // array of memo slots
    } mem_u;                          // memo slot data
    struct {
      c3_w      len_w;                // number of calls sites
      u3j_site* sit_u;                // array of sites
    } cal_u;                          // call site data
    struct {
      c3_w      len_w;                // number of registration sites
      u3j_rite* rit_u;                // array of sites
    } reg_u;                          // registration site data
  } u3n_prog;

  /**  Functions.
  **/

    /// @brief Execute Nock: produce .*(bus fol)
    ///
    /// The main Nock evaluation function. Compiles the formula to bytecode
    /// (if not cached) and executes it with the given subject.
    ///
    /// ## Internal Logic
    /// 1. Find or compile program via _n_find()
    /// 2. Execute via _n_burn_out()
    ///
    /// This function does NOT virtualize - errors propagate directly.
    ///
    /// @param bus Subject (TRANSFER)
    /// @param fol Formula (TRANSFER)
    /// @return Result of evaluation
    ///
    u3_noun
    u3n_nock_on(u3_noun bus, u3_noun fol);

    /// @brief Find or compile program for formula
    ///
    /// Looks up the formula in the bytecode cache, compiling if necessary.
    /// The key parameter allows different compilation contexts.
    ///
    /// @param key Prefix for cache key (u3_nul for none). RETAIN.
    /// @param fol Formula to compile. RETAIN.
    /// @return Program pointer (NOT a transfer - lives in cache)
    ///
    u3p(u3n_prog)
    u3n_find(u3_noun key, u3_noun fol);

    /// @brief Execute compiled program
    ///
    /// Runs a pre-compiled bytecode program with the given subject.
    ///
    /// @param pog_p Program to execute
    /// @param bus Subject (TRANSFER)
    /// @return Result of execution
    ///
    u3_noun
    u3n_burn(u3p(u3n_prog) pog_p, u3_noun bus);

    /// @brief Slam gate: produce (gat sam)
    ///
    /// Calls a gate with the given sample. Equivalent to:
    /// ```nock
    /// .*(gat(+6 sam) -.gat)
    /// ```
    ///
    /// @param gat Gate (TRANSFER)
    /// @param sam Sample (TRANSFER)
    /// @return Result
    ///
    u3_noun
    u3n_slam_on(u3_noun gat, u3_noun sam);

    /// @brief Kick gate without changing sample
    ///
    /// Fires a gate's arm without modifying its sample.
    /// Equivalent to kicking axis 2.
    ///
    /// @param gat Gate (TRANSFER)
    /// @return Result
    ///
    u3_noun
    u3n_kick_on(u3_noun gat);

    /// @brief Execute Nock in namespace, returning toon
    ///
    /// Like u3n_nock_on but virtualized with error handling and
    /// namespace support via fly.
    ///
    /// @param fly Namespace (for scry)
    /// @param bus Subject (TRANSFER)
    /// @param fol Formula (TRANSFER)
    /// @return Toon: [%0 result] or [%1 block] or [%2 trace]
    ///
    u3_noun
    u3n_nock_in(u3_noun fly, u3_noun bus, u3_noun fol);

    /// @brief Execute Nock with sea namespace
    ///
    /// @param sea Sea namespace
    /// @param bus Subject (TRANSFER)
    /// @param fol Formula (TRANSFER)
    /// @return Toon
    ///
    u3_noun
    u3n_nock_it(u3_noun sea, u3_noun bus, u3_noun fol);

    /// @brief Execute Nock with gul namespace
    ///
    /// @param gul Gul namespace
    /// @param bus Subject (TRANSFER)
    /// @param fol Formula (TRANSFER)
    /// @return Toon
    ///
    u3_noun
    u3n_nock_et(u3_noun gul, u3_noun bus, u3_noun fol);

    /// @brief Slam gate in namespace, returning toon
    ///
    /// @param fly Namespace
    /// @param gat Gate (TRANSFER)
    /// @param sam Sample (TRANSFER)
    /// @return Toon
    ///
    u3_noun
    u3n_slam_in(u3_noun fly, u3_noun gat, u3_noun sam);

    /// @brief Slam gate with sea namespace
    ///
    /// @param sea Sea namespace
    /// @param gat Gate (TRANSFER)
    /// @param sam Sample (TRANSFER)
    /// @return Toon
    ///
    u3_noun
    u3n_slam_it(u3_noun sea, u3_noun gat, u3_noun sam);

    /// @brief Slam gate with gul namespace
    ///
    /// @param gul Gul namespace
    /// @param gat Gate (TRANSFER)
    /// @param sam Sample (TRANSFER)
    /// @return Toon
    ///
    u3_noun
    u3n_slam_it(u3_noun gul, u3_noun gat, u3_noun sam);

    /// @brief Execute Nock with empty namespace
    ///
    /// Convenience wrapper for u3n_nock_in with empty fly.
    ///
    /// @param bus Subject (TRANSFER)
    /// @param fol Formula (TRANSFER)
    /// @return Toon
    ///
    u3_noun
    u3n_nock_an(u3_noun bus, u3_noun fol);

    /// @brief Promote bytecode state from junior road
    ///
    /// Integrates compiled programs from a child road into the current road.
    ///
    /// @param har_p Hash table of programs to promote
    ///
    void
    u3n_reap(u3p(u3h_root) har_p);

    /// @brief Copy junior bytecode state
    ///
    /// Creates a copy of bytecode cache suitable for promotion.
    ///
    /// @param har_p Hash table to copy
    /// @return Copied hash table
    ///
    u3p(u3h_root)
    u3n_take(u3p(u3h_root) har_p);

    /// @brief Mark bytecode cache for GC
    ///
    /// @return Quac tree of marked memory
    ///
    u3m_quac*
    u3n_mark();

    /// @brief Clear bytecode cache to reclaim memory
    ///
    /// Frees all compiled programs and recreates an empty cache.
    ///
    void
    u3n_reclaim(void);

    /// @brief Rewrite bytecode cache for loom compaction
    ///
    /// Relocates hash table pointers after loom addresses change.
    ///
    void
    u3n_rewrite_compact(void);

    /// @brief Free bytecode cache
    ///
    /// Frees all programs and the cache hash table.
    ///
    void
    u3n_free(void);

    /// @brief Refresh bytecode state after checkpoint restore
    ///
    /// Fixes up internal pointers in programs and reams call sites.
    ///
    void
    u3n_ream(void);

#endif /* ifndef U3_NOCK_H */
