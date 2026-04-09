/// @file meta.h
/// Noun metadata system for extended cells
///
/// Extended cells have additional metadata beyond the basic u3a_cell structure.
/// This metadata enables:
/// - Compiled program caching (for formulas)
/// - Battery info (for batteries)
/// - Stencil references (for static cores)
/// - Canonicalization tracking
///
/// Extended cells are detected by a marker value in mug_w: the high bit is set
/// (0x80000000). Valid mugs are 31-bit, so this bit is otherwise always 0.
/// The actual mug is stored in the metadata.

#ifndef U3_META_H
#define U3_META_H

#include "c3/c3.h"
#include "types.h"
#include "allocate.h"

//  Forward declarations
struct _u3n_prog;
struct _u3m_battery;
struct _u3m_stencil;

/// u3m_stencil_link: linked list node for stencil candidates
///
/// A formula/core can have multiple possible stencils depending on
/// which parent core context it appears in. At dispatch time, we
/// cycle through candidates to find one matching the current context.
///
typedef struct _u3m_stencil_link {
  struct _u3m_stencil*       ste_u;  //  stencil candidate
  struct _u3m_stencil_link*  nex_u;  //  next candidate (NULL if last)
} u3m_stencil_link;

/// Marker value for extended cells: high bit set
/// Valid mugs are 31-bit (max 0x7FFFFFFF), so high bit is otherwise always 0.
#define u3m_meta_marker  0x80000000

/// u3m_meta: metadata for extended cells
///
/// Not all cells have metadata - only those that have been extended.
/// Use u3m_has_meta() to check if a cell has metadata.
///
typedef struct _u3m_meta {
  c3_w                    mug_w;    //  actual mug (31-bit hash)
  c3_o                    can_o;    //  is canonical (interned)
  c3_o                    idl_o;    //  is ideal (sub-nouns also canonical)
  u3p(struct _u3n_prog)   pog_p;    //  compiled program (formulas)
  u3p(struct _u3m_battery) bat_p;   //  battery info (batteries)
  u3m_stencil_link*       ste_u;    //  stencil candidates (linked list)
} u3m_meta;

/// u3a_cell_x: extended cell with metadata
///
/// This structure extends u3a_cell with metadata fields.
/// The mug_w field contains u3m_meta_marker to indicate extension.
/// Standard cells are 4 words; extended cells are 4 + sizeof(u3m_meta)/4 words.
///
typedef struct {
  c3_w      use_w;    //  reference count
  c3_w      mug_w;    //  u3m_meta_marker (actual mug in met_u.mug_w)
  u3_noun   hed;      //  head
  u3_noun   tel;      //  tail
  u3m_meta  met_u;    //  metadata
} u3a_cell_x;

//  Size constants
#define u3a_cell_x_words  ((c3_w)c3_wiseof(u3a_cell_x))

/// u3m_has_meta(): check if cell has metadata
///
/// Returns c3y if the cell is an extended cell with metadata.
/// Checks for the marker value in mug_w.
///
static inline c3_o
u3m_has_meta(u3_noun cel)
{
  u3a_cell* cel_u = (u3a_cell*)u3a_to_ptr(cel);
  return ( u3m_meta_marker == cel_u->mug_w ) ? c3y : c3n;
}

/// u3m_meta_of(): get metadata pointer for extended cell
///
/// IMPORTANT: Only call this on cells where u3m_has_meta() returns c3y.
/// Behavior is undefined for non-extended cells.
///
static inline u3m_meta*
u3m_meta_of(u3_noun cel)
{
  u3a_cell_x* cel_u = (u3a_cell_x*)u3a_to_ptr(cel);
  return &(cel_u->met_u);
}

/// u3m_meta_get(): get metadata, returning NULL for non-extended cells
///
/// Safe version that returns NULL if cell has no metadata.
///
static inline u3m_meta*
u3m_meta_get(u3_noun cel)
{
  if ( c3n == u3a_is_cell(cel) ) {
    return NULL;
  }
  if ( c3n == u3m_has_meta(cel) ) {
    return NULL;
  }
  return u3m_meta_of(cel);
}

/// u3m_mug_get(): get actual mug value, handling extended cells
///
/// For extended cells, returns mug from metadata.
/// For normal cells, returns mug_w directly.
/// Returns 0 if mug not yet computed.
///
static inline c3_w
u3m_mug_get(u3a_cell* cel_u)
{
  if ( u3m_meta_marker == cel_u->mug_w ) {
    u3a_cell_x* cel_x = (u3a_cell_x*)cel_u;
    return cel_x->met_u.mug_w;
  }
  return cel_u->mug_w;
}

/// u3m_mug_set(): set mug value, handling extended cells
///
/// For extended cells, stores in metadata.
/// For normal cells, stores in mug_w directly.
///
static inline void
u3m_mug_set(u3a_cell* cel_u, c3_w mug_w)
{
  if ( u3m_meta_marker == cel_u->mug_w ) {
    u3a_cell_x* cel_x = (u3a_cell_x*)cel_u;
    cel_x->met_u.mug_w = mug_w;
  }
  else {
    cel_u->mug_w = mug_w;
  }
}

//  Stencil candidate list operations

/// u3m_stencil_link_new(): allocate a new stencil link node
///
u3m_stencil_link* u3m_stencil_link_new(struct _u3m_stencil* ste_u);

/// u3m_stencil_link_free(): free a stencil link list
///
void u3m_stencil_link_free(u3m_stencil_link* lnk_u);

/// u3m_stencil_add(): add stencil candidate to metadata
///
/// Adds stencil to front of candidates list if not already present.
///
void  u3m_stencil_add(u3m_meta* met_u, struct _u3m_stencil* ste_u);

/// u3m_stencil_merge(): merge stencil candidates from src into dst
///
/// Adds all candidates from src to dst (if not already present).
///
void u3m_stencil_merge(u3m_meta* dst_u, u3m_meta* src_u);

//  Metadata operations

/// u3m_meta_init(): initialize metadata to empty state
///
void u3m_meta_init(u3m_meta* met_u);

/// u3m_meta_free(): free metadata contents
///
/// Frees any resources owned by the metadata (programs, battery info, etc).
///
void u3m_meta_free(u3m_meta* met_u);

/// u3m_meta_mark(): mark metadata for GC
///
/// Returns size in words of marked data.
///
c3_w u3m_meta_mark(u3m_meta* met_u);

/// u3m_meta_take(): take metadata from child road
///
/// Copies junior metadata structures to current road.
/// Called from _ca_take_cell for extended cells.
///
void u3m_meta_take(u3m_meta* new_u, u3m_meta* old_u);

//  Extended cell allocation

/// u3m_cell_x(): allocate an extended cell
///
/// Like u3i_cell() but allocates extra space for metadata.
/// Sets mug_w to u3m_meta_marker.
///
u3_noun u3m_cell_x(u3_noun hed, u3_noun tel);

/// u3m_extend(): extend a cell to have metadata
///
/// If the cell already has metadata, returns it unchanged.
/// Otherwise, allocates a new extended cell with the same head/tail,
/// and updates all references to point to the new cell.
///
/// Returns the (possibly new) cell with metadata.
///
u3_noun u3m_extend(u3_noun cel);

#endif /* ifndef U3_META_H */
