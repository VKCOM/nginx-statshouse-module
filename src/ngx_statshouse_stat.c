/* Copyright 2022 V Kontakte LLC
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <ngx_core.h>

#include "ngx_statshouse_stat.h"


void
ngx_statshouse_stat_init(ngx_statshouse_stat_t *stat, ngx_str_t name, ngx_statshouse_stat_type_e type)
{
    stat->name = name;
    stat->type = type;

    stat->keys_count = 0;
    stat->values_count = 0;
}


void
ngx_statshouse_stat_value_zero(ngx_statshouse_stat_t *stat)
{
    ngx_statshouse_stat_value_t  value = { 0 };
    ngx_statshouse_stat_value(stat, value);
}


void
ngx_statshouse_stat_value(ngx_statshouse_stat_t *stat, ngx_statshouse_stat_value_t value)
{
    stat->values[stat->values_count++] = value;
}


void
ngx_statshouse_stat_value_counter(ngx_statshouse_stat_t *stat, int64_t counter)
{
    stat->values[stat->values_count++].counter = counter;
}

void
ngx_statshouse_stat_value_value(ngx_statshouse_stat_t *stat, double value)
{
    stat->values[stat->values_count++].value = value;
}


void
ngx_statshouse_stat_value_nvalue(ngx_statshouse_stat_t *stat, double value)
{
    stat->values[stat->values_count++].value = -value;
}


void
ngx_statshouse_stat_value_unique(ngx_statshouse_stat_t *stat, double unique)
{
    stat->values[stat->values_count++].unique = unique;
}


void
ngx_statshouse_stat_key(ngx_statshouse_stat_t *stat, ngx_str_t name, ngx_str_t value)
{
    ngx_statshouse_stat_key_t  *key;

    key = &stat->keys[stat->keys_count++];

    key->name = name;
    key->value = value;
}
