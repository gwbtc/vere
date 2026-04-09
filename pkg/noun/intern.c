/// @file intern.c
/// Noun interning implementation

#include "intern.h"
#include "allocate.h"
#include "imprison.h"
#include "retrieve.h"
#include "meta.h"
#include "tables.h"

//  Per-road interner access macro
#define _intern_p u3R->int_u.set_p

/// _intern_is_interned(): check if noun has intern bit set (O(1))
///
static inline c3_o
_intern_is_interned(u3_noun som)
{
  if ( c3y == u3a_is_cat(som) ) {
    return c3y;  //  direct atoms are always canonical
  }
  u3a_noun* box_u = u3a_to_ptr(som);
  return ( box_u->use_w & u3a_intern_bit ) ? c3y : c3n;
}

/// _intern_set_interned(): set the intern bit on a noun
///
static inline void
_intern_set_interned(u3_noun som)
{
  if ( c3n == u3a_is_cat(som) ) {
    u3a_noun* box_u = u3a_to_ptr(som);
    box_u->use_w |= u3a_intern_bit;
  }
}

/// _intern_get(): look up noun in interner (current road only)
///
static u3_weak
_intern_get(u3_noun som)
{
  if ( !_intern_p ) {
    return u3_none;
  }
  return ga_intern_set_get(_intern_p, som);
}

/// _intern_deep(): recursively intern a noun
///
/// Interns all sub-nouns first, then the noun itself.
/// Returns canonical reference.
///
static u3_noun
_intern_deep(u3_noun som)
{
  //  Direct atoms are always canonical
  if ( c3y == u3a_is_cat(som) ) {
    return som;
  }

  ////  Check intern bit for O(1) short-circuit (works for all nouns)
  if ( c3y == _intern_is_interned(som) ) {
    return som;
  }

  //  Indirect atoms: check interner
  if ( c3y == u3a_is_pug(som) ) {
    u3_weak got = _intern_get(som);
    if ( u3_none != got ) {
      u3z(som);
      return u3k(got);
    }
    _intern_set_interned(som);
    ga_intern_set_put(_intern_p, u3k(som));
    return som;
  }

  //  Check interner before recursing (avoid unnecessary work)
  u3_weak got = _intern_get(som);
  if ( u3_none != got ) {
    u3z(som);
    return u3k(got);
  }

  //  Cell: recursively intern head and tail
  u3_noun hed = _intern_deep(u3k(u3h(som)));
  u3_noun tel = _intern_deep(u3k(u3t(som)));

  //  Check if we already have this cell interned
  u3_noun cel;
  if ( hed == u3h(som) && tel == u3t(som) ) {
    //  Head and tail unchanged - free the extra refs from recursion
    u3z(hed);
    u3z(tel);

    //  Check if this cell is interned
    u3_weak got = _intern_get(som);
    if ( u3_none != got ) {
      u3z(som);
      return u3k(got);
    }
    cel = som;
  }
  else {
    //  Need to create a new cell with interned sub-nouns
    //  Use regular cell (extended only when metadata needed)
    cel = u3i_cell(hed, tel);
    u3z(som);
  }

  //  Mark as interned via refcount bit
  _intern_set_interned(cel);

  //  Add to interner
  ga_intern_set_put(_intern_p, u3k(cel));

  return cel;
}

/// u3i_intern(): intern a noun, returning canonical reference
///
u3_noun
u3i_intern(u3_noun som)
{
  if ( !_intern_p ) {
    u3i_intern_init();
  }
  return _intern_deep(som);
}

/// u3i_intern_k(): intern a noun, keeping input reference
///
u3_noun
u3i_intern_k(u3_noun som)
{
  return u3i_intern(u3k(som));
}

/// u3i_is_interned(): check if noun is in the interner
///
c3_o
u3i_is_interned(u3_noun som)
{
  if ( !_intern_p ) {
    return c3n;
  }
  if ( c3y == u3a_is_cat(som) ) {
    return c3y;  //  direct atoms are always "interned"
  }
  return (u3_none != _intern_get(som)) ? c3y : c3n;
}

/// u3i_canonical(): get canonical form of noun if interned
///
u3_weak
u3i_canonical(u3_noun som)
{
  if ( !_intern_p ) {
    return u3_none;
  }
  if ( c3y == u3a_is_cat(som) ) {
    return som;  //  direct atoms are canonical
  }
  return _intern_get(som);
}

/// u3i_intern_init(): initialize the interner for current road
///
void
u3i_intern_init(void)
{
  if ( _intern_p ) {
    return;
  }
  u3R->int_u.set_p = ga_intern_set_new();  //  uncached
}

/// u3i_intern_free(): free the interner for current road
///
void
u3i_intern_free(void)
{
  if ( _intern_p ) {
    ga_intern_set_free(_intern_p);
    u3R->int_u.set_p = 0;
  }
}

/// _intern_reap_cb(): callback for walk during reap
///
static void
_intern_reap_cb(u3_noun* som_p, void* wit)
{
  (void)wit;
  u3_noun som = *som_p;

  //  Take the noun from child to parent
  u3_noun tak = u3a_take(som);

  //  Check if already in parent's set
  u3_weak got = _intern_get(tak);
  if ( u3_none == got ) {
    //  Not in parent, add it
    ga_intern_set_put(_intern_p, tak);
  }
  else {
    //  Already in parent, free the duplicate
    u3z(tak);
  }
}

/// u3i_intern_reap(): take and merge child interner into parent
///
/// Called BEFORE u3a_drop_heap() so child data is still accessible.
/// Takes each noun from child set and adds to parent's interner.
/// Child set structure will be cleaned up by drop_heap.
///
void
u3i_intern_reap(u3p(ga_root) int_p)
{
  if ( !int_p ) {
    return;
  }

  //  Ensure parent has an interner
  if ( !_intern_p ) {
    u3i_intern_init();
  }

  //  Walk child set, take each noun, add to parent
  ga_intern_set_walk_with(int_p, _intern_reap_cb, NULL);

  //  Don't free child set - drop_heap will handle it
}

/// u3i_intern_new(): create empty interner for new child road
///
/// Child starts with empty interner. It will find parent's interned
/// nouns via equality checks, and intern its own nouns locally.
///
u3p(ga_root)
u3i_intern_new(void)
{
  return 0;
}

/// u3i_intern_mark(): mark interner for GC
///
c3_w
u3i_intern_mark(void)
{
  if ( !_intern_p ) {
    return 0;
  }
  return ga_intern_set_mark(_intern_p);
}

/// u3i_intern_ream(): rebuild after checkpoint restore
///
void
u3i_intern_ream(void)
{
  //  Clear and rebuild interner
  //  (interned nouns will be re-interned on demand)
  u3i_intern_free();
}
