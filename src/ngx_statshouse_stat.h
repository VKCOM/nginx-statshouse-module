/* Copyright 2022 V Kontakte LLC
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _NGX_STATSHOUSE_STAT_H_INCLUDED_
#define _NGX_STATSHOUSE_STAT_H_INCLUDED_

#include <ngx_core.h>
#include <ngx_rbtree.h>


#define NGX_STATSHOUSE_STAT_KEYS_MAX     (16 + 1) /* +1 skey */


typedef enum {
    ngx_statshouse_mt_counter          = 1,
    ngx_statshouse_mt_value,
    ngx_statshouse_mt_unique,
} ngx_statshouse_stat_type_e;

typedef union {
    double                               counter;
    double                               value;
    int64_t                              unique;
} ngx_statshouse_stat_value_t;

typedef struct {
    ngx_str_t                            name;
    ngx_str_t                            value;
} ngx_statshouse_stat_key_t;

typedef struct {
    ngx_statshouse_stat_value_t          value;
    ngx_statshouse_stat_type_e           type;

    ngx_str_t                            name;

    ngx_statshouse_stat_key_t            keys[NGX_STATSHOUSE_STAT_KEYS_MAX];
    ngx_int_t                            keys_count;
} ngx_statshouse_stat_t;


#endif
