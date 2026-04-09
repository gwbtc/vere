

#define HEADER_MODE
#define NAME ga_noun_map
#define KEY_TY u3_noun
#define KEY_K u3k
#define KEY_DTOR_FN u3z
#define KEY_MARK u3a_mark_noun
#define KEY_COUNT u3a_count_noun
#define KEY_DISCOUNT u3a_discount_noun
#define KEY_RELOCATE u3a_relocate_noun
#define VAL_TY u3_noun
#define VAL_K u3k
#define VAL_DTOR_FN u3z
#define VAL_MARK u3a_mark_noun
#define VAL_COUNT u3a_count_noun
#define VAL_DISCOUNT u3a_discount_noun
#define VAL_RELOCATE u3a_relocate_noun
#define HASH_FN u3r_mug
#define CMPR_FN u3r_sing
#include "gashtable.h"

#define HEADER_MODE
#define NAME ga_intern_set
#define KEY_TY u3_noun
#define KEY_K u3k
#define KEY_DTOR_FN u3z
#define KEY_MARK u3a_mark_noun
#define KEY_COUNT u3a_count_noun
#define KEY_DISCOUNT u3a_discount_noun
#define KEY_RELOCATE u3a_relocate_noun
#define HASH_FN u3r_mug
#define CMPR_FN u3r_sing
#include "gashtable.h"

//  Forward declaration for stencil.h types
struct _u3m_kernel;
struct _u3m_stencil;

#define HEADER_MODE
#define NAME ga_kernel_map
#define KEY_TY u3_noun
#define KEY_K u3k
#define KEY_DTOR_FN u3z
#define KEY_MARK u3a_mark_noun
#define KEY_COUNT u3a_count_noun
#define KEY_DISCOUNT u3a_discount_noun
#define KEY_RELOCATE u3a_relocate_noun
#define VAL_TY u3p(struct _u3m_kernel)
#define HASH_FN u3r_mug
#define CMPR_FN u3r_sing
#include "gashtable.h"

#define HEADER_MODE
#define NAME ga_stencil_set
#define KEY_TY u3p(struct _u3m_stencil)
#define HASH_FN(x) ((c3_w)(x))
#define CMPR_FN(a, b) ((a) == (b) ? c3y : c3n)
#include "gashtable.h"

