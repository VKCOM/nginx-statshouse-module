/* Copyright 2022 V Kontakte LLC
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <ngx_core.h>
#include <ngx_config.h>
#include <ngx_event.h>
#include <ngx_event_connect.h>
#include <ngx_statshouse_stat.h>

#include "ngx_statshouse.h"
#include "ngx_statshouse_tl.h"


static void       ngx_statshouse_timer_init(ngx_statshouse_server_t *server);
static size_t     ngx_statshouse_server_buffer_left(ngx_statshouse_server_t *server);
static void       ngx_statshouse_read_handler(ngx_event_t *rev);
static ngx_int_t  ngx_statshouse_connect(ngx_statshouse_server_t *server);
static void       ngx_statshouse_disconnect(ngx_statshouse_server_t *server);
static void       ngx_statshouse_timer_handler(ngx_event_t *ev);
static void       ngx_statshouse_timer(ngx_statshouse_server_t *server);
static ngx_int_t  ngx_statshouse_send_to_buffer(ngx_statshouse_server_t *server, ngx_statshouse_stat_t *stat);
static ngx_int_t  ngx_statshouse_aggregate_handler(ngx_statshouse_stat_t *stat, void *ctx);


ngx_int_t
ngx_statshouse_server_init(ngx_statshouse_server_t *server, ngx_pool_t *pool)
{
    server->buffer = ngx_create_temp_buf(pool, server->buffer_size);
    if (server->buffer == NULL) {
        return NGX_ERROR;
    }

    server->splits = ngx_pcalloc(pool, sizeof(ngx_statshouse_stat_t) * server->splits_max);
    if (server->splits == NULL) {
        return NGX_ERROR;
    }

    if (server->aggregate_size) {
        server->aggregate = ngx_pcalloc(pool, sizeof(ngx_statshouse_aggregate_t));
        if (server->aggregate == NULL) {
            return NGX_ERROR;
        }

        server->aggregate->interval = server->flush;
        server->aggregate->size = server->aggregate_size;
        server->aggregate->values = server->aggregate_values;

        server->aggregate->handler = ngx_statshouse_aggregate_handler;
        server->aggregate->ctx = server;
        server->aggregate->log = server->log;

        if (ngx_statshouse_aggregate_init(server->aggregate, pool) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    ngx_statshouse_timer_init(server);

    return NGX_OK;
}


static void
ngx_statshouse_timer_init(ngx_statshouse_server_t *server)
{
    if (server->flush_event.handler != NULL) {
        return;
    }

    server->flush_event.handler = ngx_statshouse_timer_handler;
    server->flush_event.log = server->log;
    server->flush_event.data = &server->flush_connection;
    server->flush_event.cancelable = 1;

    server->flush_connection.fd = -1;
    server->flush_connection.data = server;
}


static size_t
ngx_statshouse_server_buffer_left(ngx_statshouse_server_t *server)
{
    return (size_t) (server->buffer->end - server->buffer->last);
}


static void
ngx_statshouse_read_handler(ngx_event_t *rev)
{
    ngx_connection_t         *connection = rev->data;
    ngx_statshouse_server_t  *server;

    if (connection->close || connection->error) {
        ngx_log_error(NGX_LOG_ERR, connection->log, 0,
            "statshouse connection event: close:%d error:%d", connection->close, connection->error);

        server = connection->data;

        if (server) {
            ngx_statshouse_disconnect(server);
            server = NULL;
        }
    }
}


static ngx_int_t
ngx_statshouse_connect(ngx_statshouse_server_t *server)
{
    ngx_err_t      err = 0;
    ngx_int_t      rc;
    ngx_socket_t   s;
    ngx_event_t   *rev, *wev;

    if (server->connection) {
        return NGX_OK;
    }

    if (server->addr.naddrs == 0) {
        return NGX_ERROR;
    }

    s = ngx_socket(server->addr.addrs->sockaddr->sa_family, SOCK_DGRAM, 0);
    if (s == (ngx_socket_t) -1) {
        return NGX_ERROR;
    }

    server->connection = ngx_get_connection(s, server->log);
    if (server->connection == NULL) {
        close(s);

        return NGX_ERROR;
    }

    server->connection->idle = 1;
    server->connection->data = server;

    rev = server->connection->read;
    wev = server->connection->write;

    rev->log = server->log;
    wev->log = server->log;

    rev->handler = ngx_statshouse_read_handler;

    if (ngx_nonblocking(s) == -1) {
        err = ngx_socket_errno;
        goto failed;
    }

    rc = connect(s, server->addr.addrs->sockaddr, server->addr.addrs->socklen);
    if (rc == -1) {
        err = ngx_socket_errno;

        if (err != NGX_EINPROGRESS && err != NGX_EAGAIN) {
            goto failed;
        }
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, server->log, 0,
        "statshouse success connect: %V", &server->addr.addrs->name);

    return NGX_OK;

failed:
    ngx_log_error(NGX_LOG_ERR, server->log, err,
        "statshouse error connect: %V", &server->addr.addrs->name);

    ngx_statshouse_disconnect(server);
    return NGX_ERROR;
}


static void
ngx_statshouse_disconnect(ngx_statshouse_server_t *server)
{
    if (server->connection) {
        ngx_close_connection(server->connection);
        server->connection = NULL;
    }
}


static void
ngx_statshouse_timer_handler(ngx_event_t *ev)
{
    ngx_connection_t         *connection = ev->data;
    ngx_statshouse_server_t  *server = connection->data;

    ngx_statshouse_flush(server);
    ngx_statshouse_timer(server);
}


static void
ngx_statshouse_timer(ngx_statshouse_server_t *server)
{
    if (!server->flush || server->flush_event.timer_set) {
        return;
    }

    if (ngx_buf_size(server->buffer) == 0) {
        return;
    }

    if (ngx_terminate || ngx_exiting) {
        return;
    }

    ngx_add_timer(&server->flush_event, server->flush);
}


ngx_int_t
ngx_statshouse_flush(ngx_statshouse_server_t *server)
{
    ngx_int_t  retry, retries_max = 1;
    ngx_int_t  rc;
    ssize_t    n;

    if (ngx_buf_size(server->buffer) == 0) {
        return NGX_DECLINED;
    }

    for (retry = 0; retry < retries_max; retry++) {
        rc = ngx_statshouse_connect(server);

        if (rc != NGX_OK) {
            return rc;
        }

        n = ngx_send(server->connection, server->buffer->pos, ngx_buf_size(server->buffer));
        if (n == -1) {
            ngx_statshouse_disconnect(server);
            continue;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_CORE, server->log, 0,
            "statshouse send %z bytes: %V", n, &server->addr.addrs->name);

        if (ngx_terminate || ngx_exiting) {
            ngx_statshouse_disconnect(server);
        }

        server->buffer->last = server->buffer->pos;
        return NGX_OK;
    }

    return NGX_ERROR;
}


ngx_int_t
ngx_statshouse_flush_after_request(ngx_statshouse_server_t *server)
{
    if (server->flush_after_request) {
        return ngx_statshouse_flush(server);
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_statshouse_send_to_buffer(ngx_statshouse_server_t *server, ngx_statshouse_stat_t *stat)
{
    size_t  size;

    size = ngx_statshouse_tl_metrics_len(stat, 1);

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, server->log, 0,
        "statshouse build %z stat", size);

    if (ngx_statshouse_server_buffer_left(server) < size) {
        ngx_statshouse_flush(server);

        if (ngx_statshouse_server_buffer_left(server) < size) {
            ngx_log_error(NGX_LOG_WARN, server->log, 0,
                "statshouse error send stat: to big");

            return NGX_DECLINED;
        }
    }

    ngx_statshouse_tl_metrics(server->buffer, stat, 1);
    ngx_statshouse_timer(server);

    ngx_log_debug2(NGX_LOG_DEBUG_CORE, server->log, 0,
        "statshouse append new stat %z bytes, left %z",
        size, ngx_statshouse_server_buffer_left(server));

    return NGX_OK;
}


ngx_int_t
ngx_statshouse_send(ngx_statshouse_server_t *server, ngx_statshouse_stat_t *stat)
{
    ngx_int_t  rc;

    if (server->aggregate) {
        rc = ngx_statshouse_aggregate(server->aggregate, stat, ngx_current_msec);
        if (rc == NGX_ERROR || rc == NGX_OK) {
            return rc;
        }
    }

    return ngx_statshouse_send_to_buffer(server, stat);
}


static ngx_int_t
ngx_statshouse_stat_is_group_delimiter(u_char *p, u_char *end, u_char **next_ptr)
{
    if (p + 3 < end) {
        return 0;
    }

    if (p[0] != ' ' || p[1] != ':' || p[2] != ' ') {
        return 0;
    }

    if (*next_ptr) {
        *next_ptr = p + 3;
    }

    return 1;
}


static ngx_str_t *
ngx_statshouse_stat_part(ngx_str_t *postfix, ngx_str_t *item, u_char **next_ptr)
{
    u_char  *p, *end;

    end = postfix->data + postfix->len;

    if (item->data == NULL) {
        p = postfix->data;
    } else {
        p = *next_ptr;
    }

    while (p < end) {
        if (ngx_statshouse_stat_is_group_delimiter(p, end, &p)) {
            continue;
        }

        if (p[0] == ' ') {
            ++p;

            continue;
        }

        break;
    }

    if (p == end) {
        return NULL;
    }

    item->data = p;
    item->len = 0;

    while (p < end) {
        if (p[0] == ',') {
            break;
        }

        if (ngx_statshouse_stat_is_group_delimiter(p, end, NULL)) {
            break;
        }

        ++p;
    }

    item->len = p - item->data;

    if (p < end && p[0] == ',') {
        ++p;
    }

    if (item->len > 0 && item->data[item->len - 1] == ' ') {
        --item->len;
    }

    while (p < end) {
        if (ngx_statshouse_stat_is_group_delimiter(p, end, &p)) {
            continue;
        }

        if (p[0] == ' ') {
            ++p;

            continue;
        }

        break;
    }

    if (item->len == 0 && p == end) {
        return NULL;
    }

    if (item->len == 1 && item->data[0] == '-') {
        item->len = 0;
    }

    *next_ptr = p;

    return item;
}


static ngx_int_t
ngx_statshouse_is_empty(ngx_str_t *str)
{
    if (str->len == 0 || (str->len == 1 && (str->data[0] == '-' || str->data[0] == '0'))) {
        return 1;
    }

    return 0;
}


static ngx_int_t
ngx_statshouse_test_required_predicates(ngx_array_t *predicates,
    ngx_statshouse_complex_value_pt complex, void *complex_ctx)
{
    ngx_str_t                  val;
    ngx_uint_t                 i;
    u_char                    *cv;

    if (predicates == NULL) {
        return NGX_OK;
    }

    cv = predicates->elts;

    for (i = 0; i < predicates->nelts; i++) {
        if (complex(complex_ctx, &cv[i * predicates->size], &val) != NGX_OK) {
            return NGX_ERROR;
        }

        if (val.len == 0 || (val.len == 1 && val.data[0] == '0')) {
            return NGX_DECLINED;
        }
    }

    return NGX_OK;
}


ngx_int_t
ngx_statshouse_stat_compile(ngx_statshouse_conf_t *conf, ngx_statshouse_stat_t *stats, ngx_int_t max,
    ngx_statshouse_complex_value_pt complex, void *complex_ctx, ngx_log_t *log)
{
    ngx_statshouse_stat_t      *stat;
    ngx_str_t                   keys[NGX_STATSHOUSE_STAT_KEYS_MAX];
    ngx_int_t                   i, j, n, rc, previ, minus;
    u_char                     *next;
    time_t                      now;

    ngx_str_t  s, split;
    ngx_int_t  splits;

    if (conf->disable) {
        return NGX_DECLINED;
    }

    if (conf->timeout) {
        now = ngx_time();

        if (conf->last != 0 && conf->last + conf->timeout > now) {
            return NGX_DECLINED;
        }

        conf->last = now;
    }

    rc = ngx_statshouse_test_required_predicates(conf->condition.complex, complex, complex_ctx);
    if (rc != NGX_OK) {
        return rc;
    }

    if (complex(complex_ctx, conf->value.complex, &s) != NGX_OK) {
        return NGX_ERROR;
    }

    for (i = 0; i < NGX_STATSHOUSE_STAT_KEYS_MAX; i++) {
        ngx_str_null(&keys[i]);

        if (conf->keys[i].disable) {
            continue;
        }

        if (conf->keys[i].name.len == 0) {
            continue;
        }

        if (complex(complex_ctx, conf->keys[i].complex, &keys[i]) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    splits = 0;
    ngx_str_null(&split);

    do {
        if (conf->value.split) {
            if (ngx_statshouse_stat_part(&s, &split, &next) == NULL) {
                break;
            }
        } else {
            if (s.len == 0) {
                break;
            }

            split = s;
        }

        stat = &stats[splits];
        ngx_statshouse_stat_init(stat, conf->name, conf->value.type);

        if (split.len > 0) {
            switch (conf->value.type) {
                case ngx_statshouse_mt_counter:
                    n = ngx_atoi(split.data, split.len);
                    if (n == NGX_ERROR) {
                        ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0,
                            "statshouse error parse counter: <%V>", &split);
                        return NGX_ERROR;
                    }

                    ngx_statshouse_stat_value_counter(stat, n);
                    break;

                case ngx_statshouse_mt_value:
                    minus = 0;

                    if (split.len > 0 && split.data[0] == '-') {
                        split.data++;
                        split.len--;

                        minus = 1;
                    }

                    n = ngx_atofp(split.data, split.len, 8);
                    if (n == NGX_ERROR) {
                        ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0,
                            "statshouse error parse value: <%V>", &split);
                        return NGX_ERROR;
                    }

                    if (minus) {
                        ngx_statshouse_stat_value_nvalue(stat, ((double) n) / 100000000.0);
                    } else {
                        ngx_statshouse_stat_value_value(stat, ((double) n) / 100000000.0);
                    }

                    break;

                case ngx_statshouse_mt_unique:
                    n = ngx_atoi(split.data, split.len);
                    if (n == NGX_ERROR) {
                        ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0,
                            "statshouse error parse unique: <%V>", &split);
                        return NGX_ERROR;
                    }

                    ngx_statshouse_stat_value_unique(stat, n);
                    break;

                default:
                    return NGX_ERROR;
            }
        } else {
            ngx_statshouse_stat_value_zero(stat);
        }

        splits++;

    } while (conf->value.split && splits < max);

    if (splits == 0) {
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0,
            "statshouse empty splits: <%V>", &s);

        return NGX_DECLINED;
    }

    for (i = 0; i < NGX_STATSHOUSE_STAT_KEYS_MAX; i++) {
        if (ngx_statshouse_is_empty(&keys[i])) {
            continue;
        }

        if (conf->keys[i].split) {
            ngx_str_null(&split);
            j = 0;

            while (j < max && ngx_statshouse_stat_part(&keys[i], &split, &next)) {
                stat = &stats[j];

                if (j >= splits) {
                    // init new splits

                    ngx_statshouse_stat_init(stat, conf->name, conf->value.type);

                    if (conf->value.split) {
                        ngx_statshouse_stat_value_zero(stat);
                    } else {
                        ngx_statshouse_stat_value(stat, stats[0].values[0]);
                    }

                    for (previ = 0; previ < i; previ++) {
                        if (conf->keys[previ].name.len == 0) {
                            continue;
                        }

                        if (conf->keys[previ].split) {
                            continue;
                        }

                        if (ngx_statshouse_is_empty(&keys[previ])) {
                            continue;
                        }

                        ngx_statshouse_stat_key(stat, conf->keys[previ].name, keys[previ]);
                    }
                }

                ngx_statshouse_stat_key(stat, conf->keys[i].name, split);
                ++j;
            }

            if (j > splits) {
                splits = j;
            }
        } else {
            for (j = 0; j < splits; j++) {
                ngx_statshouse_stat_key(&stats[j], conf->keys[i].name, keys[i]);
            }
        }
    }

    ngx_log_debug2(NGX_LOG_DEBUG_CORE, log, 0,
            "statshouse return %d splits: <%V>", splits, &s);

    return splits;
}


static ngx_int_t
ngx_statshouse_aggregate_handler(ngx_statshouse_stat_t *stat, void *ctx)
{
    ngx_statshouse_server_t  *server = ctx;

    if (stat == NULL) {
        return ngx_statshouse_flush(server);
    }

    return ngx_statshouse_send_to_buffer(server, stat);
}
