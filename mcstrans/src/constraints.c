#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <selinux/selinux.h>
#include <selinux/context.h>
#include <syslog.h>

#include "mls_level.h"
#include "util.h"

/* Data structures */

typedef struct sens_constraint {
	char op;
	char *text;
	unsigned int sens;
	ebitmap_t cat;
	struct sens_constraint *next;
} sens_constraint_t;

static sens_constraint_t *sens_constraints;

typedef struct cat_constraint {
	char op;
	char *text;
	int nbits;
	ebitmap_t mask;
	ebitmap_t cat;
	struct cat_constraint *next;
} cat_constraint_t;

static cat_constraint_t *cat_constraints;

int
add_constraint(char op, const char *raw, const char *tok) {
	log_debug("%s\n", "add_constraint");
	ebitmap_t empty;
	ebitmap_init(&empty);
	if (raw == NULL || *raw == 0) {
		syslog(LOG_ERR, "unable to parse line");
		return -1;
	}

	if (*raw == 's') {
		sens_constraint_t *constraint = calloc(1, sizeof(sens_constraint_t));
		if (!constraint) {
			log_error("allocation error %s", strerror(errno));
			return -1;
		}
		if (sscanf(raw,"s%u", &constraint->sens) != 1) {
			syslog(LOG_ERR, "unable to parse level");
			free(constraint);
			return -1;
		}
		if (parse_ebitmap(&constraint->cat, &empty, tok) < 0) {
			syslog(LOG_ERR, "unable to parse cat");
			free(constraint);
			return -1;
		}
		if (asprintf(&constraint->text, "%s%c%s", raw, op, tok) < 0) {
			log_error("asprintf failed %s", strerror(errno));
			return -1;
		}
		constraint->op = op;
		sens_constraint_t **p;
		for (p= &sens_constraints; *p; p = &(*p)->next) {
			;
		}
		*p = constraint;
		return 0;
	} else if (*raw == 'c' ) {
		cat_constraint_t *constraint = calloc(1, sizeof(cat_constraint_t));
		if (!constraint) {
			log_error("allocation error %s", strerror(errno));
			return -1;
		}
		if (parse_ebitmap(&constraint->mask, &empty, raw) < 0) {
			syslog(LOG_ERR, "unable to parse mask");
			free(constraint);
			return -1;
		}
		if (parse_ebitmap(&constraint->cat, &empty, tok) < 0) {
			syslog(LOG_ERR, "unable to parse cat");
			ebitmap_destroy(&constraint->mask);
			free(constraint);
			return -1;
		}
		if (asprintf(&constraint->text, "%s%c%s", raw, op, tok) < 0) {
			log_error("asprintf failed %s", strerror(errno));
			return -1;
		}
		constraint->nbits = ebitmap_cardinality(&constraint->cat);
		constraint->op = op;
		cat_constraint_t **p;
		for (p= &cat_constraints; *p; p = &(*p)->next) {
			;
		}
		*p = constraint;
		return 0;
	} else {
		return -1;
	}

	return 0;
}

int
violates_constraints(const mls_level_t *l) {
	int nbits;
	sens_constraint_t *s;
	ebitmap_t common;
	for (s=sens_constraints; s; s=s->next) {
		if (s->sens == l->sens) {
			if (ebitmap_and(&common, &s->cat, &l->cat) < 0) {
				return 1;
			}
			nbits = ebitmap_cardinality(&common);
			ebitmap_destroy(&common);
			if (nbits) {
				char *text = mls_level_to_string(l);
				syslog(LOG_WARNING, "%s violates %s", text, s->text);
				free(text);
				return 1;
			}
		}
	}
	cat_constraint_t *c;
	for (c=cat_constraints; c; c=c->next) {
		if (ebitmap_and(&common, &c->mask, &l->cat) < 0) {
			return 1;
		}
		nbits = ebitmap_cardinality(&common);
		ebitmap_destroy(&common);
		if (nbits > 0) {
			if (ebitmap_and(&common, &c->cat, &l->cat) < 0) {
				return 1;
			}
			nbits = ebitmap_cardinality(&common);
			ebitmap_destroy(&common);
			if ((c->op == '!' && nbits) ||
			    (c->op == '>' && nbits != c->nbits)) {
				char *text = mls_level_to_string(l);
				syslog(LOG_WARNING, "%s violates %s (%d,%d)", text, c->text, nbits, c->nbits);
				free(text);
				return 1;
			}
		}
	}
	return 0;
}

void
destroy_sens_constraint(sens_constraint_t **list, sens_constraint_t *constraint) {
	if (!constraint) {
		return;
	}
	for (; list && *list; list = &(*list)->next) {
		if (*list == constraint) {
			*list = constraint->next;
			break;
		}
	}
	ebitmap_destroy(&constraint->cat);
	free(constraint->text);
	memset(constraint, 0, sizeof(sens_constraint_t));
	free(constraint);
}

void
destroy_cat_constraint(cat_constraint_t **list, cat_constraint_t *constraint) {
	if (!constraint) {
		return;
	}

	for (; list && *list; list = &(*list)->next) {
		if (*list == constraint) {
			*list = constraint->next;
			break;
		}
	}

	ebitmap_destroy(&constraint->mask);
	ebitmap_destroy(&constraint->cat);
	free(constraint->text);
	memset(constraint, 0, sizeof(cat_constraint_t));
	free(constraint);
}

void
finish_constraints(void) {
	while(sens_constraints != NULL) {
		sens_constraint_t *next = sens_constraints->next;
		destroy_sens_constraint(&sens_constraints, sens_constraints);
		sens_constraints = next;
	}

	while(cat_constraints != NULL) {
		cat_constraint_t *next = cat_constraints->next;
		destroy_cat_constraint(&cat_constraints, cat_constraints);
		cat_constraints = next;
	}
}
