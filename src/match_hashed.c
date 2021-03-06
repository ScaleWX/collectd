/**
 * collectd - src/match_hashed.c
 * Copyright (C) 2009       Florian Forster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Florian Forster <octo at collectd.org>
 **/

#include "collectd.h"

#include "filter_chain.h"
#include "utils/common/common.h"

/*
 * private data types
 */
struct mh_hash_match_s {
  uint32_t match;
  uint32_t total;
};
typedef struct mh_hash_match_s mh_hash_match_t;

struct mh_match_s;
typedef struct mh_match_s mh_match_t;
#define EXTRA_MATCH_HOST	0x0001
#define EXTRA_MATCH_PLUG	0x0002
#define EXTRA_MATCH_TYPE	0x0004
struct mh_match_s {
  uint32_t flags;
  mh_hash_match_t *matches;
  size_t matches_num;
};

/*
 * internal helper functions
 */
static int mh_config_match(const oconfig_item_t *ci, /* {{{ */
                           mh_match_t *m) {
  mh_hash_match_t *tmp;

  if ((ci->values_num != 2) || (ci->values[0].type != OCONFIG_TYPE_NUMBER) ||
      (ci->values[1].type != OCONFIG_TYPE_NUMBER)) {
    ERROR("hashed match: The `Match' option requires "
          "exactly two numeric arguments.");
    return -1;
  }

  if ((ci->values[0].value.number < 0) || (ci->values[1].value.number < 0)) {
    ERROR("hashed match: The arguments of the `Match' "
          "option must be positive.");
    return -1;
  }

  tmp = realloc(m->matches, sizeof(*tmp) * (m->matches_num + 1));
  if (tmp == NULL) {
    ERROR("hashed match: realloc failed.");
    return -1;
  }
  m->matches = tmp;
  tmp = m->matches + m->matches_num;

  tmp->match = (uint32_t)(ci->values[0].value.number + .5);
  tmp->total = (uint32_t)(ci->values[1].value.number + .5);

  if (tmp->match >= tmp->total) {
    ERROR("hashed match: The first argument of the `Match' option "
          "must be smaller than the second argument.");
    return -1;
  }
  assert(tmp->total != 0);

  m->matches_num++;
  return 0;
} /* }}} int mh_config_match */

static int mh_config_match_fields(const oconfig_item_t *ci,
                                  mh_match_t *m)
{
  if (ci->values_num == 1 &&
      ci->values[0].type == OCONFIG_TYPE_STRING &&
      strcmp(ci->values[0].value.string, "plugin") == 0)
    m->flags |= EXTRA_MATCH_PLUG;
  else if(ci->values_num == 2 &&
      ci->values[0].type == OCONFIG_TYPE_STRING &&
      strcmp(ci->values[0].value.string, "plugin") == 0 &&
      ci->values[1].type == OCONFIG_TYPE_STRING &&
      strcmp(ci->values[1].value.string, "type") == 0)
    m->flags |= (EXTRA_MATCH_PLUG | EXTRA_MATCH_TYPE);
  else {
    ERROR ("hashed match: The '%s' option format is "
           "ExtraMatchFields <plugin> [type]!", ci->key);
    return (-1);
  }

  return 0;
}

static int mh_create(const oconfig_item_t *ci, void **user_data) /* {{{ */
{
  mh_match_t *m;

  m = calloc(1, sizeof(*m));
  if (m == NULL) {
    ERROR("mh_create: calloc failed.");
    return -ENOMEM;
  }

  m->flags = EXTRA_MATCH_HOST;
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("Match", child->key) == 0)
      mh_config_match(child, m);
    else if (strcasecmp("ExtraMatchFields", child->key) == 0)
      mh_config_match_fields(child, m);
    else
      ERROR("hashed match: No such config option: %s", child->key);
  }

  if (m->matches_num == 0) {
    sfree(m->matches);
    sfree(m);
    ERROR("hashed match: No matches were configured. Not creating match.");
    return -1;
  }

  *user_data = m;
  return 0;
} /* }}} int mh_create */

static int mh_destroy(void **user_data) /* {{{ */
{
  mh_match_t *mh;

  if ((user_data == NULL) || (*user_data == NULL))
    return 0;

  mh = *user_data;
  sfree(mh->matches);
  sfree(mh);

  return 0;
} /* }}} int mh_destroy */

static int mh_match(const data_set_t __attribute__((unused)) * ds, /* {{{ */
                    const value_list_t *vl,
                    notification_meta_t __attribute__((unused)) * *meta,
                    void **user_data) {
  mh_match_t *m;
  uint32_t hash_val;

  if ((user_data == NULL) || (*user_data == NULL))
    return -1;

  m = *user_data;

  hash_val = 0;

  for (const char *host_ptr = vl->host; *host_ptr != 0; host_ptr++) {
    /* 2184401929 is some appropriately sized prime number. */
    hash_val = (hash_val * UINT32_C(2184401929)) + ((uint32_t)*host_ptr);
  }

  if (m->flags & EXTRA_MATCH_PLUG) {
    for (const char *plugin_ptr = vl->plugin; *plugin_ptr != 0; plugin_ptr++)
    {
      /* 2184401929 is some appropriately sized prime number. */
      hash_val = (hash_val * UINT32_C (2184401929)) + ((uint32_t) *plugin_ptr);
    }
    for (const char *pluginst_ptr = vl->plugin_instance; *pluginst_ptr != 0; pluginst_ptr++)
    {
      /* 2184401929 is some appropriately sized prime number. */
      hash_val = (hash_val * UINT32_C (2184401929)) + ((uint32_t) *pluginst_ptr);
    }
  }

  if (m->flags & EXTRA_MATCH_TYPE) {
    for (const char *type_ptr = vl->type; *type_ptr != 0; type_ptr++)
    {
      /* 2184401929 is some appropriately sized prime number. */
      hash_val = (hash_val * UINT32_C (2184401929)) + ((uint32_t) *type_ptr);
    }
    for (const char *typeinst_ptr = vl->type_instance; *typeinst_ptr != 0; typeinst_ptr++)
    {
      /* 2184401929 is some appropriately sized prime number. */
      hash_val = (hash_val * UINT32_C (2184401929)) + ((uint32_t) *typeinst_ptr);
    }
  }

  DEBUG ("hashed match: host = %s plugin = %s plugin_instance = %s "
         "type = %s type_instance = %s; flags = %x; hash_val = %"PRIu32";",
         vl->host, vl->plugin, vl->plugin_instance, vl->type, vl->type_instance,
         m->flags, hash_val);

  for (size_t i = 0; i < m->matches_num; i++)
    if ((hash_val % m->matches[i].total) == m->matches[i].match) {
      DEBUG("hashed match: hash_val = %"PRIu32"; match = %"PRIu32";",
            hash_val, m->matches[i].match);
      return FC_MATCH_MATCHES;
    }
  return FC_MATCH_NO_MATCH;
} /* }}} int mh_match */

void module_register(void) {
  match_proc_t mproc = {0};

  mproc.create = mh_create;
  mproc.destroy = mh_destroy;
  mproc.match = mh_match;
  fc_register_match("hashed", mproc);
} /* module_register */
