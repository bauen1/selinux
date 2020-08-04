#include "mls_level.h"

int add_constraint(char op, const char *raw, const char *tok);

int violates_constraints(const mls_level_t *l);

void finish_constraints(void);
