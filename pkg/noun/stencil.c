/// @file stencil.c
/// Stencil and kernel system implementation

#include "stencil.h"
#include "allocate.h"
#include "imprison.h"
#include "retrieve.h"
#include "meta.h"
#include "intern.h"
#include "tables.h"

//  Per-road kernel roots access macro
#define _kernel_roots_p u3R->sten_u.ker_p

//  Helper: check if a malloc'd pointer is junior (in child road)
//
static inline c3_o
_kernel_is_junior(void* ptr)
{
  if ( !ptr ) return c3n;
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

/// _kernel_take(): take a kernel from child road, storing forwarding pointer
///
/// After taking, the new kernel pointer is stored at the old kernel's
/// location (in the nam_c field), allowing stencils to read the forwarded
/// pointer and update their ker_u backpointer.
///
static u3m_kernel*
_kernel_take(u3m_kernel* old_u)
{
  if ( !old_u ) return NULL;
  if ( c3n == _kernel_is_junior(old_u) ) return old_u;

  //  Allocate new kernel
  u3m_kernel* new_u = u3a_malloc(sizeof(u3m_kernel));

  //  Take name string
  if ( old_u->nam_c ) {
    c3_w len_w = strlen(old_u->nam_c) + 1;
    new_u->nam_c = u3a_malloc(len_w);
    memcpy(new_u->nam_c, old_u->nam_c, len_w);
  } else {
    new_u->nam_c = NULL;
  }

  //  Parent will be fixed up after all kernels are taken
  new_u->par_u = old_u->par_u;
  new_u->sta_o = old_u->sta_o;
  new_u->axe   = u3a_take(old_u->axe);
  new_u->kel   = u3a_take(old_u->kel);

  //  Take arms array
  new_u->arm_w = old_u->arm_w;
  if ( old_u->arm_u && old_u->arm_w ) {
    new_u->arm_u = u3a_malloc(old_u->arm_w * sizeof(u3m_arm));
    for ( c3_w i = 0; i < old_u->arm_w; i++ ) {
      new_u->arm_u[i].axe   = u3a_take(old_u->arm_u[i].axe);
      new_u->arm_u[i].drv_f = old_u->arm_u[i].drv_f;
    }
  } else {
    new_u->arm_u = NULL;
  }

  //  Take hooks array
  new_u->hok_w = old_u->hok_w;
  if ( old_u->hok_u && old_u->hok_w ) {
    new_u->hok_u = u3a_malloc(old_u->hok_w * sizeof(u3m_hook));
    for ( c3_w i = 0; i < old_u->hok_w; i++ ) {
      c3_w len_w = strlen(old_u->hok_u[i].nam_c) + 1;
      new_u->hok_u[i].nam_c = u3a_malloc(len_w);
      memcpy(new_u->hok_u[i].nam_c, old_u->hok_u[i].nam_c, len_w);
      new_u->hok_u[i].axe = u3a_take(old_u->hok_u[i].axe);
    }
  } else {
    new_u->hok_u = NULL;
  }

  //  Maps will be rebuilt on demand (or taken separately)
  new_u->kid_p = 0;
  new_u->ste_p = 0;

  //  Store forwarding pointer in old kernel's nam_c field
  //  This allows stencils to find the new kernel location
  old_u->nam_c = (c3_c*)new_u;

  return new_u;
}

/// u3m_kernel_forwarded(): get forwarded kernel pointer
///
/// After kernel reaping, reads the forwarding pointer stored in
/// the old kernel's nam_c field.
///
u3m_kernel*
u3m_kernel_forwarded(u3m_kernel* old_u)
{
  if ( !old_u ) return NULL;
  if ( c3n == _kernel_is_junior(old_u) ) return old_u;

  //  The forwarding pointer was stored in nam_c
  return (u3m_kernel*)old_u->nam_c;
}

//  Callback for walking kernel map during reap
//  For maps, walk_with receives ga_item* (which has key and val fields)
//
static void
_kernel_reap_cb(ga_kernel_map_item* ite_u, void* wit)
{
  (void)wit;
  u3m_kernel* old_u = u3to(u3m_kernel, ite_u->val);
  u3m_kernel* new_u = _kernel_take(old_u);

  //  Take the key noun
  u3_noun new_key = u3a_take(ite_u->key);

  //  Add to parent's kernel map
  ga_kernel_map_put(_kernel_roots_p, new_key, u3of(u3m_kernel, new_u));
}

/// u3m_kernel_reap(): take kernels from child road into parent
///
/// Called BEFORE u3a_drop_heap() so child data is accessible.
/// Stores forwarding pointers in old kernels for stencil fixup.
///
void
u3m_kernel_reap(u3p(struct ga_root) ker_p)
{
  if ( !ker_p ) {
    return;
  }

  //  Ensure parent has a kernel map
  if ( !_kernel_roots_p ) {
    _kernel_roots_p = ga_kernel_map_new();
  }

  //  Walk child kernel map, take each kernel
  ga_kernel_map_walk_with(ker_p, _kernel_reap_cb, NULL);

  //  Don't free child map - drop_heap will handle it
}

/// u3m_kernel_new(): create a new kernel
///
u3m_kernel*
u3m_kernel_new(c3_c* nam_c)
{
  u3m_kernel* ker_u = u3a_malloc(sizeof(u3m_kernel));

  //  Copy name
  c3_w len_w = strlen(nam_c) + 1;
  ker_u->nam_c = u3a_malloc(len_w);
  memcpy(ker_u->nam_c, nam_c, len_w);

  ker_u->par_u = NULL;
  ker_u->sta_o = c3y;
  ker_u->axe   = 1;
  ker_u->kel   = 0;

  ker_u->arm_w = 0;
  ker_u->arm_u = NULL;

  ker_u->hok_w = 0;
  ker_u->hok_u = NULL;

  ker_u->kid_p = 0;
  ker_u->ste_p = 0;

  return ker_u;
}

/// u3m_kernel_free(): free a kernel and its resources
///
void
u3m_kernel_free(u3m_kernel* ker_u)
{
  if ( !ker_u ) return;

  //  Free name
  if ( ker_u->nam_c ) {
    u3a_free(ker_u->nam_c);
  }

  //  Free arms
  if ( ker_u->arm_u ) {
    for ( c3_w i = 0; i < ker_u->arm_w; i++ ) {
      u3z(ker_u->arm_u[i].axe);
    }
    u3a_free(ker_u->arm_u);
  }

  //  Free hooks
  if ( ker_u->hok_u ) {
    for ( c3_w i = 0; i < ker_u->hok_w; i++ ) {
      u3a_free(ker_u->hok_u[i].nam_c);
      u3z(ker_u->hok_u[i].axe);
    }
    u3a_free(ker_u->hok_u);
  }

  //  Free children map
  if ( ker_u->kid_p ) {
    ga_kernel_map_free(ker_u->kid_p);
  }

  //  Free stencils set
  if ( ker_u->ste_p ) {
    ga_stencil_set_free(ker_u->ste_p);
  }

  u3z(ker_u->axe);
  u3z(ker_u->kel);

  u3a_free(ker_u);
}

/// u3m_root_install(): install a root kernel
///
u3m_kernel*
u3m_root_install(c3_c* nam_c, u3_atom kel)
{
  if ( !_kernel_roots_p ) {
    _kernel_roots_p = ga_kernel_map_new();
  }

  //  Create key [name kelvin]
  u3_noun nam = u3i_string(nam_c);
  u3_noun key = u3nc(nam, u3k(kel));

  //  Check if already exists
  u3_weak got = ga_kernel_map_get(_kernel_roots_p, key);
  if ( u3_none != got ) {
    u3z(key);
    u3z(kel);
    return u3to(u3m_kernel, got);
  }

  //  Create new kernel
  u3m_kernel* ker_u = u3m_kernel_new(nam_c);
  ker_u->kel   = kel;
  ker_u->sta_o = c3y;  //  root kernels are always static

  //  Add to roots
  ga_kernel_map_put(_kernel_roots_p, key, u3of(u3m_kernel, ker_u));

  return ker_u;
}

/// u3m_child_install(): install a child kernel
///
u3m_kernel*
u3m_child_install(u3m_kernel* par_u, c3_c* nam_c, u3_atom axe)
{
  if ( !par_u->kid_p ) {
    par_u->kid_p = ga_kernel_map_new();
  }

  //  Create key [name parent-axis]
  u3_noun nam = u3i_string(nam_c);
  u3_noun key = u3nc(nam, u3k(axe));

  //  Check if already exists
  u3_weak got = ga_kernel_map_get(par_u->kid_p, key);
  if ( u3_none != got ) {
    u3z(key);
    u3z(axe);
    return u3to(u3m_kernel, got);
  }

  //  Create new kernel
  u3m_kernel* ker_u = u3m_kernel_new(nam_c);
  ker_u->par_u = par_u;
  ker_u->axe   = axe;

  //  Determine if static: axis == 1 AND parent is static
  if ( (1 == axe || (c3y == u3a_is_cat(axe) && 1 == axe)) &&
       (c3y == par_u->sta_o) )
  {
    ker_u->sta_o = c3y;
  }
  else {
    ker_u->sta_o = c3n;
  }

  //  Add to parent's children
  ga_kernel_map_put(par_u->kid_p, key, u3of(u3m_kernel, ker_u));

  return ker_u;
}

/// u3m_arm_install(): install an arm driver on a kernel
///
void
u3m_arm_install(u3m_kernel* ker_u, u3_atom axe, u3_noun (*drv_f)(u3_noun))
{
  c3_w old_w = ker_u->arm_w;
  c3_w new_w = old_w + 1;

  //  Reallocate arms array
  ker_u->arm_u = u3a_realloc(ker_u->arm_u, new_w * sizeof(u3m_arm));
  ker_u->arm_w = new_w;

  //  Add new arm
  ker_u->arm_u[old_w].axe   = u3k(axe);
  ker_u->arm_u[old_w].drv_f = drv_f;
}

/// u3m_hook_install(): install a hook on a kernel
///
void
u3m_hook_install(u3m_kernel* ker_u, c3_c* nam_c, u3_atom axe)
{
  c3_w old_w = ker_u->hok_w;
  c3_w new_w = old_w + 1;

  //  Reallocate hooks array
  ker_u->hok_u = u3a_realloc(ker_u->hok_u, new_w * sizeof(u3m_hook));
  ker_u->hok_w = new_w;

  //  Copy name
  c3_w len_w = strlen(nam_c) + 1;
  ker_u->hok_u[old_w].nam_c = u3a_malloc(len_w);
  memcpy(ker_u->hok_u[old_w].nam_c, nam_c, len_w);

  ker_u->hok_u[old_w].axe = u3k(axe);
}

/// u3m_hook_axis(): look up hook axis by name
///
u3_weak
u3m_hook_axis(u3m_kernel* ker_u, c3_c* nam_c)
{
  for ( c3_w i = 0; i < ker_u->hok_w; i++ ) {
    if ( 0 == strcmp(ker_u->hok_u[i].nam_c, nam_c) ) {
      return u3k(ker_u->hok_u[i].axe);
    }
  }
  return u3_none;
}

/// u3m_battery_new(): create new battery metadata
///
u3m_battery*
u3m_battery_new(u3_atom axe)
{
  u3m_battery* bat_u = u3a_malloc(sizeof(u3m_battery));

  bat_u->axe   = u3k(axe);
  bat_u->rot_p = 0;
  bat_u->kid_p = 0;
  bat_u->spo_p = 0;

  return bat_u;
}

/// u3m_battery_free(): free battery metadata
///
void
u3m_battery_free(u3m_battery* bat_u)
{
  if ( !bat_u ) return;

  u3z(bat_u->axe);

  if ( bat_u->rot_p ) {
    //  TODO: free root stencils map
  }
  if ( bat_u->kid_p ) {
    //  TODO: free child stencils map
  }

  u3a_free(bat_u);
}

/// u3m_battery_get(): get or create battery metadata for a cell
///
u3m_battery*
u3m_battery_get(u3_noun bat)
{
  //  Ensure cell has metadata
  u3_noun ext = u3m_extend(bat);
  u3m_meta* met_u = u3m_meta_of(ext);

  //  Check if already has battery info
  if ( met_u->bat_p ) {
    if ( ext != bat ) u3z(ext);
    return u3to(u3m_battery, met_u->bat_p);
  }

  //  Create new battery info
  u3m_battery* bat_u = u3m_battery_new(0);  //  axis set later
  met_u->bat_p = u3of(u3m_battery, bat_u);

  if ( ext != bat ) u3z(ext);
  return bat_u;
}

/// u3m_stencil_new(): create a new stencil
///
u3m_stencil*
u3m_stencil_new(u3m_kernel* ker_u, u3_noun bat)
{
  u3m_stencil* ste_u = u3a_malloc(sizeof(u3m_stencil));

  ste_u->ker_u = ker_u;
  ste_u->sta_o = ker_u->sta_o;

  //  Get/create battery metadata
  u3m_battery* bat_u = u3m_battery_get(bat);
  u3_noun ext = u3m_extend(bat);
  ste_u->bat_p = u3of(u3m_meta, u3m_meta_of(ext));
  if ( ext != bat ) u3z(ext);

  ste_u->who_p = 0;
  ste_u->par_u = NULL;

  return ste_u;
}

/// u3m_stencil_free(): free a stencil
///
void
u3m_stencil_free(u3m_stencil* ste_u)
{
  if ( !ste_u ) return;
  u3a_free(ste_u);
}

/// _m_spot_match(): check if stencil matches core context
///
/// For static stencils, the whole core must match.
/// For dynamic stencils, need to walk to parent.
///
static c3_o
_m_spot_match(u3m_stencil* ste_u, u3_noun cor)
{
  //  Static stencils: the whole core was interned, so if we're
  //  looking it up by the same core reference, it matches
  if ( c3y == ste_u->sta_o ) {
    //  For static stencils, the stencil is attached to the canonical
    //  core, so any lookup on this core should match
    return c3y;
  }

  //  Dynamic stencils: need to check parent context
  //  TODO: walk to parent and verify parent stencil matches
  //  For now, return yes (we'll improve matching later)
  return c3y;
}

/// u3m_spot(): find stencil for a core
///
u3m_stencil*
u3m_spot(u3_noun cor)
{
  //  Must be a cell
  if ( c3n == u3a_is_cell(cor) ) {
    return NULL;
  }

  //  Check for stencil candidates on core itself
  u3m_meta* met_u = u3m_meta_get(cor);
  if ( met_u && met_u->ste_u ) {
    //  Cycle through candidates to find one matching this context
    for ( u3m_stencil_link* cur_u = met_u->ste_u; cur_u; cur_u = cur_u->nex_u ) {
      if ( c3y == _m_spot_match(cur_u->ste_u, cor) ) {
        return cur_u->ste_u;
      }
    }
  }

  //  Get battery (head of core)
  u3_noun bat = u3h(cor);
  if ( c3n == u3a_is_cell(bat) ) {
    return NULL;
  }

  //  Check for battery metadata
  u3m_meta* bat_met_u = u3m_meta_get(bat);
  if ( !bat_met_u || !bat_met_u->bat_p ) {
    return NULL;
  }

  //  Use battery spotter
  u3m_battery* bat_u = u3to(u3m_battery, bat_met_u->bat_p);
  return u3m_spot_battery(bat_u, u3t(cor));
}

/// u3m_spot_battery(): find stencil via battery spotter
///
u3m_stencil*
u3m_spot_battery(u3m_battery* bat_u, u3_noun pay)
{
  //  TODO: implement compiled spotter
  //  For now, linear search through stencils

  //  Check root stencils (payload is kelvin atom)
  if ( c3y == u3a_is_atom(pay) && bat_u->rot_p ) {
    //  TODO: lookup by kelvin
  }

  //  Check child stencils (payload is cell, walk to parent)
  if ( c3y == u3a_is_cell(pay) && bat_u->kid_p ) {
    //  TODO: recursive spot on parent
  }

  return NULL;
}

/// _m_find_kernel(): find kernel by name path
///
/// Walks the kernel tree to find a matching kernel.
/// For root kernels, matches by name and kelvin.
/// For child kernels, recursively matches parent then child.
///
static u3m_kernel*
_m_find_kernel(u3_noun nam, u3_atom axe, u3_noun pay)
{
  //  For root cores (axe == 0), payload is kelvin
  if ( 0 == axe || (c3y == u3a_is_cat(axe) && 0 == axe) ) {
    if ( c3n == u3a_is_atom(pay) ) {
      return NULL;
    }

    //  Look up root kernel by [name kelvin]
    if ( !_kernel_roots_p ) {
      return NULL;
    }

    c3_c* nam_c = u3r_string(nam);
    u3_noun key = u3nc(u3k(nam), u3k(pay));
    u3_weak got = ga_kernel_map_get(_kernel_roots_p, key);
    u3z(key);
    c3_free(nam_c);

    if ( u3_none == got ) {
      return NULL;
    }
    return u3to(u3m_kernel, got);
  }

  //  For child cores, need to find parent first
  //  This requires walking up the payload to find the parent core
  //  For now, just check if the battery has a stencil pointing to a kernel
  //  and look for a child with matching name and axis

  return NULL;  //  TODO: implement parent walking
}

/// u3m_mine(): register a core from %fast hint
///
/// Creates a stencil for the core and stores it in metadata.
///
void
u3m_mine(u3_noun clu, u3_noun cor)
{
  u3_noun p_clu, q_clu, r_clu;

  //  Must be a cell
  if ( c3n == u3du(cor) ) {
    u3z(clu);
    u3z(cor);
    return;
  }

  //  Parse clue: [name parent-formula hooks]
  if ( c3n == u3r_trel(clu, &p_clu, &q_clu, &r_clu) ) {
    u3z(clu);
    u3z(cor);
    return;
  }

  //  Extract parent axis from formula
  //  The formula is [0 axe] for fragments, [1 0] for root
  u3_atom axe = 0;
  while ( _(u3du(q_clu)) && (11 == u3h(q_clu)) ) {
    q_clu = u3t(u3t(q_clu));
  }
  if ( _(u3du(q_clu)) ) {
    if ( (1 == u3h(q_clu)) && (0 == u3t(q_clu)) ) {
      axe = 0;  //  root
    }
    else if ( (0 == u3h(q_clu)) && _(u3a_is_cat(u3t(q_clu))) ) {
      axe = u3t(q_clu);
    }
  }

  //  Try to find matching kernel
  u3m_kernel* ker_u = _m_find_kernel(p_clu, axe, u3t(cor));

  if ( ker_u && c3y == ker_u->sta_o ) {
    //  Static kernel - intern the whole core
    u3_noun can = u3i_intern_k(cor);

    //  Extend to get metadata
    u3_noun ext = u3m_extend(can);
    u3m_meta* met_u = u3m_meta_of(ext);

    //  Create stencil and add to candidates list
    u3m_stencil* ste_u = u3m_stencil_new(ker_u, u3h(cor));
    ste_u->sta_o = c3y;
    ste_u->who_p = u3of(u3m_meta, met_u);
    //  Add to stencil candidates (u3m_stencil_add handles duplicates)
    u3m_stencil_add(met_u, ste_u);

    if ( ext != can ) u3z(ext);
    u3z(can);
  }

  u3z(clu);
  u3z(cor);
}

/// u3m_kick(): try to kick via jet
///
u3_weak
u3m_kick(u3_noun cor, u3_atom axe)
{
  //  Find stencil
  u3m_stencil* ste_u = u3m_spot(cor);
  if ( !ste_u ) {
    return u3_none;
  }

  //  Find driver for this axis
  u3m_kernel* ker_u = ste_u->ker_u;
  for ( c3_w i = 0; i < ker_u->arm_w; i++ ) {
    if ( c3y == u3r_sing(ker_u->arm_u[i].axe, axe) ) {
      //  Found driver, call it
      return ker_u->arm_u[i].drv_f(cor);
    }
  }

  return u3_none;
}

/// u3m_stencil_init(): initialize stencil system
///
void
u3m_stencil_init(void)
{
  if ( !_kernel_roots_p ) {
    _kernel_roots_p = ga_kernel_map_new();
  }
}

/// u3m_stencil_free_all(): free all stencil state
///
void
u3m_stencil_free_all(void)
{
  if ( _kernel_roots_p ) {
    //  TODO: Walk and free all kernels
    ga_kernel_map_free(_kernel_roots_p);
    _kernel_roots_p = 0;
  }
}

/// u3m_stencil_mark(): mark stencil state for GC
///
c3_w
u3m_stencil_mark(void)
{
  c3_w tot_w = 0;

  if ( _kernel_roots_p ) {
    tot_w += ga_kernel_map_mark(_kernel_roots_p);
  }

  return tot_w;
}

/// u3m_stencil_ream(): rebuild after checkpoint restore
///
void
u3m_stencil_ream(void)
{
  //  Stencils are rebuilt from cold state on demand
  u3m_stencil_free_all();
  u3m_stencil_init();
}
