/* Copyright 2022 V Kontakte LLC
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _NGX_STATSHOUSE_H_INCLUDED_
#define _NGX_STATSHOUSE_H_INCLUDED_

#include <ngx_core.h>
#include <ngx_event.h>

#include "ngx_statshouse_stat.h"
#include "ngx_statshouse_aggregate.h"


typedef ngx_int_t (*ngx_statshouse_complex_value_pt)(void *ctx, void *val, ngx_str_t *value);


typedef struct {
    void                                *complex;
    ngx_statshouse_stat_type_e           type;

    ngx_flag_t                           split;
} ngx_statshouse_conf_value_t;

typedef struct {
    void                                *complex;
    ngx_str_t                            name;

    ngx_flag_t                           split;
} ngx_statshouse_conf_key_t;

typedef struct {
    ngx_str_t                            name;
    ngx_str_t                            phase;

    ngx_array_t                         *condition;

    time_t                               timeout;
    time_t                               last;

    ngx_statshouse_conf_value_t          value;
    ngx_statshouse_conf_key_t            keys[NGX_STATSHOUSE_STAT_KEYS_MAX];
} ngx_statshouse_conf_t;

typedef struct {
    ngx_url_t                            addr;
    ngx_connection_t                    *connection;

    ngx_buf_t                           *buffer;
    ssize_t                              buffer_size;

    ngx_flag_t                           flush_after_request;

    ngx_statshouse_stat_t               *splits;
    ngx_int_t                            splits_max;

    ngx_msec_t                           flush;
    ngx_event_t                          flush_event;
    ngx_connection_t                     flush_connection;

    ngx_statshouse_aggregate_t          *aggregate;
    size_t                               aggregate_size;
} ngx_statshouse_server_t;


ngx_int_t  ngx_statshouse_server_init(ngx_statshouse_server_t *server, ngx_pool_t *pool);

ngx_int_t  ngx_statshouse_send(ngx_statshouse_server_t *server, ngx_statshouse_stat_t *stat);
ngx_int_t  ngx_statshouse_flush(ngx_statshouse_server_t *server);
ngx_int_t  ngx_statshouse_flush_after_request(ngx_statshouse_server_t *server);

ngx_int_t  ngx_statshouse_stat_compile(ngx_statshouse_conf_t *conf, ngx_statshouse_stat_t *stats, ngx_int_t max,
    ngx_statshouse_complex_value_pt complex, void *complex_ctx);


#endif
