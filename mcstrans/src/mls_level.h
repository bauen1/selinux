#ifndef __mls_level_h__
#define __mls_level_h__

#include <sepol/policydb/mls_types.h>

unsigned int mls_compute_string_len(const mls_level_t *r);
mls_level_t *mls_level_from_string(char *mls_context);
char *mls_level_to_string(const mls_level_t *r);

int parse_ebitmap(ebitmap_t *e, const ebitmap_t *def, const char *raw);

mls_level_t *parse_raw(const char *raw);

#endif
