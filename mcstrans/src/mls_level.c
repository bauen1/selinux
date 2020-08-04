#include <stdio.h>
#include "mls_level.h"
#include <sepol/policydb/ebitmap.h>
#include "util.h"

mls_level_t *mls_level_from_string(char *mls_context)
{
	char delim;
	const char *scontextp;
	char *p, *lptr;
	mls_level_t *l;

	if (!mls_context) {
		return NULL;
	}

	l = (mls_level_t *) calloc(1, sizeof(mls_level_t));

	/* Extract low sensitivity. */
	scontextp = p = mls_context;
	while (*p && *p != ':' && *p != '-') {
		p++;
	}

	delim = *p;
	if (delim != 0) {
		*p++ = 0;
	}

	if (*scontextp != 's') {
		goto err;
	}
	l->sens = atoi(scontextp + 1);

	if (delim == ':') {
		/* Extract category set. */
		while (1) {
			scontextp = p;
			while (*p && *p != ',' && *p != '-')
				p++;
			delim = *p;
			if (delim != 0)
				*p++ = 0;

			/* Separate into level if exists */
			if ((lptr = strchr(scontextp, '.')) != NULL) {
				/* Remove '.' */
				*lptr++ = 0;
			}

			if (*scontextp != 'c')
				goto err;
			int bit = atoi(scontextp + 1);
			if (ebitmap_set_bit(&l->cat, bit, 1))
				goto err;

			/* If level, set all categories in level */
			if (lptr) {
				if (*lptr != 'c')
					goto err;
				int ubit = atoi(lptr + 1);
				int i;
				for (i = bit + 1; i <= ubit; i++) {
					if (ebitmap_set_bit
						(&l->cat, i, 1))
						goto err;
				}
			}

			if (delim != ',')
				break;
		}
	}

	return l;

      err:
	free(l);
	return NULL;
}

/*
 * Return the length in bytes for the MLS fields of the
 * security context string representation of `context'.
 */
unsigned int mls_compute_string_len(const mls_level_t *l)
{
	unsigned int len = 0;
	char temp[16];
	unsigned int i, level = 0;
	ebitmap_node_t *cnode;

	if (!l)
		return 0;

	len += snprintf(temp, sizeof(temp), "s%u", l->sens);

	ebitmap_for_each_bit(&l->cat, cnode, i) {
		if (ebitmap_node_get_bit(cnode, i)) {
			if (level) {
				level++;
				continue;
			}

			len++; /* : or ,` */

			len += snprintf(temp, sizeof(temp), "c%u", i);
			level++;
		} else {
			if (level > 1)
				len += snprintf(temp, sizeof(temp), ".c%u", i-1);
			level = 0;
		}
	}

	/* Handle case where last category is the end of level */
	if (level > 1)
		len += snprintf(temp, sizeof(temp), ".c%u", i-1);
	return len;
}

char *mls_level_to_string(const mls_level_t *l)
{
	unsigned int wrote_sep, len = mls_compute_string_len(l);
	unsigned int i, level = 0;
	ebitmap_node_t *cnode;
	wrote_sep = 0;

	if (len == 0)
		return NULL;
	char *result = (char *)malloc(len + 1);
	char *p = result;

	p += sprintf(p, "s%u", l->sens);

	/* categories */
	ebitmap_for_each_bit(&l->cat, cnode, i) {
		if (ebitmap_node_get_bit(cnode, i)) {
			if (level) {
				level++;
				continue;
			}

			if (!wrote_sep) {
				*p++ = ':';
				wrote_sep = 1;
			} else
				*p++ = ',';
			p += sprintf(p, "c%u", i);
			level++;
		} else {
			if (level > 1) {
				if (level > 2)
					*p++ = '.';
				else
					*p++ = ',';

				p += sprintf(p, "c%u", i-1);
			}
			level = 0;
		}
	}
	/* Handle case where last category is the end of level */
	if (level > 1) {
		if (level > 2)
			*p++ = '.';
		else
			*p++ = ',';

		p += sprintf(p, "c%u", i-1);
	}

	*(result + len) = 0;
	return result;
}

static int
numdigits(unsigned int n)
{
	int count = 1;
	while ((n = n / 10)) {
		count++;
	}
	return count;
}

static int
parse_category(ebitmap_t *e, const char *raw, int allowinverse)
{
	int inverse = 0;
	unsigned int low, high;
	while (*raw) {
		if (allowinverse && *raw == '~') {
			inverse = !inverse;
			raw++;
			continue;
		}
		if (sscanf(raw,"c%u", &low) != 1)
			return -1;
		raw += numdigits(low) + 1;
		if (*raw == '.') {
			raw++;
			if (sscanf(raw,"c%u", &high) != 1)
				return -1;
			raw += numdigits(high) + 1;
		} else {
			high = low;
		}
		while (low <= high) {
			if (low >= maxbit)
				maxbit = low + 1;
			if (ebitmap_set_bit(e, low, inverse ? 0 : 1) < 0)
				return -1;
			low++;
		}
		if (*raw == ',') {
			raw++;
			inverse = 0;
		} else if (*raw != '\0') {
			return -1;
		}
	}
	return 0;
}

int
parse_ebitmap(ebitmap_t *e, const ebitmap_t *def, const char *raw) {
	int rc = ebitmap_cpy(e, def);
	if (rc < 0)
		return rc;
	rc = parse_category(e, raw, 1);
	if (rc < 0)
		return rc;
	return 0;
}

mls_level_t *
parse_raw(const char *raw) {
	mls_level_t *mls = calloc(1, sizeof(mls_level_t));
	if (mls == NULL) {
		goto err;
	}

	if (sscanf(raw,"s%u", &mls->sens) != 1) {
		goto err;
	}

	raw += numdigits(mls->sens) + 1;
	if (*raw == ':') {
		raw++;
		if (parse_category(&mls->cat, raw, 0) < 0) {
			goto err;
		}
	} else if (*raw != '\0') {
		goto err;
	}

	return mls;

err:
	ebitmap_destroy(&mls->cat);
	free(mls);
	return NULL;
}
