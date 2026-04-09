/// @file jets.h
/// @brief Jet System Header - Defines data structures and functions for Urbit's jet acceleration system
///
/// The jet system provides native C implementations of commonly-used Nock functions.
/// Jets bypass the Nock interpreter for performance-critical operations while
/// maintaining semantic equivalence with the Nock specification.
///
/// ## Architecture Overview
///
/// The jet system maintains three states:
/// - **Hot State**: Static driver definitions compiled into the binary (read-only)
/// - **Cold State**: Persistent mapping of batteries to their registries (survives restart)
/// - **Warm State**: Runtime cache of locations to activations (rebuilt from cold on restart)
///
/// ## Key Concepts
///
/// - **Battery**: The code portion of a core (the head of a core cell)
/// - **Payload**: The data portion of a core (the tail of a core cell)
/// - **Location**: Describes where a core sits in the hierarchy (static vs dynamic parent)
/// - **Registry**: Maps batteries to locations within a core hierarchy
/// - **Activation**: Runtime jet state including axis-to-arm mappings
/// - **Bash**: Battery hash - SHA-256 of the jammed battery, used for lookup
///
/// ## Hoon Type Definitions (for reference)
///
/// ```hoon
/// +=  location    $:  pattern=(each static dynamic)
///                     name=term
///                     hooks=(map term axis)
///                 ==
/// +=  static      (each payload=* parent=location)
/// +=  dynamic     [where=axis parent=location]
/// +=  registry    [roots=(map * location) parents=(list parent)]
/// +=  parent      (pair axis (map location location))
/// +=  activation  $:  hot-index=@ud
///                     drivers=(map axis @ud)
///                     label=path
///                     jit=*
///                 ==
/// +=  hot-info    $:  reg=registry
///                     hot-index=@ud
///                     drivers=(map axis @ud)
///                     label=path
///                 ==
/// +=  bash        @  :: battery hash (sha-256 based)
/// +=  hot         (map bash hot-info)
/// +=  cold        (map battery=^ (pair bash registry))
/// +=  warm        (map location activation)
/// ```

#ifndef U3_JETS_H
#define U3_JETS_H

#include "allocate.h"
#include "c3/c3.h"
#include "types.h"
#include "hashtable.h"

  /** Data structures.
  ***
  *** All of these are transient structures allocated with malloc.
  **/

    /// @struct u3j_harm
    /// @brief Jet arm definition - describes a single jetted arm within a core
    ///
    /// Each harm represents one arm (function) that can be accelerated by a C implementation.
    /// Arms are identified either by a direct axis (`.axe` notation) or by name lookup
    /// through the core's hook table.
    ///
    /// ## Memory Semantics
    /// - `fcs_c`: Static string, not freed
    /// - `fun_f`: Function pointer to the jet implementation
    /// - `cop_u`: Back-pointer set during initialization, not owned
    ///
    /// ## Fields
    /// - `fcs_c`: Focus string - either ".axe" (e.g., ".2") for direct axis or arm name
    /// - `fun_f`: Jet function pointer. Takes core (semitransfer), returns result
    /// - `ice`: If yes, jet is "perfect" - don't verify against Nock
    /// - `tot`: If yes, jet is "total" - never punts back to Nock
    /// - `liv`: If yes, jet is live (enabled). Set to no to disable
    /// - `axe_l`: Computed axis within battery (discovered at boot)
    /// - `cop_u`: Back-pointer to containing core definition
    ///
    typedef struct _u3j_harm {
      c3_c*             fcs_c;               //  `.axe` or name
      u3_noun           (*fun_f)(u3_noun);   //  compute or 0 / semitransfer
      // c3_o           (*val_f)(u3_noun);   //  validate or 0 / retain
      c3_o              ice;                 //  perfect (don't test)
      c3_o              tot;                 //  total (never punts)
      c3_o              liv;                 //  live (enabled)
      c3_l              axe_l;               //  computed/discovered axis
      struct _u3j_core* cop_u;               //  containing core
    } u3j_harm;

    /// @struct u3j_hood
    /// @brief Hook description - maps a name to an axis within a core
    ///
    /// Hooks allow arms to be referenced by name rather than axis. They can
    /// either reference a direct fragment (kic_o = no) or a kick (kic_o = yes).
    ///
    /// ## Fields
    /// - `nam_c`: Hook name (e.g., "add", "mul")
    /// - `axe_l`: Axis where the hook is located
    /// - `kic_o`: If yes, hook is a kick (nock 9); if no, just a fragment (nock 0)
    /// - `sax_l`: Subject axis for kicks (0 means axis 1, i.e., whole subject)
    ///
    typedef struct _u3j_hood {
      c3_c*             nam_c;               //  hook name
      c3_l              axe_l;               //  hook axis (XX: direct)
      c3_o              kic_o;               //  hook is kick (vs. fragment)
      c3_l              sax_l;               //  hook subject axis (XX: direct)
    } u3j_hood;

    /// @struct u3j_core
    /// @brief Driver definition - describes a jetted core and its relationship to parents
    ///
    /// Core definitions form a tree structure mirroring the Hoon core hierarchy.
    /// Each core has a name, optional parent axis, arms, child cores, battery hashes,
    /// and hooks.
    ///
    /// ## Tree Structure
    /// - `dev_u`: Points to child cores (null-terminated array)
    /// - `par_u`: Points to parent core (set dynamically at boot)
    /// - `axe_l`: Axis from this core to its parent (0 for root, 3 for static parent)
    ///
    /// ## Battery Hashes
    /// - `bas_u`: Array of SHA-256 hashes (as hex strings) for batteries that should
    ///   match this core. Multiple hashes allow for minor code variations.
    ///
    /// ## Fields
    /// - `cos_c`: Control string / core name
    /// - `axe_l`: Axis to parent (0 = root, 3 = standard payload parent)
    /// - `arm_u`: Null-terminated array of arms
    /// - `dev_u`: Null-terminated array of child cores
    /// - `bas_u`: Null-terminated array of battery hash strings
    /// - `huc_u`: Null-terminated array of hooks
    /// - `par_u`: Dynamic parent pointer (set at boot)
    /// - `jax_l`: Index in global dashboard array
    ///
    typedef struct _u3j_core {
      c3_c*             cos_c;        //  control string
      c3_l              axe_l;        //  axis to parent
      struct _u3j_harm* arm_u;        //  blank-terminated static list
      struct _u3j_core* dev_u;        //  blank-terminated static list
      c3_c**            bas_u;        //  blank-terminated static list
      struct _u3j_hood* huc_u;        //  blank-terminated static list
      struct _u3j_core* par_u;        //  dynamic parent pointer
      c3_l              jax_l;        //  index in global dashboard
    } u3j_core;

    /// @struct u3j_dash
    /// @brief Jet dashboard singleton - global registry of all jet drivers
    ///
    /// The dashboard is the root structure containing all jet definitions.
    /// It's initialized at boot time and remains constant during execution.
    ///
    /// ## Fields
    /// - `dev_u`: Static list of root cores (from jet tree definition)
    /// - `len_l`: Number of cores in ray_u (actual count)
    /// - `all_l`: Allocated length of ray_u array
    /// - `ray_u`: Flattened array of all cores, indexed by jax_l
    ///
    typedef struct _u3j_dash {
      u3j_core*     dev_u;            //  null-terminated static list
      c3_l          len_l;            //  dynamic array length
      c3_l          all_l;            //  allocated length
      u3j_core*     ray_u;            //  dynamic array by axis
    } u3j_dash;

    /// @struct u3j_fist
    /// @brief Fine check step - one battery/axis pair in a verification chain
    ///
    /// Used to verify that a core's lineage matches expectations by checking
    /// each battery in the parent chain.
    ///
    /// ## Fields
    /// - `bat`: Battery (code) that should be at this position
    /// - `pax`: Axis to the next parent in the chain
    ///
    typedef struct {
      u3_noun bat;                  //  battery
      u3_noun pax;                  //  parent axis
    } u3j_fist;

    /// @struct u3j_fink
    /// @brief Fine check structure - verifies a core's location in the hierarchy
    ///
    /// Contains enough information to verify that a given core is located
    /// where we expect it to be. Used for cache validation.
    ///
    /// ## Verification Process
    /// 1. Walk the fist array, comparing each battery
    /// 2. At each step, traverse to parent via pax axis
    /// 3. Finally, compare sat against the static root
    ///
    /// ## Fields
    /// - `len_w`: Number of fists in the chain
    /// - `sat`: Static noun at the end of the check (the root payload)
    /// - `fis_u`: Variable-length array of fist steps
    ///
    typedef struct {
      c3_w    len_w;                //  number of fists
      u3_noun sat;                  //  static noun at end of check
      u3j_fist fis_u[];             //  fists
    } u3j_fink;

    /// @struct u3j_rite
    /// @brief Registration site cache - caches %fast hint processing
    ///
    /// When a core is registered with %fast, we cache the clue and verification
    /// data to avoid re-mining on subsequent calls.
    ///
    /// ## Fields
    /// - `own_o`: If yes, this rite owns the fink (should free it)
    /// - `clu`: Cached product of the clue formula (or u3_none if not cached)
    /// - `fin_p`: Fine check for verifying the core
    ///
    typedef struct {
      c3_o          own_o;          //  rite owns fink?
      u3_weak       clu;            //  cached product of clue formula
      u3p(u3j_fink) fin_p;          //  fine check
    } u3j_rite;

    /// @struct u3j_site
    /// @brief Call site cache - caches nock 9 (kick) operations
    ///
    /// When we kick an arm repeatedly, we cache the jet driver, program,
    /// and verification data to avoid lookup overhead.
    ///
    /// ## Cache Invalidation
    /// The cache is invalidated when the battery changes (checked via bat field).
    /// The fine check (fin_p) verifies the entire core lineage.
    ///
    /// ## Fields
    /// - `pog_p`: Compiled bytecode program for this arm
    /// - `axe`: Axis being kicked
    /// - `bat`: Cached battery for cache invalidation check
    /// - `bas`: Battery hash (bash) for hot state lookup
    /// - `loc`: Location of the core (for warm state lookup)
    /// - `jet_o`: Yes if we have a jet driver for this arm
    /// - `fon_o`: Yes if this site owns the fink (should free it)
    /// - `lab`: Label path for tracing
    /// - `cop_u`: Jet core pointer (if jetted)
    /// - `ham_u`: Jet arm pointer (if jetted)
    /// - `fin_p`: Fine check for verification
    ///
    struct _u3n_prog;
    typedef struct {
      u3p(struct _u3n_prog) pog_p;  //  program for formula
      u3_noun       axe;            //  axis
      u3_weak       bat;            //  battery (for verification)
      u3_weak       bas;            //  hash of battery (for hot find)
      u3_weak       loc;            //  location (for reaming)
      c3_o          jet_o;          //  have jet driver?
      c3_o          fon_o;          //  site owns fink?
      u3_weak       lab;            //  label (for tracing)
      u3j_core*     cop_u;          //  jet core
      u3j_harm*     ham_u;          //  jet arm
      u3p(u3j_fink) fin_p;          //  fine check
    } u3j_site;

    /// @struct u3j_hank
    /// @brief Cached hook information - caches hook lookups by name
    ///
    /// When calling hooks repeatedly (via u3j_cook), we cache the hook axis
    /// and call site data to avoid repeated lookups.
    ///
    /// ## Fields
    /// - `hax`: Axis to the inner core where the hook was found (or u3_none)
    /// - `sit_u`: Call site data for the hook's arm
    ///
    typedef struct {
      u3_weak  hax;                     //  axis of hooked inner core
      u3j_site sit_u;                   //  call-site data
    } u3j_hank;

  /** Globals.
  **/

    /// @var u3j_Dash
    /// @brief Global jet dashboard singleton
    ///
    /// Contains all jet driver definitions. Aliased as u3D for convenience.
    ///
    extern u3j_dash u3j_Dash;
#     define u3D u3j_Dash

  /**  Functions.
  **/

    /// @brief Initialize jet system
    ///
    /// Sets up the jet dashboard, allocates the ray_u array, and installs
    /// all jet drivers into the hot state.
    ///
    /// ## Internal Logic
    /// 1. Count all cores in the tree via _cj_count()
    /// 2. Allocate ray_u with some headroom
    /// 3. If not new (nuu_o = no), free existing hot state
    /// 4. Create new hot state hash table
    /// 5. Install all cores via _cj_install()
    ///
    /// @param nuu_o Yes if this is a fresh boot (no prior hot state)
    /// @return Number of cores installed
    ///
    c3_w
    u3j_boot(c3_o nuu_o);

    /// @brief Clear jet table to re-register
    ///
    /// Used when jet definitions need to be reloaded.
    ///
    void
    u3j_clear(void);

    /// @brief Execute hook from core with cached call site
    ///
    /// Looks up and executes a hook by name, caching the lookup for future calls.
    /// The cache key is formed from key_c and tam_c.
    ///
    /// ## Internal Logic
    /// 1. Look up or create hank cache entry via _cj_hank_find()
    /// 2. If cache invalid, populate via _cj_hank_fill()
    /// 3. Kick via _cj_site_kick(), falling back to bytecode
    ///
    /// @param key_c Arbitrary C string for cache key prefix
    /// @param cor Core to execute hook on (TRANSFER)
    /// @param tam_c Hook name
    /// @return Result of hook execution
    ///
    u3_noun
    u3j_cook(const c3_c* key_c,
             u3_noun     cor,
             const c3_c* tam_c);

    /// @brief Execute hook from core (with jets)
    ///
    /// Looks up and executes a hook by name. Uses jets if available.
    ///
    /// @param cor Core to execute hook on (TRANSFER)
    /// @param tam_c Hook name
    /// @return Result of hook execution
    ///
    u3_noun
    u3j_hook(u3_noun     cor,
             const c3_c* tam_c);

    /// @brief Execute hook from core without jets
    ///
    /// Like u3j_hook but explicitly disables jet acceleration.
    /// Useful for testing jet correctness.
    ///
    /// @param cor Core to execute hook on (TRANSFER)
    /// @param tam_c Hook name
    /// @return Result of hook execution
    ///
    u3_noun
    u3j_soft(u3_noun     cor,
             const c3_c* tam_c);

    /// @brief Try to kick by jet
    ///
    /// Attempts to execute an arm using its jet. Returns u3_none if
    /// no jet is available or the jet punts.
    ///
    /// ## Internal Logic
    /// 1. Find core location via _cj_spot()
    /// 2. Look up activation in warm state
    /// 3. Find arm in axis-to-arm map
    /// 4. Call jet function via _cj_kick_z()
    ///
    /// @param cor Core (RETAINED if no kick, TRANSFERRED if kick)
    /// @param axe Axis to kick (RETAINED)
    /// @return Result or u3_none
    ///
    u3_weak
    u3j_kick(u3_noun cor, u3_noun axe);

    /// @brief Kick either by jet or by nock
    ///
    /// Tries jet first, falls back to Nock interpretation.
    ///
    /// @param cor Core (TRANSFER)
    /// @param axe Axis to kick
    /// @return Result of kick
    ///
    u3_noun
    u3j_kink(u3_noun cor,
               u3_noun axe);

    /// @brief Register core for jets
    ///
    /// Processes a %fast hint to register a core in the jet system.
    ///
    /// ## Internal Logic (via _cj_mile)
    /// 1. Parse clue via _cj_je_fsck()
    /// 2. Check if core already known via _cj_spot()
    /// 3. If not, register via _cj_mine()
    ///
    /// @param clu Clue (TRANSFER)
    /// @param cor Core to register (TRANSFER)
    ///
    void
    u3j_mine(u3_noun clu,
               u3_noun cor);

    /// @brief Refresh warm state after restoring from checkpoint
    ///
    /// Rebuilds warm state from cold state. Called after loading a snapshot.
    ///
    /// ## Internal Logic
    /// 1. Free existing warm state
    /// 2. Tap cold state to get all [battery, [bash registry]] pairs
    /// 3. Rebuild warm state via _cj_ream()
    /// 4. Ream all hank cache entries
    ///
    void
    u3j_ream(void);

    /// @brief Extract cold state for persistence
    ///
    /// Returns a list of all cold state entries for checkpointing.
    ///
    /// @return List of [key value] pairs from cod_p
    ///
    u3_noun
    u3j_stay(void);

    /// @brief Inject cold state from persistence
    ///
    /// Loads cold state from a checkpoint.
    ///
    /// @param rel List of [key value] pairs to load
    ///
    void
    u3j_load(u3_noun rel);

    /// @brief Promote jet state from junior road
    ///
    /// Integrates jet state from a child road into the current road.
    ///
    /// @param jed_u Jet state to promote
    ///
    void
    u3j_reap(u3a_jets jed_u);

    /// @brief Copy junior jet state
    ///
    /// Creates a copy of jet state suitable for promotion.
    ///
    /// @param jed_u Jet state to copy
    /// @return Copied jet state
    ///
    u3a_jets
    u3j_take(u3a_jets jed_u);

    /// @brief Register core with rite caching
    ///
    /// Like u3j_mine but uses a rite cache to avoid re-mining.
    ///
    /// @param rit_u Rite cache
    /// @param clu Clue (TRANSFER)
    /// @param cor Core (TRANSFER)
    ///
    void
    u3j_rite_mine(u3j_rite* rit_u, u3_noun clu, u3_noun cor);

    /// @brief Copy junior rite references
    ///
    /// @param dst_u Destination (uninitialized)
    /// @param src_u Source
    ///
    void
    u3j_rite_take(u3j_rite* dst_u, u3j_rite* src_u);

    /// @brief Merge rite references, losing old
    ///
    /// @param dst_u Destination (losing old references)
    /// @param src_u Source
    ///
    void
    u3j_rite_merge(u3j_rite* dst_u, u3j_rite* src_u);

    /// @brief Take and merge rite references
    ///
    void
    u3j_rite_take_merge(u3j_rite* dst_u, u3j_rite* src_u);

    /// @brief Copy junior site references
    ///
    /// @param dst_u Destination (uninitialized)
    /// @param src_u Source
    ///
    void
    u3j_site_take(u3j_site* dst_u, u3j_site* src_u);

    /// @brief Merge site references, losing old
    ///
    /// @param dst_u Destination (losing old references)
    /// @param src_u Source
    ///
    void
    u3j_site_merge(u3j_site* dst_u, u3j_site* src_u);

    /// @brief Take and merge site references
    ///
    void
    u3j_site_take_merge(u3j_site* dst_u, u3j_site* src_u);

    /// @brief Refresh site after checkpoint restore
    ///
    /// Re-resolves jet pointers from location.
    ///
    /// @param sit_u Site to refresh
    ///
    void
    u3j_site_ream(u3j_site* sit_u);

    /// @brief Kick a core with site cache
    ///
    /// Uses cached site information to accelerate repeated kicks.
    ///
    /// @param cor Core (behavior varies - see u3j_kick)
    /// @param sit_u Site cache
    /// @return Result or u3_none
    ///
    u3_weak
    u3j_site_kick(u3_noun cor, u3j_site* sit_u);

    /// @brief Prepare gate for repeated calls
    ///
    /// Sets up a site cache for a gate that will be called repeatedly
    /// with different samples.
    ///
    /// @param sit_u Site to prepare (output)
    /// @param cor Gate (TRANSFER - stored in sit_u->bat)
    ///
    void
    u3j_gate_prep(u3j_site* sit_u, u3_noun cor);

    /// @brief Slam prepared gate with sample
    ///
    /// @param sit_u Prepared site
    /// @param sam Sample (TRANSFER)
    /// @return Result
    ///
    u3_noun
    u3j_gate_slam(u3j_site* sit_u, u3_noun sam);

    /// @brief Clean up prepared gate site
    ///
    /// @param sit_u Site to clean up
    ///
    void
    u3j_gate_lose(u3j_site* sit_u);

    /// @brief Mark rite for GC
    ///
    /// @param rit_u Rite to mark
    /// @return Words marked
    ///
    c3_w
    u3j_rite_mark(u3j_rite* rit_u);

    /// @brief Lose rite references (but don't free struct)
    ///
    /// @param rit_u Rite to lose
    ///
    void
    u3j_rite_lose(u3j_rite* rit_u);

    /// @brief Lose site references (but don't free struct)
    ///
    /// @param sit_u Site to lose
    ///
    void
    u3j_site_lose(u3j_site* sit_u);

    /// @brief Mark site for GC
    ///
    /// @param sit_u Site to mark
    /// @return Words marked
    ///
    c3_w
    u3j_site_mark(u3j_site* sit_u);

    /// @brief Mark all jet state for GC
    ///
    /// @return Quac tree of marked memory
    ///
    u3m_quac*
    u3j_mark();

    /// @brief Free all jet state
    ///
    void
    u3j_free(void);

    /// @brief Free a hank cache entry
    ///
    /// @param ite_u Hash table item containing hank
    ///
    void
    u3j_free_hank(u3h_item* ite_u);

    /// @brief Clear caches to reclaim memory
    ///
    /// Clears the hank cache. Called during memory pressure.
    ///
    void
    u3j_reclaim(void);

    /// @brief Rewrite jet state for loom compaction
    ///
    /// Relocates all hash tables after loom addresses change.
    ///
    void
    u3j_rewrite_compact(void);

#endif /* ifndef U3_JETS_H */
