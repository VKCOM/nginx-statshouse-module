/* Copyright 2022 V Kontakte LLC
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _NGX_STREAM_STATSHOUSE_H_INCLUDED_
#define _NGX_STREAM_STATSHOUSE_H_INCLUDED_

#include <ngx_core.h>
#include <ngx_stream.h>

#include <ngx_statshouse_stat.h>


ngx_stream_session_t  *ngx_stream_statshouse_make_session(ngx_cycle_t *cycle,
    ngx_pool_t *pool, ngx_stream_conf_ctx_t *ctx);

ngx_int_t  ngx_stream_statshouse_send(ngx_stream_session_t *session, ngx_str_t *phase);
ngx_int_t  ngx_stream_statshouse_send_stat(ngx_stream_session_t *session, ngx_statshouse_stat_t *stat);
ngx_int_t  ngx_stream_statshouse_flush(ngx_stream_session_t *session);


ngx_int_t  ngx_stream_statshouse_send_ctx(ngx_cycle_t *cycle, ngx_stream_conf_ctx_t *ctx,
    ngx_pool_t *pool, ngx_str_t *phase);
ngx_int_t  ngx_stream_statshouse_send_stream(ngx_cycle_t *cycle, ngx_pool_t *pool, ngx_str_t *phase);
ngx_int_t  ngx_stream_statshouse_send_server(ngx_cycle_t *cycle, ngx_stream_core_srv_conf_t *server,
    ngx_pool_t *pool, ngx_str_t *phase);


ngx_int_t  ngx_stream_statshouse_send_stat_ctx(ngx_cycle_t *cycle, ngx_stream_conf_ctx_t *ctx,
    ngx_pool_t *pool, ngx_statshouse_stat_t *stat);
ngx_int_t  ngx_stream_statshouse_send_stat_stream(ngx_cycle_t *cycle, ngx_pool_t *pool, ngx_statshouse_stat_t *stat);
ngx_int_t  ngx_stream_statshouse_send_stat_server(ngx_cycle_t *cycle, ngx_stream_core_srv_conf_t *server,
    ngx_pool_t *pool, ngx_statshouse_stat_t *stat);


ngx_int_t  ngx_stream_statshouse_flush_ctx(ngx_cycle_t *cycle, ngx_stream_conf_ctx_t *ctx, ngx_pool_t *pool);
ngx_int_t  ngx_stream_statshouse_flush_stream(ngx_cycle_t *cycle, ngx_pool_t *pool);
ngx_int_t  ngx_stream_statshouse_flush_server(ngx_cycle_t *cycle, ngx_stream_core_srv_conf_t *server, ngx_pool_t *pool);


#endif
