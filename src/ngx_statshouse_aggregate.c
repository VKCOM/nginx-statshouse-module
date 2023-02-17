/* Copyright 2022 V Kontakte LLC
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <ngx_core.h>
#include <ngx_statshouse_stat.h>

#include "ngx_statshouse_aggregate.h"


typedef struct {
    ngx_rbtree_node_t                    node;
    size_t                               size;

    ngx_msec_t                           time;
    ngx_queue_t                          queue;

    ngx_statshouse_stat_t                stat;
} ngx_statshouse_aggregate_stat_t;


static void  ngx_statshouse_aggregate_timer_handler(ngx_event_t *ev);
static void  ngx_statshouse_aggregate_timer(ngx_statshouse_aggregate_t *aggregate, ngx_msec_t now);

static void  ngx_statshouse_aggregate_insert_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
static ngx_statshouse_aggregate_stat_t  *ngx_statshouse_aggregate_lookup(ngx_statshouse_aggregate_t *server,
    uint32_t hash, size_t size, ngx_int_t type);
static void  *ngx_statshouse_aggregate_alloc(ngx_statshouse_aggregate_t *aggregate, size_t size);
static void  ngx_statshouse_aggregate_free(ngx_statshouse_aggregate_t *aggregate, void *ptr, size_t size);


ngx_int_t
ngx_statshouse_aggregate_init(ngx_statshouse_aggregate_t *aggregate, ngx_pool_t *pool)
{
    aggregate->alloc.start = ngx_palloc(pool, aggregate->size);
    if (aggregate->alloc.start == NULL) {
        return NGX_ERROR;
    }

    aggregate->alloc.end = aggregate->alloc.start + aggregate->size;
    aggregate->alloc.pos = aggregate->alloc.start;
    aggregate->alloc.last = aggregate->alloc.start;

    ngx_rbtree_init(&aggregate->rbtree, &aggregate->sentinel, ngx_statshouse_aggregate_insert_value);
    ngx_queue_init(&aggregate->queue);

    aggregate->timer_event.handler = ngx_statshouse_aggregate_timer_handler;
    aggregate->timer_event.log = ngx_cycle->log;
    aggregate->timer_event.data = &aggregate->timer_connection;
    aggregate->timer_event.cancelable = 1;

    aggregate->timer_connection.fd = -1;
    aggregate->timer_connection.data = aggregate;

    return NGX_OK;
}


ngx_int_t
ngx_statshouse_aggregate(ngx_statshouse_aggregate_t *aggregate, ngx_statshouse_stat_t *stat, ngx_msec_t now)
{
    ngx_statshouse_aggregate_stat_t  *astat;
    ngx_int_t                         i, rc;
    uint32_t                          hash;
    size_t                            size;
    u_char                           *p;

    if (stat->type != ngx_statshouse_mt_counter && aggregate->values == 0) {
        return NGX_DECLINED;
    }

    if (ngx_terminate || ngx_exiting) {
        ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
            "statshouse decline aggregate, worker exiting");

        return NGX_DECLINED;
    }

    ngx_crc32_init(hash);
    size = sizeof(ngx_statshouse_aggregate_stat_t);

    ngx_crc32_update(&hash, stat->name.data, stat->name.len);

    for (i = 0; i < stat->keys_count; i++) {
        ngx_crc32_update(&hash, stat->keys[i].name.data, stat->keys[i].name.len);
        ngx_crc32_update(&hash, stat->keys[i].value.data, stat->keys[i].value.len);

        size += stat->keys[i].value.len;
    }

    ngx_crc32_final(hash);

    if (stat->type != ngx_statshouse_mt_counter) {
        size += sizeof(ngx_statshouse_stat_value_t) * (aggregate->values - 1);
    }

    astat = ngx_statshouse_aggregate_lookup(aggregate, hash, size, stat->type);
    if (astat != NULL) {
        if (stat->type == ngx_statshouse_mt_counter) {
            astat->stat.values[0].counter += stat->values[0].counter;

            ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                "statshouse success aggregate counter, found exists node (%v)", &stat->name);

            return NGX_OK;
        }

        if (astat->stat.values_count < aggregate->values) {
            astat->stat.values[astat->stat.values_count] = stat->values[0];
            astat->stat.values_count++;

            ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                "statshouse success aggregate value, found exists node (%v)", &stat->name);

            return NGX_OK;
        }

        ngx_rbtree_delete(&aggregate->rbtree, &astat->node);

        astat->node.key = 0;
        astat = NULL;
    }

    astat = ngx_statshouse_aggregate_alloc(aggregate, size);
    if (astat == NULL) {
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
            "statshouse aggregate, error allocate %uz size", size);

        rc = ngx_statshouse_aggregate_process(aggregate, now);
        if (rc != NGX_OK) {
            return rc;
        }
        

        astat = ngx_statshouse_aggregate_alloc(aggregate, size);
        if (astat == NULL) {
            ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                "statshouse error aggregate, double error allocate %uz size", size);

            return NGX_DECLINED;
        }
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
        "statshouse aggregate, success allocate %uz size", size);

    astat->node.key = hash;
    astat->size = size;
    astat->time = now;

    astat->stat.name = stat->name;
    astat->stat.values_count = 1;
    astat->stat.values[0] = stat->values[0];
    astat->stat.type = stat->type;
    astat->stat.keys_count = stat->keys_count;

    p = (u_char *) astat + sizeof(ngx_statshouse_aggregate_stat_t);
    if (stat->type != ngx_statshouse_mt_counter) {
        p += sizeof(ngx_statshouse_stat_value_t) * (aggregate->values - 1);
    }

    for (i = 0; i < stat->keys_count; i++) {
        astat->stat.keys[i].name = stat->keys[i].name;

        astat->stat.keys[i].value.data = p;
        astat->stat.keys[i].value.len = stat->keys[i].value.len;

        ngx_memcpy(p, stat->keys[i].value.data, stat->keys[i].value.len);
        p += stat->keys[i].value.len;
    }

    ngx_rbtree_insert(&aggregate->rbtree, &astat->node);
    ngx_queue_insert_tail(&aggregate->queue, &astat->queue);

    ngx_statshouse_aggregate_timer(aggregate, now);

    return NGX_OK;
}


ngx_int_t
ngx_statshouse_aggregate_process(ngx_statshouse_aggregate_t *aggregate, ngx_msec_t now)
{
    ngx_statshouse_aggregate_stat_t  *astat;
    ngx_queue_t                      *queue;
    ngx_msec_t                        diff;
    ngx_int_t                         rc, count = 0, flush = 0;

    if (ngx_queue_empty(&aggregate->queue)) {
        ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
            "statshouse aggregate, empty queue");

        return NGX_DECLINED;
    }

    if (ngx_terminate || ngx_exiting) {
        ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
            "statshouse aggregate, flush all queue");

        flush = 1;
    }

    do {
        queue = ngx_queue_head(&aggregate->queue);
        astat = ngx_queue_data(queue, ngx_statshouse_aggregate_stat_t, queue);

        diff = now - astat->time;
        if (!flush && diff < aggregate->interval) {
            break;
        }

        rc = aggregate->handler(&astat->stat, aggregate->ctx);
        if (rc != NGX_OK) {
            return rc;
        }

        ++count;

        ngx_queue_remove(queue);

        if (astat->node.key) {
            ngx_rbtree_delete(&aggregate->rbtree, &astat->node);
        }

        ngx_statshouse_aggregate_free(aggregate, astat, astat->size);

    } while (!ngx_queue_empty(&aggregate->queue));

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
        "statshouse aggregate, flush %d stats", count);

    if (count == 0) {
        return NGX_DECLINED;
    }

    return aggregate->handler(NULL, aggregate->ctx);
}


static void
ngx_statshouse_aggregate_timer_handler(ngx_event_t *ev)
{
    ngx_statshouse_aggregate_t  *aggregate;
    ngx_connection_t            *connection = ev->data;
    ngx_msec_t                   now = ngx_current_msec;

    aggregate = connection->data;

    ngx_statshouse_aggregate_process(aggregate, now);
    ngx_statshouse_aggregate_timer(aggregate, now);
}


static void
ngx_statshouse_aggregate_timer(ngx_statshouse_aggregate_t *aggregate, ngx_msec_t now)
{
    ngx_statshouse_aggregate_stat_t  *astat;
    ngx_queue_t                      *queue;
    ngx_msec_t                        diff;

    if (aggregate->timer_event.timer_set || aggregate->timer_event.posted) {
        return;
    }

    if (ngx_queue_empty(&aggregate->queue)) {
        return;
    }

    queue = ngx_queue_head(&aggregate->queue);
    astat = ngx_queue_data(queue, ngx_statshouse_aggregate_stat_t, queue);

    diff = now - astat->time;
    if (diff >= aggregate->interval) {
        ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
            "statshouse aggregate, set posted");

        ngx_post_event(&aggregate->timer_event, &ngx_posted_events);
        return;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
        "statshouse aggregate, flush timer %M", aggregate->interval - diff);

    ngx_add_timer(&aggregate->timer_event, aggregate->interval - diff);
}


static void
ngx_statshouse_aggregate_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t                **p;
    ngx_statshouse_aggregate_stat_t   *sn, *sn_temp;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            sn = ngx_rbtree_data(node, ngx_statshouse_aggregate_stat_t, node);
            sn_temp = ngx_rbtree_data(temp, ngx_statshouse_aggregate_stat_t, node);

            p = (sn->size < sn_temp->size) ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


static ngx_statshouse_aggregate_stat_t *
ngx_statshouse_aggregate_lookup(ngx_statshouse_aggregate_t *aggregate, uint32_t hash, size_t size, ngx_int_t type)
{
    ngx_rbtree_node_t                *node, *sentinel;
    ngx_statshouse_aggregate_stat_t  *astat;

    node = aggregate->rbtree.root;
    sentinel = aggregate->rbtree.sentinel;

    while (node != sentinel) {

        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        /* hash == node->key */

        astat = ngx_rbtree_data(node, ngx_statshouse_aggregate_stat_t, node);

        if (size < astat->size) {
            node = node->left;
            continue;
        }

        if (size > astat->size) {
            node = node->right;
            continue;
        }

        if (astat->stat.type == type) {
            return astat;
        }

        node = (astat->stat.type < type) ? node->left : node->right;
    }

    /* not found */

    return NULL;
}


static void *
ngx_statshouse_aggregate_alloc(ngx_statshouse_aggregate_t *aggregate, size_t size)
{
    u_char  *ptr = NULL;

    if (aggregate->alloc.pos >= aggregate->alloc.last)  {
        if ((aggregate->alloc.end - aggregate->alloc.pos) > (ptrdiff_t) size)  {
            ptr = aggregate->alloc.pos;
            aggregate->alloc.pos += size;
        } else if ((aggregate->alloc.last - aggregate->alloc.start) > (ptrdiff_t) size) {
            ptr = aggregate->alloc.start;
            aggregate->alloc.pos = (aggregate->alloc.start + size);
        }
    } else {
        if ((aggregate->alloc.last - aggregate->alloc.pos) > (ptrdiff_t) size)  {
            ptr = aggregate->alloc.pos;
            aggregate->alloc.pos += size;
        }
    }

    if (ptr == NULL) {
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
            "statshouse aggregate, error allocate %uz bytes", size);

        return NULL;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
        "statshouse aggregate, success allocate %uz bytes", size);

    return ptr;
}


static void
ngx_statshouse_aggregate_free(ngx_statshouse_aggregate_t *aggregate, void *ptr, size_t size)
{
    ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
        "statshouse aggregate, free %uz bytes", size);

    if (ptr == aggregate->alloc.start) {
        aggregate->alloc.last = aggregate->alloc.start;
    }

    aggregate->alloc.last += size;
}
