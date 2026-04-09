/// @file gashtable.h
/// Generic HAMT (hash array mapped trie) template
///
/// This is a template-based hashtable for noun keys (and optionally values),
/// following the verstable.h pattern but using the hashtable.h HAMT structure.
///
/// Usage (Set mode - stores u3_post keys):
///   #define NAME   my_set
///   #define HASH_FN(key) u3r_mug(key)
///   #define CMPR_FN(a, b) u3r_sing(a, b)
///   #include "gashtable.h"
///
/// Usage (Map mode - stores ga_item with key/val):
///   #define NAME   my_map
///   #define HASH_FN(key) u3r_mug(key)
///   #define CMPR_FN(a, b) u3r_sing(a, b)
///   #define VAL_TY u3_weak           // enables map mode, del returns value
///   #include "gashtable.h"
///
/// Required macros:
///   NAME    - prefix for generated types and functions
///   HASH_FN - macro/function that hashes a key (u3_post), returns c3_w
///   CMPR_FN - macro/function that compares two keys (u3_post), returns c3_o
///
/// Optional macros:
///   VAL_TY             - value type (enables map mode where del returns value)
///                        Item type is u3_post for sets, ga_item for maps
///   KEY_DTOR_FN(item)  - destructor for key (frees key resources)
///   VAL_DTOR_FN(item)  - destructor for value (maps only, frees val resources)
///   HEADER_MODE        - only emit declarations
///   IMPLEMENTATION_MODE - only emit definitions
///
/// GC macros (optional, enable mark/count/relocate/discount functions):
///   KEY_MARK(key)        - mark function for key, returns c3_w
///   VAL_MARK(val)        - mark function for value, returns c3_w (maps only)
///   KEY_COUNT(key)       - count function for key, returns c3_w
///   VAL_COUNT(val)       - count function for value, returns c3_w (maps only)
///   KEY_RELOCATE(key_ptr) - relocate function for key (takes pointer to key)
///   VAL_RELOCATE(val_ptr) - relocate function for value (maps only)
///   KEY_DISCOUNT(key)    - discount function for key, returns c3_w
///   VAL_DISCOUNT(val)    - discount function for value, returns c3_w (maps only)

#ifndef GASHTABLE_COMMON_H
#define GASHTABLE_COMMON_H

#include "c3/c3.h"
#include "types.h"

// Two-way concatenation macro
#define GA_CAT_(a, b) a##b
#define GA_CAT(a, b) GA_CAT_(a, b)

/// Slot storage: raw noun or pointer, no encoding.
/// Type (node vs item) and warm/cold tracked via separate bitmaps in each node.
typedef c3_w ga_slot;

// Bitmap helpers for 32-bit (nodes/buckets)
#define ga_bit_get(map, i)     (((map) >> (i)) & 1)
#define ga_bit_set(map, i)     ((map) | ((c3_w)1 << (i)))
#define ga_bit_clr(map, i)     ((map) & ~((c3_w)1 << (i)))

// Bitmap helpers for 64-bit (root)
#define ga_bit_get_d(map, i)   (((map) >> (i)) & 1)
#define ga_bit_set_d(map, i)   ((map) | ((c3_d)1 << (i)))
#define ga_bit_clr_d(map, i)   ((map) & ~((c3_d)1 << (i)))

// Helper macros for bit manipulation
#define GA_CUT_END(a_w, b_w) ((a_w) & (((c3_w)1 << (b_w)) - 1))
#define GA_BIT_SET(a_w, b_w) ((a_w) & ((c3_w)1 << (b_w)))

/// Node in the HAMT (common type, doesn't depend on ITEM_TY)
typedef struct ga_node {
  c3_w    map_w;    // bitmap for which slots are filled
  c3_w    nod_w;    // bitmap: 1 = slot is node/bucket, 0 = slot is item
  c3_w    wrm_w;    // bitmap: 1 = item is warm, 0 = item is cold
  ga_slot sot_w[];  // filled slots (raw pointers/nouns)
} ga_node;

/// Bucket at the bottom of the HAMT (common type)
typedef struct ga_buck {
  c3_w    len_w;    // length of [sot_w]
  c3_w    wrm_w;    // bitmap: 1 = item is warm, 0 = item is cold
  ga_slot sot_w[];  // filled slots (raw pointers/nouns)
} ga_buck;

/// Root hash table (common type)
typedef struct ga_root {
  c3_w    max_w;     // number of cache lines (0 for no trimming)
  c3_w    use_w;     // number of entries currently filled
  struct {
    c3_w  mug_w;     // current hash
    c3_w  inx_w;     // index into current bucket
    c3_o  buc_o;
  } arm_u;           // clock arm for trimming
  c3_d    nod_d;     // bitmap: 1 = slot is node/bucket, 0 = slot is item
  c3_d    wrm_d;     // bitmap: 1 = item is warm, 0 = item is cold
  ga_slot sot_w[64]; // slots (raw pointers/nouns)
} ga_root;

/// Item for maps (key-value pair)
typedef struct ga_item {
  u3_post key;
  u3_post val;
} ga_item;

#endif // GASHTABLE_COMMON_H

//////////////////////////////////////////////////////////////////////////////
// Common function implementations (independent of ITEM_TY)
//////////////////////////////////////////////////////////////////////////////

#ifdef GASHTABLE_COMMON_IMPL
#ifndef GASHTABLE_IMPL_H
#define GASHTABLE_IMPL_H

static c3_w
ga_popcount(c3_w num_w)
{
  return c3_pc_w(num_w);
}

static ga_buck*
ga_buck_new(c3_w len_w)
{
  ga_buck* hab_u = u3a_walloc(c3_wiseof(ga_buck) +
                              (len_w * c3_wiseof(ga_slot)));
  hab_u->len_w = len_w;
  hab_u->wrm_w = 0;
  return hab_u;
}

static ga_node*
ga_node_new(c3_w len_w)
{
  ga_node* han_u = u3a_walloc(c3_wiseof(ga_node) +
                              (len_w * c3_wiseof(ga_slot)));
  han_u->map_w = 0;
  han_u->nod_w = 0;
  han_u->wrm_w = 0;
  return han_u;
}

static u3p(ga_root)
ga_new_cache(c3_w max_w)
{
  ga_root* har_u = u3a_walloc(c3_wiseof(ga_root));
  u3p(ga_root) har_p = u3of(ga_root, har_u);
  c3_w i_w;

  har_u->max_w = max_w;
  har_u->use_w = 0;
  har_u->arm_u.mug_w = 0;
  har_u->arm_u.inx_w = 0;
  har_u->nod_d = 0;
  har_u->wrm_d = 0;

  for (i_w = 0; i_w < 64; i_w++) {
    har_u->sot_w[i_w] = 0;
  }
  return har_p;
}

static u3p(ga_root)
ga_new(void)
{
  return ga_new_cache(0);
}

static c3_w
ga_skip_slot(c3_w mug_w, c3_w lef_w)
{
  c3_w hig_w = mug_w >> lef_w;
  c3_w new_w = GA_CUT_END(hig_w + 1, (31 - lef_w));
  return new_w << lef_w;
}

// Create a new node/bucket with two items underneath.
// Returns the raw pointer (caller must set nod bit for their slot).
// had_w/add_w are item slots, wrm_had/wrm_add indicate if they're warm.
// If add_p is non-NULL, sets *add_p to point to where add_w was stored.
static void*
ga_two(ga_slot had_w, c3_o wrm_had, ga_slot add_w, c3_o wrm_add,
       c3_w lef_w, c3_w ham_w, c3_w mad_w, ga_slot** add_p)
{
  if (0 == lef_w) {
    // Create bucket with two items
    ga_buck* hab_u = ga_buck_new(2);
    hab_u->sot_w[0] = had_w;
    hab_u->sot_w[1] = add_w;
    hab_u->wrm_w = (_(wrm_had) ? 1 : 0) | (_(wrm_add) ? 2 : 0);
    if ( add_p ) *add_p = &hab_u->sot_w[1];
    return hab_u;
  }
  else {
    c3_w hop_w, tad_w;
    lef_w -= 5;
    hop_w = ham_w >> lef_w;
    tad_w = mad_w >> lef_w;
    if (hop_w == tad_w) {
      // Same bucket, recurse
      ga_node* han_u = ga_node_new(1);
      han_u->map_w = (c3_w)1 << hop_w;
      ham_w = GA_CUT_END(ham_w, lef_w);
      mad_w = GA_CUT_END(mad_w, lef_w);
      han_u->sot_w[0] = u3a_outa(ga_two(had_w, wrm_had, add_w, wrm_add,
                                        lef_w, ham_w, mad_w, add_p));
      han_u->nod_w = 1;  // slot 0 is a node
      return han_u;
    }
    else {
      // Different buckets, two items
      ga_node* han_u = ga_node_new(2);
      han_u->map_w = ((c3_w)1 << hop_w) | ((c3_w)1 << tad_w);
      // nod_w stays 0 (both are items)
      if (hop_w < tad_w) {
        han_u->sot_w[0] = had_w;
        han_u->sot_w[1] = add_w;
        han_u->wrm_w = (_(wrm_had) ? 1 : 0) | (_(wrm_add) ? 2 : 0);
        if ( add_p ) *add_p = &han_u->sot_w[1];
      }
      else {
        han_u->sot_w[0] = add_w;
        han_u->sot_w[1] = had_w;
        han_u->wrm_w = (_(wrm_add) ? 1 : 0) | (_(wrm_had) ? 2 : 0);
        if ( add_p ) *add_p = &han_u->sot_w[0];
      }
      return han_u;
    }
  }
}

#undef GASHTABLE_COMMON_IMPL
#endif // GASHTABLE_IMPL_H
#else // GASHTABLE_COMMON_IMPL

//////////////////////////////////////////////////////////////////////////////
// Template instantiation begins here
//////////////////////////////////////////////////////////////////////////////

#if !defined(NAME)
#error "NAME must be defined before including gashtable.h"
#endif

#if !defined(HASH_FN)
#error "HASH_FN must be defined before including gashtable.h"
#endif

#if !defined(CMPR_FN)
#error "CMPR_FN must be defined before including gashtable.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// Auto-define ITEM_TY based on VAL_TY
//////////////////////////////////////////////////////////////////////////////

#ifdef VAL_TY
// Map mode: item is ga_item struct with key and val
// Slot stores raw pointer to allocated ga_item struct (no encoding)
#define ITEM_TY ga_item
#define GA_ITEM_KEY(item) ((item)->key)
#define GA_ITEM_VAL(item) ((item)->val)
#define GA_ITEM_KEY_PTR(item) (&(item)->key)
#define GA_ITEM_VAL_PTR(item) (&(item)->val)
#define ga_slot_to_item(sot) u3to(ITEM_TY, (sot))
#define ga_item_to_slot(ptr) u3of(ITEM_TY, (ptr))
#else
// Set mode: item is noun stored directly in slot (no encoding)
// For sets, the "item" is the slot value itself (a noun)
// ITEM_TY* is ga_slot* which points to the slot containing the noun
#define ITEM_TY ga_slot
#define GA_ITEM_KEY(item) (*(item))
#define GA_ITEM_KEY_PTR(item) (item)
// ga_slot_to_item: convert slot value to item value (for sets, identity)
#define ga_slot_to_item(sot) (sot)
#define ga_item_to_slot(noun) ((ga_slot)(noun))
#endif

//////////////////////////////////////////////////////////////////////////////
// Template type aliases
//////////////////////////////////////////////////////////////////////////////

#ifndef IMPLEMENTATION_MODE

typedef ga_node GA_CAT(NAME, _node);
typedef ga_buck GA_CAT(NAME, _buck);
typedef ga_root GA_CAT(NAME, _root);
#ifdef VAL_TY
typedef ga_item GA_CAT(NAME, _item);
#endif

#endif // !IMPLEMENTATION_MODE

//////////////////////////////////////////////////////////////////////////////
// Function declarations
//////////////////////////////////////////////////////////////////////////////

#if defined(HEADER_MODE) || defined(IMPLEMENTATION_MODE)
#define GA_API_FN_QUALIFIERS
#else
#define GA_API_FN_QUALIFIERS static inline
#endif

// Return type and helpers differ between sets and maps
// Note: warm/cold status is now tracked in bitmaps, not encoded in slot values
#ifdef VAL_TY
// Maps: return pointer to allocated ga_item struct
#define GA_ITEM_REF ITEM_TY*
#define GA_ITEM_REF_NONE NULL
#define GA_OUT_TY ITEM_TY**
// Type for trim/free operations (what gets passed to _item_free)
#define GA_FREE_TY ITEM_TY*
#define GA_FREE_NONE NULL
#define GA_FREE_IS_NONE(x) ((x) == NULL)
// Get key from slot value
#define GA_SLOT_KEY(sot) GA_ITEM_KEY(ga_slot_to_item(sot))
// Set output to existing item in slot
#define GA_OUT_SET(out, slot_ptr) do { *(out) = ga_slot_to_item(*(slot_ptr)); } while(0)
// Create new item and set output (caller must set wrm bitmap bit)
#define GA_OUT_NEW(out, slot_ptr, key) do { \
  *(out) = GA_CAT(NAME, _item_new)(); \
  *(slot_ptr) = ga_item_to_slot(*(out)); \
} while(0)
// Convert slot pointer to item ref (for get returns)
#define GA_SLOT_PTR_TO_REF(slot_ptr) ga_slot_to_item(*(slot_ptr))
#else
// Sets: return pointer to slot containing the noun
#define GA_ITEM_REF ga_slot*
#define GA_ITEM_REF_NONE NULL
#define GA_OUT_TY ga_slot**
// Type for trim/free operations (the noun itself)
#define GA_FREE_TY ITEM_TY
#define GA_FREE_NONE u3_none
#define GA_FREE_IS_NONE(x) ((x) == u3_none)
// Get key from slot value (noun stored directly)
#define GA_SLOT_KEY(sot) ga_slot_to_item(sot)
// Set output to pointer to existing slot
#define GA_OUT_SET(out, slot_ptr) do { *(out) = (slot_ptr); } while(0)
// Initialize new slot to u3_none (caller stores key); set output to slot pointer
#define GA_OUT_NEW(out, slot_ptr, key) do { \
  (void)(key); \
  *(slot_ptr) = u3_none; \
  *(out) = (slot_ptr); \
} while(0)
// Convert slot pointer to item ref (for get returns) - just the slot pointer
#define GA_SLOT_PTR_TO_REF(slot_ptr) (slot_ptr)
#endif

#ifndef IMPLEMENTATION_MODE

/// Create a new hash table with bounded size (for caching)
GA_API_FN_QUALIFIERS u3p(GA_CAT(NAME, _root))
GA_CAT(NAME, _new_cache)(c3_w max_w);

/// Create a new hash table (unbounded)
GA_API_FN_QUALIFIERS u3p(GA_CAT(NAME, _root))
GA_CAT(NAME, _new)(void);

/// Get or create an item slot for the given key.
/// For maps: returns pointer to ga_item (check if item->key == u3_none for new)
/// For sets: returns pointer to slot (check if *slot == 0 for new, then write with ga_item_to_slot)
GA_API_FN_QUALIFIERS GA_ITEM_REF
GA_CAT(NAME, _put_get_ptr)(u3p(GA_CAT(NAME, _root)) har_p, u3_post key);

/// Insert into hash table. Ownership of key (and val for maps) is transferred.
/// For maps: replaces existing entry if present (old key/val freed via DTOR).
/// For sets: if key already present, the duplicate is freed via KEY_DTOR_FN.
#ifdef VAL_TY
GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _put)(u3p(GA_CAT(NAME, _root)) har_p, u3_post key, VAL_TY val);
#else
GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _put)(u3p(GA_CAT(NAME, _root)) har_p, u3_post key);
#endif

/// Get from hash table (no refcount bump).
/// For maps: returns value, or u3_none if not found.
/// For sets: returns key, or u3_none if not found.
GA_API_FN_QUALIFIERS u3_weak
GA_CAT(NAME, _git)(u3p(GA_CAT(NAME, _root)) har_p, u3_post key);

/// Get from hash table (with refcount bump via KEY_K).
/// For maps: returns value, or u3_none if not found.
/// For sets: returns key, or u3_none if not found.
GA_API_FN_QUALIFIERS u3_weak
GA_CAT(NAME, _get)(u3p(GA_CAT(NAME, _root)) har_p, u3_post key);

/// Delete an item from the hash table.
/// For sets: returns c3y if found and deleted, c3n otherwise.
/// For maps: returns the deleted value, or u3_none if not found.
#ifdef VAL_TY
GA_API_FN_QUALIFIERS VAL_TY
#else
GA_API_FN_QUALIFIERS c3_o
#endif
GA_CAT(NAME, _del)(u3p(GA_CAT(NAME, _root)) har_p, u3_post key);

/// Free the entire hash table
GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _free)(u3p(GA_CAT(NAME, _root)) har_p);

#ifdef KEY_MARK
/// Mark hashtable for gc (requires KEY_MARK, optionally VAL_MARK)
GA_API_FN_QUALIFIERS c3_w
GA_CAT(NAME, _mark)(u3p(GA_CAT(NAME, _root)) har_p);
#endif

#ifdef KEY_RELOCATE
/// Relocate hashtable for compaction (requires KEY_RELOCATE, optionally VAL_RELOCATE)
GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _relocate)(u3p(GA_CAT(NAME, _root))* har_p);
#endif

#ifdef KEY_COUNT
/// Count hashtable for gc (requires KEY_COUNT, optionally VAL_COUNT)
GA_API_FN_QUALIFIERS c3_w
GA_CAT(NAME, _count)(u3p(GA_CAT(NAME, _root)) har_p);
#endif

#ifdef KEY_DISCOUNT
/// Discount hashtable for gc (requires KEY_DISCOUNT, optionally VAL_DISCOUNT)
GA_API_FN_QUALIFIERS c3_w
GA_CAT(NAME, _discount)(u3p(GA_CAT(NAME, _root)) har_p);
#endif

/// Walk the hash table, calling fun_f on each item
/// For maps: fun_f receives ga_item*
/// For sets: fun_f receives ga_slot* (decode with ga_slot_to_item)
GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _walk)(u3p(GA_CAT(NAME, _root)) har_p,
                    void (*fun_f)(GA_ITEM_REF));

/// Walk with additional data argument
GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _walk_with)(u3p(GA_CAT(NAME, _root)) har_p,
                         void (*fun_f)(GA_ITEM_REF, void*),
                         void* wit);

/// Get number of entries
GA_API_FN_QUALIFIERS c3_w
GA_CAT(NAME, _wyt)(u3p(GA_CAT(NAME, _root)) har_p);

/// Trim to n entries
GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _trim_to)(u3p(GA_CAT(NAME, _root)) har_p, c3_w n_w);

/// Unify hashtables, copying [rah_p] into [har_p]
GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _uni)(u3p(GA_CAT(NAME, _root)) har_p, u3p(GA_CAT(NAME, _root)) rah_p);

/// Unify hashtables with take semantics (for cross-road copy)
GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _take_uni)(u3p(GA_CAT(NAME, _root)) har_p, u3p(GA_CAT(NAME, _root)) rah_p);

#endif // !IMPLEMENTATION_MODE

//////////////////////////////////////////////////////////////////////////////
// Template function implementations
//////////////////////////////////////////////////////////////////////////////

#ifndef HEADER_MODE
#include "allocate.h"

#ifdef VAL_TY
// Map mode: allocate ga_item struct
static ITEM_TY*
GA_CAT(NAME, _item_new)(void)
{
  ITEM_TY* ite_u = u3a_walloc(c3_wiseof(ITEM_TY));
  ite_u->key = u3_none;  // Mark as uninitialized for _put check
  ite_u->val = u3_none;  // Mark as uninitialized for _put check
  return ite_u;
}
#endif

// Free item completely (key + val + struct for maps, just key for sets)
// Takes GA_FREE_TY: pointer for maps, noun value for sets
static void
GA_CAT(NAME, _item_free)(GA_FREE_TY ite_u)
{
#ifdef KEY_DTOR_FN
#ifdef VAL_TY
  KEY_DTOR_FN(GA_ITEM_KEY(ite_u));
#else
  KEY_DTOR_FN(ite_u);  // ite_u is the noun directly in set mode
#endif
#endif
#ifdef VAL_DTOR_FN
  VAL_DTOR_FN(GA_ITEM_VAL(ite_u));
#endif
#ifdef VAL_TY
  u3a_wfree(ite_u);
#endif
}

// Free item for delete (key + struct only, val is returned)
// Takes GA_FREE_TY: pointer for maps, noun value for sets
static void
GA_CAT(NAME, _item_free_del)(GA_FREE_TY ite_u)
{
#ifdef KEY_DTOR_FN
#ifdef VAL_TY
  KEY_DTOR_FN(GA_ITEM_KEY(ite_u));
#else
  KEY_DTOR_FN(ite_u);  // ite_u is the noun directly in set mode
#endif
#endif
#ifdef VAL_TY
  u3a_wfree(ite_u);
#endif
}

// Forward declarations for internal functions
static void GA_CAT(NAME, _slot_put)(ga_slot*, u3_post, GA_OUT_TY, c3_w, c3_w, c3_w*);
static GA_FREE_TY GA_CAT(NAME, _trim_root)(ga_root*);

GA_API_FN_QUALIFIERS u3p(GA_CAT(NAME, _root))
GA_CAT(NAME, _new_cache)(c3_w max_w)
{
  return ga_new_cache(max_w);
}

GA_API_FN_QUALIFIERS u3p(GA_CAT(NAME, _root))
GA_CAT(NAME, _new)(void)
{
  return ga_new();
}

// Forward declaration for _slot_put_node
static void GA_CAT(NAME, _slot_put_node)(ga_node* han_u, c3_w inx_w,
                                         u3_post key, GA_OUT_TY out_u,
                                         c3_w lef_w, c3_w rem_w, c3_w* use_w);

static ga_node*
GA_CAT(NAME, _node_add)(ga_node* han_u,
                        c3_w lef_w, c3_w rem_w,
                        u3_post key, GA_OUT_TY out_u, c3_w* use_w)
{
  c3_w bit_w, inx_w, map_w, i_w;

  lef_w -= 5;
  bit_w = (rem_w >> lef_w);
  rem_w = GA_CUT_END(rem_w, lef_w);
  map_w = han_u->map_w;
  inx_w = ga_popcount(GA_CUT_END(map_w, bit_w));

  if (GA_BIT_SET(map_w, bit_w)) {
    // Slot exists, recurse into it
    GA_CAT(NAME, _slot_put_node)(han_u, inx_w, key, out_u, lef_w, rem_w, use_w);
    return han_u;
  }
  else {
    // Need to add new slot
    c3_w len_w = ga_popcount(map_w);
    ga_node* nah_u = ga_node_new(1 + len_w);
    nah_u->map_w = han_u->map_w | ((c3_w)1 << bit_w);

    // Copy slots before insertion point, shifting bitmap bits
    for (i_w = 0; i_w < inx_w; i_w++) {
      nah_u->sot_w[i_w] = han_u->sot_w[i_w];
      if (ga_bit_get(han_u->nod_w, i_w)) {
        nah_u->nod_w = ga_bit_set(nah_u->nod_w, i_w);
      }
      if (ga_bit_get(han_u->wrm_w, i_w)) {
        nah_u->wrm_w = ga_bit_set(nah_u->wrm_w, i_w);
      }
    }
    // Insert new item at inx_w (not a node, mark as warm)
    GA_OUT_NEW(out_u, &(nah_u->sot_w[inx_w]), key);
    nah_u->wrm_w = ga_bit_set(nah_u->wrm_w, inx_w);
    // Copy slots after insertion point, shifting bitmap bits
    for (i_w = inx_w; i_w < len_w; i_w++) {
      nah_u->sot_w[i_w + 1] = han_u->sot_w[i_w];
      if (ga_bit_get(han_u->nod_w, i_w)) {
        nah_u->nod_w = ga_bit_set(nah_u->nod_w, i_w + 1);
      }
      if (ga_bit_get(han_u->wrm_w, i_w)) {
        nah_u->wrm_w = ga_bit_set(nah_u->wrm_w, i_w + 1);
      }
    }

    u3a_wfree(han_u);
    *use_w += 1;
    return nah_u;
  }
}

static ga_buck*
GA_CAT(NAME, _buck_add)(ga_buck* hab_u,
                        u3_post key, GA_OUT_TY out_u, c3_w* use_w)
{
  c3_w i_w;

  for (i_w = 0; i_w < hab_u->len_w; i_w++) {
    if (c3y == CMPR_FN(key, GA_SLOT_KEY(hab_u->sot_w[i_w]))) {
      GA_OUT_SET(out_u, &(hab_u->sot_w[i_w]));
      return hab_u;
    }
  }

  {
    ga_buck* bah_u = ga_buck_new(1 + hab_u->len_w);
    GA_OUT_NEW(out_u, &(bah_u->sot_w[0]), key);
    bah_u->wrm_w = ga_bit_set(bah_u->wrm_w, 0);  // new item is warm

    // Copy old items with their warm bits
    for (i_w = 0; i_w < hab_u->len_w; i_w++) {
      bah_u->sot_w[i_w + 1] = hab_u->sot_w[i_w];
      if (ga_bit_get(hab_u->wrm_w, i_w)) {
        bah_u->wrm_w = ga_bit_set(bah_u->wrm_w, i_w + 1);
      }
    }

    u3a_wfree(hab_u);
    *use_w += 1;
    return bah_u;
  }
}

static void*
GA_CAT(NAME, _some_add)(void* han_v, c3_o is_buck, c3_w lef_w, c3_w rem_w,
                        u3_post key, GA_OUT_TY out_u, c3_w* use_w)
{
  if (_(is_buck)) {
    return GA_CAT(NAME, _buck_add)((ga_buck*)han_v, key, out_u, use_w);
  }
  else {
    return GA_CAT(NAME, _node_add)((ga_node*)han_v, lef_w, rem_w, key, out_u, use_w);
  }
}

// Put into a slot within a node (has bitmap context)
static void
GA_CAT(NAME, _slot_put_node)(ga_node* han_u, c3_w inx_w,
                             u3_post key, GA_OUT_TY out_u,
                             c3_w lef_w, c3_w rem_w, c3_w* use_w)
{
  ga_slot* sot_w = &han_u->sot_w[inx_w];

  if (ga_bit_get(han_u->nod_w, inx_w)) {
    // Slot is a node/bucket, recurse
    c3_o is_buck = (0 == lef_w) ? c3y : c3n;
    void* hav_v = GA_CAT(NAME, _some_add)(u3a_into(*sot_w), is_buck,
                                          lef_w, rem_w, key, out_u, use_w);
    *sot_w = u3a_outa(hav_v);
  }
  else {
    // Slot is an item
    if (c3y == CMPR_FN(key, GA_SLOT_KEY(*sot_w))) {
      GA_OUT_SET(out_u, sot_w);
    }
    else {
      // Collision: create a new subtree with both items
      ga_slot old_w = *sot_w;
      c3_o wrm_old = ga_bit_get(han_u->wrm_w, inx_w) ? c3y : c3n;
#ifdef VAL_TY
      // Map mode: allocate item, its pointer remains valid after ga_two
      GA_OUT_NEW(out_u, sot_w, key);
      ga_slot add_w = *sot_w;
      c3_w ham_w = GA_CUT_END(HASH_FN(GA_SLOT_KEY(old_w)), lef_w);
      void* new_v = ga_two(old_w, wrm_old, add_w, c3y, lef_w, ham_w, rem_w, NULL);
#else
      // Set mode: u3_none placeholder, ga_two sets *out_u to the slot
      ga_slot add_w = u3_none;
      c3_w ham_w = GA_CUT_END(HASH_FN(GA_SLOT_KEY(old_w)), lef_w);
      void* new_v = ga_two(old_w, wrm_old, add_w, c3y, lef_w, ham_w, rem_w, out_u);
#endif
      *sot_w = u3a_outa(new_v);
      // Mark this slot as a node now
      han_u->nod_w = ga_bit_set(han_u->nod_w, inx_w);
      han_u->wrm_w = ga_bit_clr(han_u->wrm_w, inx_w);  // nodes don't have warm
      *use_w += 1;
    }
  }
}

// Trim functions - use bitmap context for warm/cold checks

// Forward declarations
static GA_FREE_TY GA_CAT(NAME, _trim_slot_node)(ga_root*, ga_node*, c3_w, c3_w, c3_w);

// Trim item in a bucket (bucket has wrm_w bitmap)
static GA_FREE_TY
GA_CAT(NAME, _trim_buck)(ga_root* har_u, ga_buck* hab_u, c3_w* parent_nod_p)
{
  c3_w i_w, len_w;

  for (len_w = hab_u->len_w;
       har_u->arm_u.inx_w < len_w;
       har_u->arm_u.inx_w += 1)
  {
    c3_w idx = har_u->arm_u.inx_w;
    if (ga_bit_get(hab_u->wrm_w, idx)) {
      // Item is warm, make it cold
      hab_u->wrm_w = ga_bit_clr(hab_u->wrm_w, idx);
    }
    else {
      // Item is cold, delete it
      GA_FREE_TY ret = ga_slot_to_item(hab_u->sot_w[idx]);
      if (2 == len_w) {
        // Bucket will have 1 item, collapse to parent
        c3_w other = (0 == idx) ? 1 : 0;
        // The parent slot will become an item (handled by caller)
        // For now just return the deleted item
        har_u->arm_u.inx_w = 0;
      }
      hab_u->len_w = --len_w;
      // Shift remaining items and their warm bits
      for (i_w = idx; i_w < len_w; ++i_w) {
        hab_u->sot_w[i_w] = hab_u->sot_w[i_w + 1];
        if (ga_bit_get(hab_u->wrm_w, i_w + 1)) {
          hab_u->wrm_w = ga_bit_set(hab_u->wrm_w, i_w);
        } else {
          hab_u->wrm_w = ga_bit_clr(hab_u->wrm_w, i_w);
        }
      }
      ++(har_u->arm_u.inx_w);
      return ret;
    }
  }

  har_u->arm_u.mug_w = (har_u->arm_u.mug_w + 1) & 0x7FFFFFFF;
  har_u->arm_u.inx_w = 0;
  return GA_FREE_NONE;
}

// Trim within a node
static GA_FREE_TY
GA_CAT(NAME, _trim_node)(ga_root* har_u, ga_node* han_u, c3_w lef_w, c3_w rem_w)
{
  c3_w bit_w, map_w, inx_w;

  lef_w -= 5;
  bit_w = (rem_w >> lef_w);
  map_w = han_u->map_w;

  if (!GA_BIT_SET(map_w, bit_w)) {
    har_u->arm_u.mug_w = ga_skip_slot(har_u->arm_u.mug_w, lef_w);
    return GA_FREE_NONE;
  }

  rem_w = GA_CUT_END(rem_w, lef_w);
  inx_w = ga_popcount(GA_CUT_END(map_w, bit_w));

  GA_FREE_TY ret = GA_CAT(NAME, _trim_slot_node)(har_u, han_u, inx_w, lef_w, rem_w);
  if (!GA_FREE_IS_NONE(ret) && (0 == han_u->sot_w[inx_w])) {
    c3_w i_w, len_w = ga_popcount(map_w);

    if (2 == len_w) {
      // Node will have 1 slot - check if we can collapse
      c3_w ken_w = (0 == inx_w) ? 1 : 0;
      if (!ga_bit_get(han_u->nod_w, ken_w)) {
        // Other slot is an item, will collapse (handled by caller)
      }
    }
    // Remove slot from node
    han_u->map_w &= ~(1 << bit_w);
    --len_w;
    for (i_w = inx_w; i_w < len_w; i_w++) {
      han_u->sot_w[i_w] = han_u->sot_w[i_w + 1];
      // Shift bitmap bits
      if (ga_bit_get(han_u->nod_w, i_w + 1)) {
        han_u->nod_w = ga_bit_set(han_u->nod_w, i_w);
      } else {
        han_u->nod_w = ga_bit_clr(han_u->nod_w, i_w);
      }
      if (ga_bit_get(han_u->wrm_w, i_w + 1)) {
        han_u->wrm_w = ga_bit_set(han_u->wrm_w, i_w);
      } else {
        han_u->wrm_w = ga_bit_clr(han_u->wrm_w, i_w);
      }
    }
  }
  return ret;
}

// Trim a slot within a node (has bitmap context)
static GA_FREE_TY
GA_CAT(NAME, _trim_slot_node)(ga_root* har_u, ga_node* han_u, c3_w inx_w,
                              c3_w lef_w, c3_w rem_w)
{
  if (ga_bit_get(han_u->nod_w, inx_w)) {
    // Slot is a node/bucket
    if (0 == lef_w) {
      return GA_CAT(NAME, _trim_buck)(har_u, u3a_into(han_u->sot_w[inx_w]), NULL);
    } else {
      return GA_CAT(NAME, _trim_node)(har_u, u3a_into(han_u->sot_w[inx_w]), lef_w, rem_w);
    }
  }
  else {
    // Slot is an item
    har_u->arm_u.mug_w = ga_skip_slot(har_u->arm_u.mug_w, lef_w);
    if (ga_bit_get(han_u->wrm_w, inx_w)) {
      // Item is warm, make it cold
      han_u->wrm_w = ga_bit_clr(han_u->wrm_w, inx_w);
      return GA_FREE_NONE;
    }
    else {
      // Item is cold, delete it
      GA_FREE_TY ret = ga_slot_to_item(han_u->sot_w[inx_w]);
      han_u->sot_w[inx_w] = 0;
      return ret;
    }
  }
}

// Trim from root level
static GA_FREE_TY
GA_CAT(NAME, _trim_root)(ga_root* har_u)
{
  c3_w mug_w = har_u->arm_u.mug_w;
  c3_w inx_w = mug_w >> 25;
  ga_slot sot_w = har_u->sot_w[inx_w];

  if (0 == sot_w) {
    har_u->arm_u.mug_w = ga_skip_slot(har_u->arm_u.mug_w, 25);
    return GA_FREE_NONE;
  }

  if (ga_bit_get_d(har_u->nod_d, inx_w)) {
    // Slot is a node
    c3_w rem_w = GA_CUT_END(mug_w, 25);
    return GA_CAT(NAME, _trim_node)(har_u, u3a_into(sot_w), 25, rem_w);
  }
  else {
    // Slot is an item
    har_u->arm_u.mug_w = ga_skip_slot(har_u->arm_u.mug_w, 25);
    if (ga_bit_get_d(har_u->wrm_d, inx_w)) {
      // Item is warm, make it cold
      har_u->wrm_d = ga_bit_clr_d(har_u->wrm_d, inx_w);
      return GA_FREE_NONE;
    }
    else {
      // Item is cold, delete it
      GA_FREE_TY ret = ga_slot_to_item(sot_w);
      har_u->sot_w[inx_w] = 0;
      return ret;
    }
  }
}

// Put into a slot at root level (uses 64-bit bitmaps)
static void
GA_CAT(NAME, _slot_put_root)(ga_root* har_u, c3_w inx_w,
                             u3_post key, GA_OUT_TY out_u,
                             c3_w rem_w)
{
  ga_slot* sot_w = &har_u->sot_w[inx_w];

  if (ga_bit_get_d(har_u->nod_d, inx_w)) {
    // Slot is a node, recurse
    void* hav_v = GA_CAT(NAME, _node_add)(u3a_into(*sot_w),
                                          25, rem_w, key, out_u,
                                          &har_u->use_w);
    *sot_w = u3a_outa(hav_v);
  }
  else {
    // Slot is an item
    if (c3y == CMPR_FN(key, GA_SLOT_KEY(*sot_w))) {
      GA_OUT_SET(out_u, sot_w);
    }
    else {
      // Collision: create a new subtree with both items
      ga_slot old_w = *sot_w;
      c3_o wrm_old = ga_bit_get_d(har_u->wrm_d, inx_w) ? c3y : c3n;
#ifdef VAL_TY
      // Map mode: allocate item, its pointer remains valid after ga_two
      GA_OUT_NEW(out_u, sot_w, key);
      ga_slot add_w = *sot_w;
      c3_w ham_w = GA_CUT_END(HASH_FN(GA_SLOT_KEY(old_w)), 25);
      void* new_v = ga_two(old_w, wrm_old, add_w, c3y, 25, ham_w, rem_w, NULL);
#else
      // Set mode: u3_none placeholder, ga_two sets *out_u to the slot
      ga_slot add_w = u3_none;
      c3_w ham_w = GA_CUT_END(HASH_FN(GA_SLOT_KEY(old_w)), 25);
      void* new_v = ga_two(old_w, wrm_old, add_w, c3y, 25, ham_w, rem_w, out_u);
#endif
      *sot_w = u3a_outa(new_v);
      // Mark this slot as a node now
      har_u->nod_d = ga_bit_set_d(har_u->nod_d, inx_w);
      har_u->wrm_d = ga_bit_clr_d(har_u->wrm_d, inx_w);
      har_u->use_w += 1;
    }
  }
}

GA_API_FN_QUALIFIERS GA_ITEM_REF
GA_CAT(NAME, _put_get_ptr)(u3p(GA_CAT(NAME, _root)) har_p, u3_post key)
{
  ga_root* har_u = u3to(ga_root, har_p);
  c3_w mug_w = HASH_FN(key);
  c3_w inx_w = (mug_w >> 25);
  c3_w rem_w = GA_CUT_END(mug_w, 25);
  ga_slot* sot_w = &(har_u->sot_w[inx_w]);
  GA_ITEM_REF out_u;

  if (0 == *sot_w) {
    // Empty slot, create new item
    GA_OUT_NEW(&out_u, sot_w, key);
    har_u->wrm_d = ga_bit_set_d(har_u->wrm_d, inx_w);  // mark as warm
    har_u->use_w += 1;
  }
  else {
    GA_CAT(NAME, _slot_put_root)(har_u, inx_w, key, &out_u, rem_w);
  }

  // Trim if over capacity
  if (har_u->max_w && (har_u->use_w > har_u->max_w)) {
    GA_FREE_TY del_u;
    do {
      del_u = GA_CAT(NAME, _trim_root)(har_u);
      if (!GA_FREE_IS_NONE(del_u)) {
        GA_CAT(NAME, _item_free)(del_u);
        har_u->use_w -= 1;
      }
    } while (GA_FREE_IS_NONE(del_u));
  }

  return out_u;
}

#ifdef VAL_TY
#define GA_PUT_EXTRA_PARAMS , VAL_TY val
#else
#define GA_PUT_EXTRA_PARAMS
#endif

GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _put)(u3p(GA_CAT(NAME, _root)) har_p, u3_post key GA_PUT_EXTRA_PARAMS)
{
  GA_ITEM_REF ref = GA_CAT(NAME, _put_get_ptr)(har_p, key);
#ifdef VAL_TY
  // Map mode: check if key was already set, free old values, store new
  if ( u3_none != GA_ITEM_KEY(ref) ) {
#ifdef KEY_DTOR_FN
    KEY_DTOR_FN(GA_ITEM_KEY(ref));
#endif
#ifdef VAL_DTOR_FN
    VAL_DTOR_FN(GA_ITEM_VAL(ref));
#endif
  }
  ref->key = KEY_K(key);
  ref->val = val;
#else
  // Set mode: put_get_ptr returns slot with u3_none for new, or existing key
  // If new (u3_none), store the key; if exists, free duplicate if DTOR defined
  if ( u3_none == *ref ) {
    *ref = ga_item_to_slot(key);
  }
  else {
#ifdef KEY_DTOR_FN
    KEY_DTOR_FN(key);  // key already exists, free the duplicate
#endif
  }
#endif
}

#undef GA_PUT_EXTRA_PARAMS

// Get functions - use bitmap context for type checks
static GA_ITEM_REF
GA_CAT(NAME, _buck_git)(ga_buck* hab_u, u3_post key)
{
  c3_w i_w;
  for (i_w = 0; i_w < hab_u->len_w; i_w++) {
    if (_(CMPR_FN(key, GA_SLOT_KEY(hab_u->sot_w[i_w])))) {
      return GA_SLOT_PTR_TO_REF(&(hab_u->sot_w[i_w]));
    }
  }
  return GA_ITEM_REF_NONE;
}

static GA_ITEM_REF
GA_CAT(NAME, _node_git)(ga_node* han_u, c3_w lef_w, c3_w rem_w, u3_post key)
{
  c3_w bit_w, map_w;

  lef_w -= 5;
  bit_w = (rem_w >> lef_w);
  rem_w = GA_CUT_END(rem_w, lef_w);
  map_w = han_u->map_w;

  if (!GA_BIT_SET(map_w, bit_w)) {
    return GA_ITEM_REF_NONE;
  }
  else {
    c3_w inx_w = ga_popcount(GA_CUT_END(map_w, bit_w));
    ga_slot sot_w = han_u->sot_w[inx_w];

    if (!ga_bit_get(han_u->nod_w, inx_w)) {
      // Slot is an item
      if (_(CMPR_FN(key, GA_SLOT_KEY(sot_w)))) {
        return GA_SLOT_PTR_TO_REF(&(han_u->sot_w[inx_w]));
      }
      else {
        return GA_ITEM_REF_NONE;
      }
    }
    else {
      // Slot is a node/bucket
      if (0 == lef_w) {
        return GA_CAT(NAME, _buck_git)(u3a_into(sot_w), key);
      }
      else {
        return GA_CAT(NAME, _node_git)(u3a_into(sot_w), lef_w, rem_w, key);
      }
    }
  }
}

// Internal get - returns item ref (used by _put for sets)
static GA_ITEM_REF
GA_CAT(NAME, _raw_git)(u3p(GA_CAT(NAME, _root)) har_p, u3_post key)
{
  ga_root* har_u = u3to(ga_root, har_p);
  c3_w mug_w = HASH_FN(key);
  c3_w inx_w = (mug_w >> 25);
  c3_w rem_w = GA_CUT_END(mug_w, 25);
  ga_slot sot_w = har_u->sot_w[inx_w];

  if (0 == sot_w) {
    return GA_ITEM_REF_NONE;
  }
  else if (!ga_bit_get_d(har_u->nod_d, inx_w)) {
    // Slot is an item
    if (_(CMPR_FN(key, GA_SLOT_KEY(sot_w)))) {
      return GA_SLOT_PTR_TO_REF(&(har_u->sot_w[inx_w]));
    }
    else {
      return GA_ITEM_REF_NONE;
    }
  }
  else {
    // Slot is a node
    return GA_CAT(NAME, _node_git)(u3a_into(sot_w), 25, rem_w, key);
  }
}

// Get without refcount bump
GA_API_FN_QUALIFIERS u3_weak
GA_CAT(NAME, _git)(u3p(GA_CAT(NAME, _root)) har_p, u3_post key)
{
  GA_ITEM_REF ref = GA_CAT(NAME, _raw_git)(har_p, key);
  if ( !ref ) {
    return u3_none;
  }
#ifdef VAL_TY
  return GA_ITEM_VAL(ref);
#else
  return ga_slot_to_item(*ref);
#endif
}

// Get with refcount bump
GA_API_FN_QUALIFIERS u3_weak
GA_CAT(NAME, _get)(u3p(GA_CAT(NAME, _root)) har_p, u3_post key)
{
  u3_weak got = GA_CAT(NAME, _git)(har_p, key);
  if ( u3_none != got ) {
    return KEY_K(got);
  }
  return u3_none;
}

// Delete functions - return type depends on VAL_TY being defined
#ifdef VAL_TY
#define GA_DEL_RET_TY VAL_TY
#define GA_DEL_NONE u3_none
#define GA_DEL_FOUND(ite_u) GA_ITEM_VAL(ite_u)
#else
#define GA_DEL_RET_TY c3_o
#define GA_DEL_NONE c3n
#define GA_DEL_FOUND(ite_u) c3y
#endif

// Delete from bucket - returns deleted value (or c3y for sets)
static GA_DEL_RET_TY
GA_CAT(NAME, _buck_del)(ga_buck* hab_u, u3_post key)
{
  c3_w fin_w = hab_u->len_w;
  c3_w i_w;
  GA_DEL_RET_TY ret = GA_DEL_NONE;

  for (i_w = 0; i_w < hab_u->len_w; i_w++) {
    if (c3y == CMPR_FN(key, GA_SLOT_KEY(hab_u->sot_w[i_w]))) {
      fin_w = i_w;
#ifdef VAL_TY
      ITEM_TY* ite_u = ga_slot_to_item(hab_u->sot_w[i_w]);
      ret = GA_ITEM_VAL(ite_u);
      GA_CAT(NAME, _item_free_del)(ite_u);
#else
      ret = c3y;
      GA_CAT(NAME, _item_free)(ga_slot_to_item(hab_u->sot_w[i_w]));
#endif
      break;
    }
  }

  if (fin_w == hab_u->len_w) {
    return ret;
  }

  hab_u->len_w--;
  // Shift remaining slots and warm bits
  for (i_w = fin_w; i_w < hab_u->len_w; i_w++) {
    hab_u->sot_w[i_w] = hab_u->sot_w[i_w + 1];
    if (ga_bit_get(hab_u->wrm_w, i_w + 1)) {
      hab_u->wrm_w = ga_bit_set(hab_u->wrm_w, i_w);
    } else {
      hab_u->wrm_w = ga_bit_clr(hab_u->wrm_w, i_w);
    }
  }

  return ret;
}

// Forward declaration
static GA_DEL_RET_TY GA_CAT(NAME, _slot_del_node)(ga_node*, c3_w, u3_post, c3_w, c3_w);

// Delete from node
static GA_DEL_RET_TY
GA_CAT(NAME, _node_del)(ga_node* han_u, u3_post key, c3_w lef_w, c3_w rem_w)
{
  c3_w bit_w, inx_w, map_w, i_w;

  lef_w -= 5;
  bit_w = (rem_w >> lef_w);
  rem_w = GA_CUT_END(rem_w, lef_w);
  map_w = han_u->map_w;

  if (!GA_BIT_SET(map_w, bit_w)) {
    return GA_DEL_NONE;
  }

  inx_w = ga_popcount(GA_CUT_END(map_w, bit_w));

  GA_DEL_RET_TY ret = GA_CAT(NAME, _slot_del_node)(han_u, inx_w, key, lef_w, rem_w);
#ifdef VAL_TY
  if (u3_none == ret) {
    return ret;
  }
#else
  if (c3n == ret) {
    return ret;
  }
#endif

  if (0 != han_u->sot_w[inx_w]) {
    return ret;
  }

  // Slot was cleared, need to compact node
  c3_w len_w = ga_popcount(map_w);

  if (2 == len_w) {
    // Node will have 1 slot - check if we can collapse (handled by caller)
    c3_w ken_w = (0 == inx_w) ? 1 : 0;
    if (!ga_bit_get(han_u->nod_w, ken_w)) {
      // Other slot is an item, could collapse (caller handles)
    }
  }

  // Remove slot from node
  han_u->map_w &= ~(1 << bit_w);
  --len_w;
  for (i_w = inx_w; i_w < len_w; i_w++) {
    han_u->sot_w[i_w] = han_u->sot_w[i_w + 1];
    if (ga_bit_get(han_u->nod_w, i_w + 1)) {
      han_u->nod_w = ga_bit_set(han_u->nod_w, i_w);
    } else {
      han_u->nod_w = ga_bit_clr(han_u->nod_w, i_w);
    }
    if (ga_bit_get(han_u->wrm_w, i_w + 1)) {
      han_u->wrm_w = ga_bit_set(han_u->wrm_w, i_w);
    } else {
      han_u->wrm_w = ga_bit_clr(han_u->wrm_w, i_w);
    }
  }

  return ret;
}

// Delete from a slot within a node (has bitmap context)
static GA_DEL_RET_TY
GA_CAT(NAME, _slot_del_node)(ga_node* han_u, c3_w inx_w, u3_post key,
                             c3_w lef_w, c3_w rem_w)
{
  ga_slot* sot_w = &han_u->sot_w[inx_w];

  if (!ga_bit_get(han_u->nod_w, inx_w)) {
    // Slot is an item
    if (c3y == CMPR_FN(key, GA_SLOT_KEY(*sot_w))) {
      GA_DEL_RET_TY ret;
#ifdef VAL_TY
      ITEM_TY* ite_u = ga_slot_to_item(*sot_w);
      ret = GA_ITEM_VAL(ite_u);
      GA_CAT(NAME, _item_free_del)(ite_u);
#else
      ret = c3y;
      GA_CAT(NAME, _item_free)(ga_slot_to_item(*sot_w));
#endif
      *sot_w = 0;
      return ret;
    }
    return GA_DEL_NONE;
  }
  else {
    // Slot is a node/bucket
    if (0 == lef_w) {
      return GA_CAT(NAME, _buck_del)(u3a_into(*sot_w), key);
    }
    return GA_CAT(NAME, _node_del)(u3a_into(*sot_w), key, lef_w, rem_w);
  }
}

#ifdef VAL_TY
GA_API_FN_QUALIFIERS VAL_TY
#else
GA_API_FN_QUALIFIERS c3_o
#endif
GA_CAT(NAME, _del)(u3p(GA_CAT(NAME, _root)) har_p, u3_post key)
{
  ga_root* har_u = u3to(ga_root, har_p);
  c3_w mug_w = HASH_FN(key);
  c3_w inx_w = (mug_w >> 25);
  c3_w rem_w = GA_CUT_END(mug_w, 25);
  ga_slot sot_w = har_u->sot_w[inx_w];

  if (0 == sot_w) {
    return GA_DEL_NONE;
  }

  GA_DEL_RET_TY ret;

  if (!ga_bit_get_d(har_u->nod_d, inx_w)) {
    // Slot is an item
    if (c3y == CMPR_FN(key, GA_SLOT_KEY(sot_w))) {
#ifdef VAL_TY
      ITEM_TY* ite_u = ga_slot_to_item(sot_w);
      ret = GA_ITEM_VAL(ite_u);
      GA_CAT(NAME, _item_free_del)(ite_u);
#else
      ret = c3y;
      GA_CAT(NAME, _item_free)(ga_slot_to_item(har_u->sot_w[inx_w]));
#endif
      har_u->sot_w[inx_w] = 0;
      har_u->wrm_d = ga_bit_clr_d(har_u->wrm_d, inx_w);
      har_u->use_w--;
      return ret;
    }
    return GA_DEL_NONE;
  }
  else {
    // Slot is a node
    ret = GA_CAT(NAME, _node_del)(u3a_into(sot_w), key, 25, rem_w);
#ifdef VAL_TY
    if (u3_none != ret) {
      har_u->use_w--;
    }
#else
    if (c3y == ret) {
      har_u->use_w--;
    }
#endif
    return ret;
  }
}

#undef GA_DEL_RET_TY
#undef GA_DEL_NONE
#undef GA_DEL_FOUND

// Free functions - use bitmap context
static void
GA_CAT(NAME, _free_buck)(ga_buck* hab_u)
{
  c3_w i_w;
  for (i_w = 0; i_w < hab_u->len_w; i_w++) {
    GA_CAT(NAME, _item_free)(ga_slot_to_item(hab_u->sot_w[i_w]));
  }
  u3a_wfree(hab_u);
}

static void
GA_CAT(NAME, _free_node)(ga_node* han_u, c3_w lef_w)
{
  c3_w len_w = ga_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for (i_w = 0; i_w < len_w; i_w++) {
    ga_slot sot_w = han_u->sot_w[i_w];
    if (0 == sot_w) {
      // empty (shouldn't happen in valid node)
    }
    else if (!ga_bit_get(han_u->nod_w, i_w)) {
      // slot is an item
      GA_CAT(NAME, _item_free)(ga_slot_to_item(han_u->sot_w[i_w]));
    }
    else {
      // slot is a node/bucket
      if (0 == lef_w) {
        GA_CAT(NAME, _free_buck)(u3a_into(sot_w));
      }
      else {
        GA_CAT(NAME, _free_node)(u3a_into(sot_w), lef_w);
      }
    }
  }
  u3a_wfree(han_u);
}

GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _free)(u3p(GA_CAT(NAME, _root)) har_p)
{
  ga_root* har_u = u3to(ga_root, har_p);
  c3_w i_w;

  for (i_w = 0; i_w < 64; i_w++) {
    ga_slot sot_w = har_u->sot_w[i_w];

    if (0 == sot_w) {
      // empty
    }
    else if (!ga_bit_get_d(har_u->nod_d, i_w)) {
      // slot is an item
      GA_CAT(NAME, _item_free)(ga_slot_to_item(har_u->sot_w[i_w]));
    }
    else {
      // slot is a node
      GA_CAT(NAME, _free_node)(u3a_into(sot_w), 25);
    }
  }
  u3a_wfree(har_u);
}

// Walk functions - use bitmap context
static void
GA_CAT(NAME, _walk_buck)(ga_buck* hab_u,
                         void (*fun_f)(GA_ITEM_REF, void*), void* wit)
{
  c3_w i_w;
  for (i_w = 0; i_w < hab_u->len_w; i_w++) {
    fun_f(GA_SLOT_PTR_TO_REF(&(hab_u->sot_w[i_w])), wit);
  }
}

static void
GA_CAT(NAME, _walk_node)(ga_node* han_u, c3_w lef_w,
                         void (*fun_f)(GA_ITEM_REF, void*), void* wit)
{
  c3_w len_w = ga_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for (i_w = 0; i_w < len_w; i_w++) {
    ga_slot sot_w = han_u->sot_w[i_w];

    if (!ga_bit_get(han_u->nod_w, i_w)) {
      // slot is an item
      fun_f(GA_SLOT_PTR_TO_REF(&(han_u->sot_w[i_w])), wit);
    }
    else {
      // slot is a node/bucket
      if (0 == lef_w) {
        GA_CAT(NAME, _walk_buck)(u3a_into(sot_w), fun_f, wit);
      }
      else {
        GA_CAT(NAME, _walk_node)(u3a_into(sot_w), lef_w, fun_f, wit);
      }
    }
  }
}

GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _walk_with)(u3p(GA_CAT(NAME, _root)) har_p,
                         void (*fun_f)(GA_ITEM_REF, void*),
                         void* wit)
{
  ga_root* har_u = u3to(ga_root, har_p);
  c3_w i_w;

  for (i_w = 0; i_w < 64; i_w++) {
    ga_slot sot_w = har_u->sot_w[i_w];

    if (0 == sot_w) {
      // empty
    }
    else if (!ga_bit_get_d(har_u->nod_d, i_w)) {
      // slot is an item
      fun_f(GA_SLOT_PTR_TO_REF(&(har_u->sot_w[i_w])), wit);
    }
    else {
      // slot is a node
      GA_CAT(NAME, _walk_node)(u3a_into(sot_w), 25, fun_f, wit);
    }
  }
}

static void
GA_CAT(NAME, _walk_plain)(GA_ITEM_REF ite_u, void* wit)
{
  void (*fun_f)(GA_ITEM_REF) = (void (*)(GA_ITEM_REF))wit;
  fun_f(ite_u);
}

GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _walk)(u3p(GA_CAT(NAME, _root)) har_p, void (*fun_f)(GA_ITEM_REF))
{
  GA_CAT(NAME, _walk_with)(har_p, GA_CAT(NAME, _walk_plain), (void*)fun_f);
}

GA_API_FN_QUALIFIERS c3_w
GA_CAT(NAME, _wyt)(u3p(GA_CAT(NAME, _root)) har_p)
{
  ga_root* har_u = u3to(ga_root, har_p);
  return har_u->use_w;
}

GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _trim_to)(u3p(GA_CAT(NAME, _root)) har_p, c3_w n_w)
{
  ga_root* har_u = u3to(ga_root, har_p);

  while (har_u->use_w > n_w) {
    GA_FREE_TY del_u = GA_CAT(NAME, _trim_root)(har_u);
    if (!GA_FREE_IS_NONE(del_u)) {
      har_u->use_w -= 1;
      GA_CAT(NAME, _item_free)(del_u);
    }
  }
}

// Callback for _uni
static void
GA_CAT(NAME, _uni_with)(GA_ITEM_REF ref, void* wit)
{
  u3p(GA_CAT(NAME, _root)) har_p = *(u3p(GA_CAT(NAME, _root))*)wit;
#ifdef VAL_TY
  GA_CAT(NAME, _put)(har_p, GA_ITEM_KEY(ref), KEY_K(GA_ITEM_VAL(ref)));
#else
  GA_CAT(NAME, _put)(har_p, ga_slot_to_item(*ref));
#endif
}

GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _uni)(u3p(GA_CAT(NAME, _root)) har_p, u3p(GA_CAT(NAME, _root)) rah_p)
{
  GA_CAT(NAME, _walk_with)(rah_p, GA_CAT(NAME, _uni_with), &har_p);
}

// Callback for _take_uni - directly uses put_get_ptr like hashtable.c
static void
GA_CAT(NAME, _take_uni_with)(GA_ITEM_REF ref, void* wit)
{
  u3p(GA_CAT(NAME, _root)) har_p = *(u3p(GA_CAT(NAME, _root))*)wit;
#ifdef VAL_TY
  // Map mode: take key, use put_get_ptr, then directly assign
  ref->key = u3a_take(ref->key);
  GA_ITEM_REF eti_u = GA_CAT(NAME, _put_get_ptr)(har_p, ref->key);
  if ( u3_none != eti_u->key ) {
#ifdef KEY_DTOR_FN
    KEY_DTOR_FN(eti_u->key);
#endif
#ifdef VAL_DTOR_FN
    VAL_DTOR_FN(eti_u->val);
#endif
  }
  eti_u->key = ref->key;
  eti_u->val = u3a_take(ref->val);
#else
  // Set mode: take key, use put_get_ptr to get slot, store if new
  u3_post key = u3a_take(ga_slot_to_item(*ref));
  GA_ITEM_REF sot_p = GA_CAT(NAME, _put_get_ptr)(har_p, key);
  if ( u3_none == *sot_p ) {
    // New slot, store the taken key
    *sot_p = ga_item_to_slot(key);
  }
  else {
    // Key already exists, free the duplicate
#ifdef KEY_DTOR_FN
    KEY_DTOR_FN(key);
#endif
  }
#endif
}

GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _take_uni)(u3p(GA_CAT(NAME, _root)) har_p, u3p(GA_CAT(NAME, _root)) rah_p)
{
  GA_CAT(NAME, _walk_with)(rah_p, GA_CAT(NAME, _take_uni_with), &har_p);
}

//////////////////////////////////////////////////////////////////////////////
// GC functions (mark, count, relocate, discount)
//////////////////////////////////////////////////////////////////////////////

#ifdef KEY_MARK

// Item mark: marks key (and val if VAL_MARK defined)
// For maps: also marks the allocated ga_item struct
// For sets: just marks the key (noun stored directly in slot)
static c3_w
GA_CAT(NAME, _mark_item)(ga_slot sot_w)
{
  c3_w tot_w = KEY_MARK(GA_SLOT_KEY(sot_w));
#ifdef VAL_TY
  ITEM_TY* ite_u = ga_slot_to_item(sot_w);
#ifdef VAL_MARK
  tot_w += VAL_MARK(GA_ITEM_VAL(ite_u));
#endif
  tot_w += u3a_mark_ptr(ite_u);
#endif
  return tot_w;
}

static c3_w
GA_CAT(NAME, _mark_buck)(ga_buck* hab_u)
{
  c3_w tot_w = 0;
  c3_w i_w;

  for (i_w = 0; i_w < hab_u->len_w; i_w++) {
    tot_w += GA_CAT(NAME, _mark_item)(hab_u->sot_w[i_w]);
  }
  tot_w += u3a_mark_ptr(hab_u);

  return tot_w;
}

static c3_w
GA_CAT(NAME, _mark_node)(ga_node* han_u, c3_w lef_w)
{
  c3_w tot_w = 0;
  c3_w len_w = ga_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for (i_w = 0; i_w < len_w; i_w++) {
    ga_slot sot_w = han_u->sot_w[i_w];

    if (!ga_bit_get(han_u->nod_w, i_w)) {
      // slot is an item
      tot_w += GA_CAT(NAME, _mark_item)(sot_w);
    }
    else {
      // slot is a node/bucket
      if (0 == lef_w) {
        tot_w += GA_CAT(NAME, _mark_buck)(u3a_into(sot_w));
      }
      else {
        tot_w += GA_CAT(NAME, _mark_node)(u3a_into(sot_w), lef_w);
      }
    }
  }

  tot_w += u3a_mark_ptr(han_u);

  return tot_w;
}

GA_API_FN_QUALIFIERS c3_w
GA_CAT(NAME, _mark)(u3p(GA_CAT(NAME, _root)) har_p)
{
  c3_w tot_w = 0;
  ga_root* har_u = u3to(ga_root, har_p);
  c3_w i_w;

  for (i_w = 0; i_w < 64; i_w++) {
    ga_slot sot_w = har_u->sot_w[i_w];

    if (0 == sot_w) {
      // empty
    }
    else if (!ga_bit_get_d(har_u->nod_d, i_w)) {
      // slot is an item
      tot_w += GA_CAT(NAME, _mark_item)(sot_w);
    }
    else {
      // slot is a node
      tot_w += GA_CAT(NAME, _mark_node)(u3a_into(sot_w), 25);
    }
  }

  tot_w += u3a_mark_ptr(har_u);

  return tot_w;
}

#endif // KEY_MARK

#ifdef KEY_RELOCATE

// Relocate item: relocates key (and val if VAL_RELOCATE defined)
// For maps: also relocates the allocated ga_item struct pointer
// For sets: just relocates the key (noun stored directly in slot)
static void
GA_CAT(NAME, _relocate_item)(ga_slot* sot_w)
{
#ifdef VAL_TY
  // Map mode: relocate the ga_item struct, then key and val inside
  ITEM_TY* ite_u = ga_slot_to_item(*sot_w);
  u3_post new_p, sot_p = u3a_outa(ite_u);
  c3_t fir_t;

  new_p = u3a_mark_relocate_post(sot_p, &fir_t);
  *sot_w = ga_item_to_slot(u3a_into(new_p));

  if (!fir_t) return;

  KEY_RELOCATE(GA_ITEM_KEY_PTR(ite_u));
#ifdef VAL_RELOCATE
  VAL_RELOCATE(GA_ITEM_VAL_PTR(ite_u));
#endif
#else
  // Set mode: just relocate the key noun stored in the slot
  u3_post key = ga_slot_to_item(*sot_w);
  KEY_RELOCATE(&key);
  *sot_w = ga_item_to_slot(key);
#endif
}

static void
GA_CAT(NAME, _relocate_buck)(ga_slot* sot_w)
{
  ga_buck* hab_u = u3a_into(*sot_w);
  u3_post new_p, sot_p = u3a_outa(hab_u);
  c3_t fir_t;

  new_p = u3a_mark_relocate_post(sot_p, &fir_t);
  *sot_w = u3a_outa(u3a_into(new_p));

  if (!fir_t) return;

  for (c3_w i_w = 0; i_w < hab_u->len_w; i_w++) {
    GA_CAT(NAME, _relocate_item)(&(hab_u->sot_w[i_w]));
  }
}

static void GA_CAT(NAME, _relocate_slot_node)(ga_node*, c3_w, c3_w);

static void
GA_CAT(NAME, _relocate_node)(ga_slot* sot_w, c3_w lef_w)
{
  ga_node* han_u = u3a_into(*sot_w);
  u3_post new_p, sot_p = u3a_outa(han_u);
  c3_w len_w;
  c3_t fir_t;

  new_p = u3a_mark_relocate_post(sot_p, &fir_t);
  *sot_w = u3a_outa(u3a_into(new_p));

  if (!fir_t) return;

  len_w = ga_popcount(han_u->map_w);
  lef_w -= 5;

  for (c3_w i_w = 0; i_w < len_w; i_w++) {
    GA_CAT(NAME, _relocate_slot_node)(han_u, i_w, lef_w);
  }
}

// Relocate a slot within a node (has bitmap context)
static void
GA_CAT(NAME, _relocate_slot_node)(ga_node* han_u, c3_w inx_w, c3_w lef_w)
{
  ga_slot* sot_w = &han_u->sot_w[inx_w];

  if (!ga_bit_get(han_u->nod_w, inx_w)) {
    // slot is an item
    GA_CAT(NAME, _relocate_item)(sot_w);
  }
  else if (!lef_w) {
    GA_CAT(NAME, _relocate_buck)(sot_w);
  }
  else {
    GA_CAT(NAME, _relocate_node)(sot_w, lef_w);
  }
}

// Relocate a slot within the root (uses 64-bit bitmap context)
static void
GA_CAT(NAME, _relocate_slot_root)(ga_root* har_u, c3_w inx_w)
{
  ga_slot* sot_w = &har_u->sot_w[inx_w];

  if (!ga_bit_get_d(har_u->nod_d, inx_w)) {
    // slot is an item
    GA_CAT(NAME, _relocate_item)(sot_w);
  }
  else {
    GA_CAT(NAME, _relocate_node)(sot_w, 25);
  }
}

GA_API_FN_QUALIFIERS void
GA_CAT(NAME, _relocate)(u3p(GA_CAT(NAME, _root))* har_p)
{
  u3_post new_p, old_p = *har_p;
  ga_root* har_u = u3to(ga_root, old_p);
  c3_w i_w;
  c3_t fir_t;

  new_p = u3a_mark_relocate_post(old_p, &fir_t);
  *har_p = new_p;

  if (!fir_t) return;

  for (i_w = 0; i_w < 64; i_w++) {
    if (0 != har_u->sot_w[i_w]) {
      GA_CAT(NAME, _relocate_slot_root)(har_u, i_w);
    }
  }
}

#endif // KEY_RELOCATE

#ifdef KEY_COUNT

// Item count: counts key (and val if VAL_COUNT defined)
// For maps: also counts the allocated ga_item struct
// For sets: just counts the key (noun stored directly in slot)
static c3_w
GA_CAT(NAME, _count_item)(ga_slot sot_w)
{
  c3_w tot_w = KEY_COUNT(GA_SLOT_KEY(sot_w));
#ifdef VAL_TY
  ITEM_TY* ite_u = ga_slot_to_item(sot_w);
#ifdef VAL_COUNT
  tot_w += VAL_COUNT(GA_ITEM_VAL(ite_u));
#endif
  tot_w += u3a_count_ptr(ite_u);
#endif
  return tot_w;
}

static c3_w
GA_CAT(NAME, _count_buck)(ga_buck* hab_u)
{
  c3_w tot_w = 0;
  c3_w i_w;

  for (i_w = 0; i_w < hab_u->len_w; i_w++) {
    tot_w += GA_CAT(NAME, _count_item)(hab_u->sot_w[i_w]);
  }
  tot_w += u3a_count_ptr(hab_u);

  return tot_w;
}

static c3_w
GA_CAT(NAME, _count_node)(ga_node* han_u, c3_w lef_w)
{
  c3_w tot_w = 0;
  c3_w len_w = ga_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for (i_w = 0; i_w < len_w; i_w++) {
    ga_slot sot_w = han_u->sot_w[i_w];

    if (!ga_bit_get(han_u->nod_w, i_w)) {
      tot_w += GA_CAT(NAME, _count_item)(sot_w);
    }
    else {
      void* hav_v = u3a_into(sot_w);

      if (0 == lef_w) {
        tot_w += GA_CAT(NAME, _count_buck)(hav_v);
      }
      else {
        tot_w += GA_CAT(NAME, _count_node)(hav_v, lef_w);
      }
    }
  }

  tot_w += u3a_count_ptr(han_u);

  return tot_w;
}

GA_API_FN_QUALIFIERS c3_w
GA_CAT(NAME, _count)(u3p(GA_CAT(NAME, _root)) har_p)
{
  c3_w tot_w = 0;
  ga_root* har_u = u3to(ga_root, har_p);
  c3_w i_w;

  for (i_w = 0; i_w < 64; i_w++) {
    ga_slot sot_w = har_u->sot_w[i_w];

    if (0 == sot_w) {
      continue;
    }
    else if (!ga_bit_get_d(har_u->nod_d, i_w)) {
      tot_w += GA_CAT(NAME, _count_item)(sot_w);
    }
    else {
      ga_node* han_u = u3a_into(sot_w);

      tot_w += GA_CAT(NAME, _count_node)(han_u, 25);
    }
  }

  tot_w += u3a_count_ptr(har_u);

  return tot_w;
}

#endif // KEY_COUNT

#ifdef KEY_DISCOUNT

// Item discount: discounts key (and val if VAL_DISCOUNT defined)
// For maps: also discounts the allocated ga_item struct
// For sets: just discounts the key (noun stored directly in slot)
static c3_w
GA_CAT(NAME, _discount_item)(ga_slot sot_w)
{
  c3_w tot_w = KEY_DISCOUNT(GA_SLOT_KEY(sot_w));
#ifdef VAL_TY
  ITEM_TY* ite_u = ga_slot_to_item(sot_w);
#ifdef VAL_DISCOUNT
  tot_w += VAL_DISCOUNT(GA_ITEM_VAL(ite_u));
#endif
  tot_w += u3a_discount_ptr(ite_u);
#endif
  return tot_w;
}

static c3_w
GA_CAT(NAME, _discount_buck)(ga_buck* hab_u)
{
  c3_w tot_w = 0;
  c3_w i_w;

  for (i_w = 0; i_w < hab_u->len_w; i_w++) {
    tot_w += GA_CAT(NAME, _discount_item)(hab_u->sot_w[i_w]);
  }
  tot_w += u3a_discount_ptr(hab_u);

  return tot_w;
}

static c3_w
GA_CAT(NAME, _discount_node)(ga_node* han_u, c3_w lef_w)
{
  c3_w tot_w = 0;
  c3_w len_w = ga_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for (i_w = 0; i_w < len_w; i_w++) {
    ga_slot sot_w = han_u->sot_w[i_w];

    if (!ga_bit_get(han_u->nod_w, i_w)) {
      tot_w += GA_CAT(NAME, _discount_item)(sot_w);
    }
    else {
      void* hav_v = u3a_into(sot_w);

      if (0 == lef_w) {
        tot_w += GA_CAT(NAME, _discount_buck)(hav_v);
      }
      else {
        tot_w += GA_CAT(NAME, _discount_node)(hav_v, lef_w);
      }
    }
  }

  tot_w += u3a_discount_ptr(han_u);

  return tot_w;
}

GA_API_FN_QUALIFIERS c3_w
GA_CAT(NAME, _discount)(u3p(GA_CAT(NAME, _root)) har_p)
{
  c3_w tot_w = 0;
  ga_root* har_u = u3to(ga_root, har_p);
  c3_w i_w;

  for (i_w = 0; i_w < 64; i_w++) {
    ga_slot sot_w = har_u->sot_w[i_w];

    if (0 == sot_w) {
      continue;
    }
    else if (!ga_bit_get_d(har_u->nod_d, i_w)) {
      tot_w += GA_CAT(NAME, _discount_item)(sot_w);
    }
    else {
      ga_node* han_u = u3a_into(sot_w);

      tot_w += GA_CAT(NAME, _discount_node)(han_u, 25);
    }
  }

  tot_w += u3a_discount_ptr(har_u);

  return tot_w;
}

#endif // KEY_DISCOUNT

#endif // !HEADER_MODE

//////////////////////////////////////////////////////////////////////////////
// Cleanup macros
//////////////////////////////////////////////////////////////////////////////

#undef NAME
#undef ITEM_TY
#undef HASH_FN
#undef CMPR_FN
#ifdef KEY_DTOR_FN
#undef KEY_DTOR_FN
#endif
#ifdef KEY_TY
#undef KEY_TY
#endif
#ifdef VAL_TY
#undef VAL_TY
#endif
#ifdef GET_VAL
#undef GET_VAL
#endif
#ifdef VAL_DTOR_FN
#undef VAL_DTOR_FN
#endif
#ifdef KEY_MARK
#undef KEY_MARK
#endif
#ifdef VAL_MARK
#undef VAL_MARK
#endif
#ifdef KEY_COUNT
#undef KEY_COUNT
#endif
#ifdef VAL_COUNT
#undef VAL_COUNT
#endif
#ifdef KEY_RELOCATE
#undef KEY_RELOCATE
#endif
#ifdef VAL_RELOCATE
#undef VAL_RELOCATE
#endif
#ifdef KEY_DISCOUNT
#undef KEY_DISCOUNT
#endif
#ifdef VAL_DISCOUNT
#undef VAL_DISCOUNT
#endif
#ifdef HEADER_MODE
#undef HEADER_MODE
#endif
#ifdef IMPLEMENTATION_MODE
#undef IMPLEMENTATION_MODE
#endif
#undef GA_API_FN_QUALIFIERS
#undef ga_slot_to_item
#undef ga_item_to_slot
#undef GA_ITEM_KEY
#undef GA_ITEM_KEY_PTR
#ifdef GA_ITEM_VAL
#undef GA_ITEM_VAL
#undef GA_ITEM_VAL_PTR
#endif
#undef GA_ITEM_REF
#undef GA_ITEM_REF_NONE
#undef GA_OUT_TY
#undef GA_FREE_TY
#undef GA_FREE_NONE
#undef GA_FREE_IS_NONE
#undef GA_SLOT_KEY
#undef GA_OUT_SET
#undef GA_OUT_NEW
#undef GA_SLOT_PTR_TO_REF

#endif // !GASHTABLE_COMMON_IMPL
