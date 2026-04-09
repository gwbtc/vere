/// @file intern.h
/// Noun interning (canonicalization)
///
/// The interner maintains canonical references for nouns.
/// When a noun is interned, if an equal noun already exists in the
/// interner, the existing reference is returned. This enables
/// O(1) identity comparison for interned nouns.

#ifndef U3_INTERN_H
#define U3_INTERN_H

#include "c3/c3.h"
#include "types.h"

//  Forward declaration
struct ga_root;

/// u3i_intern(): intern a noun, returning canonical reference
///
/// If an equal noun is already interned, returns that reference
/// and frees the input. Otherwise interns the input and returns it.
///
/// The returned noun is TRANSFERRED (caller owns the reference).
///
u3_noun u3i_intern(u3_noun som);

/// u3i_intern_k(): intern a noun, keeping input reference
///
/// Like u3i_intern but the input reference is RETAINED.
/// The returned noun is TRANSFERRED (caller owns the reference).
///
u3_noun u3i_intern_k(u3_noun som);

/// u3i_is_interned(): check if noun is in the interner
///
c3_o u3i_is_interned(u3_noun som);

/// u3i_canonical(): get canonical form of noun if interned
///
/// Returns the canonical reference if the noun is interned,
/// u3_none otherwise.
///
u3_weak u3i_canonical(u3_noun som);

//  Lifecycle

/// u3i_intern_init(): initialize the interner for current road
///
void u3i_intern_init(void);

/// u3i_intern_free(): free the interner for current road
///
void u3i_intern_free(void);

/// u3i_intern_reap(): promote interner state from child road
///
/// Merges interned nouns from child road into parent.
///
void u3i_intern_reap(u3p(struct ga_root) int_p);

/// u3i_intern_take(): copy interner state for child road
///
u3p(struct ga_root) u3i_intern_take(void);

/// u3i_intern_mark(): mark interner for GC
///
c3_w u3i_intern_mark(void);

/// u3i_intern_ream(): rebuild after checkpoint restore
///
void u3i_intern_ream(void);

#endif /* ifndef U3_INTERN_H */
