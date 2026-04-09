/// @file stencil.h
/// Stencil and kernel system for jet dispatch
///
/// Kernels form a tree that mirrors the Hoon kernel structure.
/// Stencils are created when cores are registered via %fast hints.
/// Stencil matching provides O(1) jet dispatch for static cores.

#ifndef U3_STENCIL_H
#define U3_STENCIL_H

#include "c3/c3.h"
#include "types.h"
#include "allocate.h"

//  Forward declarations
struct _u3m_kernel;
struct _u3m_stencil;
struct _u3n_prog;

/// u3m_arm: jetted arm in a kernel
///
typedef struct _u3m_arm {
  u3_atom             axe;      //  axis of arm in battery
  u3_noun           (*drv_f)(u3_noun);  //  driver function
} u3m_arm;

/// u3m_hook: named hook in a kernel
///
typedef struct _u3m_hook {
  c3_c*               nam_c;    //  hook name
  u3_atom             axe;      //  hook axis
} u3m_hook;

/// u3m_kernel: kernel node in the jet tree
///
/// Kernels can be:
/// - Root kernels (payload is kelvin atom)
/// - Static child kernels (parent axis is 1, parent is static)
/// - Dynamic child kernels (parent axis > 1 or parent is dynamic)
///
typedef struct _u3m_kernel {
  c3_c*                       nam_c;    //  kernel name
  struct _u3m_kernel*         par_u;    //  parent kernel (NULL for root)
  c3_o                        sta_o;    //  is static kernel?
  u3_atom                     axe;      //  axis to parent (1 for static)

  //  For root kernels only
  u3_atom                     kel;      //  kelvin number

  //  Arms (jetted functions)
  c3_w                        arm_w;    //  number of arms
  u3m_arm*                    arm_u;    //  array of arms

  //  Hooks (named arm lookups)
  c3_w                        hok_w;    //  number of hooks
  u3m_hook*                   hok_u;    //  array of hooks

  //  Child kernels (hash by [name parent-axis])
  u3p(struct ga_root)         kid_p;    //  children map

  //  Stencils created for this kernel
  u3p(struct ga_root)         ste_p;    //  stencils set
} u3m_kernel;

/// u3m_battery: battery metadata
///
/// Stored in the metadata of battery cells (heads of cores).
///
typedef struct _u3m_battery {
  u3_atom                     axe;      //  parent axis (from %fast clue)
  u3p(struct ga_root)         rot_p;    //  root stencils by kelvin
  u3p(struct ga_root)         kid_p;    //  child stencils by parent stencil
  u3p(void)                   spo_p;    //  compiled spotter function (cache)
} u3m_battery;

/// u3m_stencil: stencil for jet matching
///
/// A stencil represents a registered core shape. Static stencils
/// intern the whole core for O(1) matching. Dynamic stencils only
/// intern the battery and must walk to parent for matching.
///
typedef struct _u3m_stencil {
  u3m_kernel*                 ker_u;    //  kernel this stencil belongs to
  u3p(struct _u3m_meta)       bat_p;    //  battery metadata pointer

  //  For static stencils: whole core is interned
  //  For dynamic stencils: only battery is interned, parent stencil stored
  c3_o                        sta_o;    //  is static stencil?
  union {
    u3p(struct _u3m_meta)     who_p;    //  static: whole core metadata
    struct _u3m_stencil*      par_u;    //  dynamic: parent stencil
  };
} u3m_stencil;

//  Kernel tree operations

/// u3m_kernel_new(): create a new kernel
///
u3m_kernel* u3m_kernel_new(c3_c* nam_c);

/// u3m_kernel_free(): free a kernel and its resources
///
void u3m_kernel_free(u3m_kernel* ker_u);

/// u3m_kernel_forwarded(): get forwarded kernel pointer after reap
///
/// After kernel reaping, returns the new kernel pointer from the
/// forwarding pointer stored in the old kernel's location.
///
u3m_kernel* u3m_kernel_forwarded(u3m_kernel* old_u);

/// u3m_kernel_reap(): take kernels from child road into parent
///
/// Called BEFORE u3a_drop_heap() so child data is accessible.
/// Stores forwarding pointers in old kernels for stencil fixup.
///
void u3m_kernel_reap(u3p(struct ga_root) ker_p);

/// u3m_root_install(): install a root kernel
///
/// Root kernels have a kelvin number and no parent.
///
u3m_kernel* u3m_root_install(c3_c* nam_c, u3_atom kel);

/// u3m_child_install(): install a child kernel
///
/// If axe == 1 and parent is static, child is static.
/// Otherwise child is dynamic.
///
u3m_kernel* u3m_child_install(u3m_kernel* par_u, c3_c* nam_c, u3_atom axe);

/// u3m_arm_install(): install an arm driver on a kernel
///
void u3m_arm_install(u3m_kernel* ker_u, u3_atom axe, u3_noun (*drv_f)(u3_noun));

/// u3m_hook_install(): install a hook on a kernel
///
void u3m_hook_install(u3m_kernel* ker_u, c3_c* nam_c, u3_atom axe);

/// u3m_hook_axis(): look up hook axis by name
///
/// Returns u3_none if hook not found.
///
u3_weak u3m_hook_axis(u3m_kernel* ker_u, c3_c* nam_c);

//  Battery operations

/// u3m_battery_new(): create new battery metadata
///
u3m_battery* u3m_battery_new(u3_atom axe);

/// u3m_battery_free(): free battery metadata
///
void u3m_battery_free(u3m_battery* bat_u);

/// u3m_battery_get(): get or create battery metadata for a cell
///
/// If the cell doesn't have battery metadata, creates it.
///
u3m_battery* u3m_battery_get(u3_noun bat);

//  Stencil operations

/// u3m_stencil_new(): create a new stencil
///
u3m_stencil* u3m_stencil_new(u3m_kernel* ker_u, u3_noun bat);

/// u3m_stencil_free(): free a stencil
///
void u3m_stencil_free(u3m_stencil* ste_u);

/// u3m_spot(): find stencil for a core
///
/// Returns the stencil matching the core, or NULL if none.
/// Uses O(1) lookup for static cores, walks to parent for dynamic.
///
u3m_stencil* u3m_spot(u3_noun cor);

/// u3m_spot_battery(): find stencil via battery spotter
///
/// Uses compiled spotter function if available.
///
u3m_stencil* u3m_spot_battery(u3m_battery* bat_u, u3_noun pay);

//  Registration (from %fast hints)

/// u3m_mine(): register a core from %fast hint
///
/// Creates stencil and installs drivers on arm programs.
///
void u3m_mine(u3_noun clu, u3_noun cor);

//  Jet dispatch

/// u3m_kick(): try to kick via jet
///
/// Returns result if jet fired, u3_none if should fall back to Nock.
///
u3_weak u3m_kick(u3_noun cor, u3_atom axe);

//  Lifecycle

/// u3m_stencil_init(): initialize stencil system
///
void u3m_stencil_init(void);

/// u3m_stencil_free_all(): free all stencil state
///
void u3m_stencil_free_all(void);

/// u3m_stencil_mark(): mark stencil state for GC
///
c3_w u3m_stencil_mark(void);

/// u3m_stencil_ream(): rebuild after checkpoint restore
///
void u3m_stencil_ream(void);

#endif /* ifndef U3_STENCIL_H */
