/* Copyright 2022 V Kontakte LLC
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _NGX_STATSHOUSE_AGGREGATE_H_INCLUDED_
#define _NGX_STATSHOUSE_AGGREGATE_H_INCLUDED_

#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_statshouse_stat.h>


typedef ngx_int_t (*ngx_statshouse_aggregate_pt)(ngx_statshouse_stat_t *stat, void *ctx);


typedef struct {
    ngx_statshouse_aggregate_pt   handler;
    void                         *ctx;

    ngx_buf_t                     alloc;

    ngx_rbtree_t                  rbtree;
    ngx_rbtree_node_t             sentinel;

    ngx_queue_t                   queue;

    ngx_event_t                   timer_event;
    ngx_connection_t              timer_connection;

    ngx_msec_t                    interval;
    ngx_int_t                     values;
    size_t                        size;
} ngx_statshouse_aggregate_t;


ngx_int_t  ngx_statshouse_aggregate_init(ngx_statshouse_aggregate_t *aggregate, ngx_pool_t *pool);
ngx_int_t  ngx_statshouse_aggregate(ngx_statshouse_aggregate_t *aggregate, ngx_statshouse_stat_t *stat, ngx_msec_t now);
ngx_int_t  ngx_statshouse_aggregate_process(ngx_statshouse_aggregate_t *aggregate, ngx_msec_t now);

#endif
