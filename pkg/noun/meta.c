/// @file meta.c
/// Noun metadata system implementation

#include "meta.h"
#include "allocate.h"
#include "imprison.h"
#include "manage.h"
#include "vortex.h"
#include "stencil.h"
#include "nock.h"

//  Helper: check if a malloc'd pointer is senior (in parent road)
//
static inline c3_o
_meta_is_junior(u3a_road* rod_u, void* ptr)
{
  //if ( !ptr ) return c3y;  //  null is "senior" (nothing to take)
  c3_w off_w = u3a_outa(ptr);
  if ( c3y == u3a_is_north(u3R) ) {
    if ( (u3R->hat_p <= off_w) && (off_w < u3R->mat_p) ) {
      return c3y;
    }
    return c3n;
  } else {
    if ( (u3R->mat_p <= off_w) && (off_w < u3R->rut_p) ) {
      return c3y;
    }
    return c3n;
  }
}

//  Helper: check if a malloc'd pointer is senior (in parent road)
//
static inline c3_o
_meta_is_senior(u3a_road* rod_u, void* ptr)
{
  if ( !ptr ) return c3y;  //  null is "senior" (nothing to take)
  c3_w off_w = u3a_outa(ptr);
  u3_noun fak = u3a_to_pug(off_w);  //  fake as indirect noun
  return u3a_is_senior(u3R, fak);
}

/// u3m_stencil_link_new(): allocate a new stencil link node
///
u3m_stencil_link*
u3m_stencil_link_new(struct _u3m_stencil* ste_u)
{
  u3m_stencil_link* lnk_u = u3a_malloc(sizeof(u3m_stencil_link));
  lnk_u->ste_u = ste_u;
  lnk_u->nex_u = NULL;
  return lnk_u;
}

/// u3m_stencil_link_free(): free a stencil link list
///
void
u3m_stencil_link_free(u3m_stencil_link* lnk_u)
{
  while ( lnk_u ) {
    u3m_stencil_link* nex_u = lnk_u->nex_u;
    //  Note: we don't free the stencil itself, just the link node
    u3a_free(lnk_u);
    lnk_u = nex_u;
  }
}

/// _stencil_equal(): check if two stencils are semantically equal
///
/// Equal if same kernel, same static/dynamic status, and same context:
/// - Static: same whole-core reference
/// - Dynamic: equal parent stencil chain
///
static c3_o
_stencil_equal(u3m_stencil* a_u, u3m_stencil* b_u)
{
  //  Same pointer is trivially equal
  if ( a_u == b_u ) {
    return c3y;
  }

  //  Must have same kernel
  if ( a_u->ker_u != b_u->ker_u ) {
    return c3n;
  }

  //  Must have same static/dynamic status
  if ( a_u->sta_o != b_u->sta_o ) {
    return c3n;
  }

  if ( c3y == a_u->sta_o ) {
    //  Static: same whole-core reference
    return (a_u->who_p == b_u->who_p) ? c3y : c3n;
  }
  else {
    //  Dynamic: recursively check parent stencil equality
    return _stencil_equal(a_u->par_u, b_u->par_u);
  }
}

/// _stencil_pick_senior(): pick the more senior stencil
///
/// Walks up road hierarchy to find which is truly more senior.
/// Returns the stencil that should be kept.
///
static u3m_stencil*
_stencil_pick_senior(u3m_stencil* a_u, u3m_stencil* b_u)
{
  //  Pointer equality - same stencil
  if ( a_u == b_u ) {
    return a_u;
  }

  u3a_road* rod_u = u3R;
  while ( true ) {
    c3_t asr_t = ( c3y == _meta_is_senior(rod_u, a_u) );
    c3_t bsr_t = ( c3y == _meta_is_senior(rod_u, b_u) );

    if ( asr_t == bsr_t ) {
      if ( !asr_t ) {
        //  Both junior at this level - pick by address
        //  In north road, higher addresses are older
        //  In south road, lower addresses are older
        if ( (a_u > b_u) == (c3y == u3a_is_north(rod_u)) ) {
          return a_u;
        } else {
          return b_u;
        }
      } else {
        //  Both senior - walk up to parent road
        if ( &u3H->rod_u == rod_u ) {
          //  At home road, both equally senior - pick a
          return a_u;
        }
        rod_u = u3to(u3a_road, rod_u->par_p);
      }
    } else {
      //  One senior, one junior - return the senior one
      return asr_t ? a_u : b_u;
    }
  }
}

/// u3m_stencil_add(): add stencil candidate to metadata
///
/// If an equal stencil already exists, keeps the senior one.
///
void
u3m_stencil_add(u3m_meta* met_u, struct _u3m_stencil* ste_u)
{
  //  Check if an equal stencil already exists
  for ( u3m_stencil_link* cur_u = met_u->ste_u; cur_u; cur_u = cur_u->nex_u ) {
    //  Pointer equality short circuit
    if ( cur_u->ste_u == ste_u ) {
      return;
    }

    if ( c3y == _stencil_equal(cur_u->ste_u, ste_u) ) {
      //  Found equal stencil - keep the senior one
      cur_u->ste_u = _stencil_pick_senior(cur_u->ste_u, ste_u);
      return;
    }
  }

  //  Not found - add to front of list
  u3m_stencil_link* lnk_u = u3m_stencil_link_new(ste_u);
  lnk_u->nex_u = met_u->ste_u;
  met_u->ste_u = lnk_u;
}

/// u3m_stencil_merge(): merge stencil candidates from src into dst
///
void
u3m_stencil_merge(u3m_meta* a_u, u3m_meta* b_u)
{
  u3m_meta* dst_u;
  u3m_meta* src_u;

  u3a_road* rod_u = u3R;
  while ( true ) {
    c3_t asr_t = ( c3y == _meta_is_senior(rod_u, a_u) );
    c3_t bsr_t = ( c3y == _meta_is_senior(rod_u, b_u) );
    if (asr_t == bsr_t) {
      if ( !asr_t ) {
        if ( (a_u > b_u) == (c3y == u3a_is_north(rod_u)) ) {
          dst_u = a_u;
          src_u = b_u;
        } else {
          dst_u = b_u;
          src_u = a_u;
        }
        break;
      } else {
        if ( &u3H->rod_u == rod_u ) {
          dst_u = NULL;
          src_u = NULL;
          abort();
        }
        rod_u = u3to(u3a_road, rod_u->par_p);
      }
    } else {
      if ( asr_t ) {
        dst_u = b_u;
        src_u = a_u;
      } else {
        dst_u = a_u;
        src_u = b_u;
      }
      break;
    }
  }

  //  Add each stencil from src to dst (skips duplicates)
  for ( u3m_stencil_link* cur_u = src_u->ste_u; cur_u; cur_u = cur_u->nex_u ) {
    u3m_stencil_add(dst_u, cur_u->ste_u);
  }
}

/// u3m_meta_init(): initialize metadata to empty state
///
void
u3m_meta_init(u3m_meta* met_u)
{
  met_u->mug_w = 0;
  met_u->can_o = c3n;
  met_u->idl_o = c3n;
  met_u->pog_p = 0;
  met_u->bat_p = 0;
  met_u->ste_u = NULL;
}

/// u3m_meta_free(): free metadata contents
///
void
u3m_meta_free(u3m_meta* met_u)
{
  //  Free compiled program if present
  //  (implementation will be added when we integrate with nock.c)
  if ( met_u->pog_p ) {
    //  TODO: u3n_prog_free(u3to(struct _u3n_prog, met_u->pog_p));
    met_u->pog_p = 0;
  }

  //  Free battery info if present
  if ( met_u->bat_p ) {
    //  TODO: u3m_battery_free(u3to(struct _u3m_battery, met_u->bat_p));
    met_u->bat_p = 0;
  }

  //  Free stencil candidate list
  //  Note: stencils themselves are shared, so we only free the link nodes
  if ( met_u->ste_u ) {
    u3m_stencil_link_free(met_u->ste_u);
    met_u->ste_u = NULL;
  }

  met_u->can_o = c3n;
  met_u->idl_o = c3n;
}

/// u3m_meta_mark(): mark metadata for GC
///
c3_w
u3m_meta_mark(u3m_meta* met_u)
{
  c3_w tot_w = 0;

  //  Mark compiled program
  if ( met_u->pog_p ) {
    //  TODO: tot_w += u3n_prog_mark(u3to(struct _u3n_prog, met_u->pog_p));
  }

  //  Mark battery info
  if ( met_u->bat_p ) {
    //  TODO: tot_w += u3m_battery_mark(u3to(struct _u3m_battery, met_u->bat_p));
  }

  //  Mark stencil candidates
  for ( u3m_stencil_link* cur_u = met_u->ste_u; cur_u; cur_u = cur_u->nex_u ) {
    //  TODO: tot_w += u3m_stencil_mark(cur_u->ste_u);
    (void)cur_u;  //  suppress unused warning until TODO is done
  }

  return tot_w;
}

/// _meta_prog_take(): take a compiled program from child road
///
static u3n_prog*
_meta_prog_take(u3n_prog* old_u)
{
  if ( !old_u ) return NULL;
  if ( c3n == _meta_is_junior(u3R, old_u) ) return old_u;

  //  Allocate new program struct
  u3n_prog* new_u = u3a_malloc(sizeof(u3n_prog));
  *new_u = *old_u;  //  copy scalar fields

  //  Take bytecode array
  if ( old_u->byc_u.ops_y && old_u->byc_u.own_o ) {
    new_u->byc_u.ops_y = u3a_malloc(old_u->byc_u.len_w);
    memcpy(new_u->byc_u.ops_y, old_u->byc_u.ops_y, old_u->byc_u.len_w);
  }

  //  Take literal pool (nouns need u3a_take)
  if ( old_u->lit_u.len_w ) {
    new_u->lit_u.non = u3a_malloc(old_u->lit_u.len_w * sizeof(u3_noun));
    for ( c3_w i = 0; i < old_u->lit_u.len_w; i++ ) {
      new_u->lit_u.non[i] = u3a_take(old_u->lit_u.non[i]);
    }
  }

  //  Take memo slots
  if ( old_u->mem_u.len_w ) {
    new_u->mem_u.sot_u = u3a_malloc(old_u->mem_u.len_w * sizeof(u3n_memo));
    memcpy(new_u->mem_u.sot_u, old_u->mem_u.sot_u,
           old_u->mem_u.len_w * sizeof(u3n_memo));
  }

  //  Take call sites (contain nouns)
  if ( old_u->cal_u.len_w ) {
    new_u->cal_u.sit_u = u3a_malloc(old_u->cal_u.len_w * sizeof(u3j_site));
    for ( c3_w i = 0; i < old_u->cal_u.len_w; i++ ) {
      new_u->cal_u.sit_u[i] = old_u->cal_u.sit_u[i];
      //  Take nouns in site
      new_u->cal_u.sit_u[i].axe = u3a_take(old_u->cal_u.sit_u[i].axe);
      if ( u3_none != old_u->cal_u.sit_u[i].bat ) {
        new_u->cal_u.sit_u[i].bat = u3a_take(old_u->cal_u.sit_u[i].bat);
      }
      if ( u3_none != old_u->cal_u.sit_u[i].bas ) {
        new_u->cal_u.sit_u[i].bas = u3a_take(old_u->cal_u.sit_u[i].bas);
      }
      if ( u3_none != old_u->cal_u.sit_u[i].loc ) {
        new_u->cal_u.sit_u[i].loc = u3a_take(old_u->cal_u.sit_u[i].loc);
      }
      if ( u3_none != old_u->cal_u.sit_u[i].lab ) {
        new_u->cal_u.sit_u[i].lab = u3a_take(old_u->cal_u.sit_u[i].lab);
      }
      //  Clear cached pointers (will be recomputed)
      new_u->cal_u.sit_u[i].pog_p = 0;
      new_u->cal_u.sit_u[i].fin_p = 0;
    }
  }

  //  Take registration sites (contain nouns)
  if ( old_u->reg_u.len_w ) {
    new_u->reg_u.rit_u = u3a_malloc(old_u->reg_u.len_w * sizeof(u3j_rite));
    for ( c3_w i = 0; i < old_u->reg_u.len_w; i++ ) {
      new_u->reg_u.rit_u[i] = old_u->reg_u.rit_u[i];
      //  Take nouns in rite
      if ( u3_none != old_u->reg_u.rit_u[i].clu ) {
        new_u->reg_u.rit_u[i].clu = u3a_take(old_u->reg_u.rit_u[i].clu);
      }
      //  Clear cached pointers
      new_u->reg_u.rit_u[i].fin_p = 0;
    }
  }

  return new_u;
}

/// _meta_battery_take(): take battery metadata from child road
///
static u3m_battery*
_meta_battery_take(u3m_battery* old_u)
{
  if ( !old_u ) return NULL;
  if ( c3n == _meta_is_junior(u3R, old_u) ) return old_u;

  //  Allocate and copy
  u3m_battery* new_u = u3a_malloc(sizeof(u3m_battery));
  new_u->axe   = u3a_take(old_u->axe);
  new_u->rot_p = 0;  //  maps will be rebuilt on demand
  new_u->kid_p = 0;
  new_u->spo_p = 0;

  return new_u;
}

/// _meta_stencil_take(): take a stencil from child road
///
/// Kernels must be reaped FIRST (via u3m_kernel_reap) before calling this,
/// so that forwarding pointers are available for kernel backpointer fixup.
///
static u3m_stencil*
_meta_stencil_take(u3m_stencil* old_u)
{
  if ( !old_u ) return NULL;
  if ( c3n == _meta_is_junior(u3R, old_u) ) return old_u;

  //  Allocate and copy
  u3m_stencil* new_u = u3a_malloc(sizeof(u3m_stencil));

  //  Kernel backpointer: read forwarding pointer if kernel was reaped
  new_u->ker_u = u3m_kernel_forwarded(old_u->ker_u);

  new_u->bat_p = old_u->bat_p;  //  will be fixed up by caller if needed
  new_u->sta_o = old_u->sta_o;

  if ( c3y == old_u->sta_o ) {
    new_u->who_p = old_u->who_p;  //  will be fixed up
  }
  else {
    //  Dynamic: take parent stencil recursively
    new_u->par_u = _meta_stencil_take(old_u->par_u);
  }

  return new_u;
}

/// _meta_stencil_link_take(): take stencil link list from child road
///
static u3m_stencil_link*
_meta_stencil_link_take(u3m_stencil_link* old_u)
{
  if ( !old_u ) return NULL;

  u3m_stencil_link* new_u = NULL;
  u3m_stencil_link* tail_u = NULL;

  for ( u3m_stencil_link* cur_u = old_u; cur_u; cur_u = cur_u->nex_u ) {
    //  Take this link if junior
    u3m_stencil_link* lnk_u;
    if ( c3n == _meta_is_junior(u3R, cur_u) ) {
      lnk_u = cur_u;
    }
    else {
      lnk_u = u3a_malloc(sizeof(u3m_stencil_link));
      lnk_u->ste_u = _meta_stencil_take(cur_u->ste_u);
      lnk_u->nex_u = NULL;
    }

    if ( !new_u ) {
      new_u = lnk_u;
    }
    else {
      tail_u->nex_u = lnk_u;
    }
    tail_u = lnk_u;
  }

  return new_u;
}

/// u3m_meta_take(): take metadata from child road
///
/// Copies junior metadata structures to current road.
/// Called from _ca_take_cell for extended cells.
///
void
u3m_meta_take(u3m_meta* new_u, u3m_meta* old_u)
{
  //  Copy scalar fields
  new_u->mug_w = old_u->mug_w;
  new_u->can_o = old_u->can_o;
  new_u->idl_o = old_u->idl_o;

  //  Take program if junior
  if ( old_u->pog_p ) {
    u3n_prog* pog_u = u3to(u3n_prog, old_u->pog_p);
    u3n_prog* tak_u = _meta_prog_take(pog_u);
    new_u->pog_p = u3of(u3n_prog, tak_u);
  }
  else {
    new_u->pog_p = 0;
  }

  //  Take battery if junior
  if ( old_u->bat_p ) {
    u3m_battery* bat_u = u3to(u3m_battery, old_u->bat_p);
    u3m_battery* tak_u = _meta_battery_take(bat_u);
    new_u->bat_p = u3of(u3m_battery, tak_u);
  }
  else {
    new_u->bat_p = 0;
  }

  //  Take stencil links if junior
  new_u->ste_u = _meta_stencil_link_take(old_u->ste_u);
}

/// u3m_cell_x(): allocate an extended cell
///
u3_noun
u3m_cell_x(u3_noun hed, u3_noun tel)
{
  //  Allocate extended cell
  u3a_cell_x* cel_u = u3a_walloc(u3a_cell_x_words);

  cel_u->use_w = 1;
  cel_u->mug_w = u3m_meta_marker;  //  marker indicates extended cell
  cel_u->hed   = hed;
  cel_u->tel   = tel;

  //  Initialize metadata (including mug_w = 0)
  u3m_meta_init(&(cel_u->met_u));

  //  Convert to noun (cell pointer)
  c3_w off_w = u3a_outa(cel_u);
  return u3a_to_pom(off_w);
}

/// u3m_extend(): extend a cell to have metadata
///
u3_noun
u3m_extend(u3_noun cel)
{
  //  Check if already extended
  if ( c3y == u3m_has_meta(cel) ) {
    return cel;
  }

  //  Must be a cell
  if ( c3n == u3a_is_cell(cel) ) {
    return cel;
  }

  //  Get current head/tail
  u3_noun hed = u3k(u3h(cel));
  u3_noun tel = u3k(u3t(cel));

  //  Allocate new extended cell
  u3_noun new = u3m_cell_x(hed, tel);

  //  Copy mug to metadata if computed
  u3a_cell* old_u = (u3a_cell*)u3a_to_ptr(cel);
  if ( old_u->mug_w ) {
    u3a_cell_x* new_u = (u3a_cell_x*)u3a_to_ptr(new);
    new_u->met_u.mug_w = old_u->mug_w;  //  store in metadata, not marker
  }

  //  Note: we don't automatically free the old cell or update references
  //  The caller is responsible for managing the transition
  //  (This allows interning to handle reference unification properly)

  return new;
}
