/* Copyright 2022 V Kontakte LLC
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_config.h>

#include "ngx_statshouse.h"
#include "ngx_statshouse_stat.h"


#define NGX_HTTP_STATSHOUSE_CONF                0x100000000
#define NGX_HTTP_STATSHOUSE_CONF_OFFSET         offsetof(ngx_http_statshouse_conf_ctx_t, statshouse_conf)


typedef struct {
    ngx_http_conf_ctx_t                         http_conf;
    void                                      **statshouse_conf;
} ngx_http_statshouse_conf_ctx_t;

typedef struct {
    ngx_array_t                                *confs;
    ngx_statshouse_server_t                    *server;

    ngx_flag_t                                  enable;
} ngx_http_statshouse_loc_conf_t;

typedef struct {
    ngx_array_t                                *servers;
} ngx_http_statshouse_main_conf_t;


static ngx_int_t   ngx_http_statshouse_init(ngx_conf_t *cf);
static void *      ngx_http_statshouse_create_main_conf(ngx_conf_t *cf);
static void *      ngx_http_statshouse_create_loc_conf(ngx_conf_t *cf);
static char *      ngx_http_statshouse_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static char *      ngx_http_statshouse_metric_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *      ngx_http_statshouse_value_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *      ngx_http_statshouse_key_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *      ngx_http_statshouse_server_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_int_t   ngx_http_statshouse_handler(ngx_http_request_t *request);


static ngx_command_t  ngx_http_statshouse_commands[] = {
    { ngx_string("statshouse_server"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_server_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },

    { ngx_string("statshouse_metric"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_BLOCK
            |NGX_CONF_TAKE1,
        ngx_http_statshouse_metric_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },

    { ngx_string("condition"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_set_predicate_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, condition),
        NULL
    },

    { ngx_string("timeout"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_conf_set_sec_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, timeout),
        NULL
    },

    { ngx_string("count"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_value_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, value),
        (void *) ngx_statshouse_mt_counter
    },

    { ngx_string("value"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_value_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, value),
        (void *) ngx_statshouse_mt_value
    },

    { ngx_string("unique"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_value_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, value),
        (void *) ngx_statshouse_mt_unique
    },

    { ngx_string("key0"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[0]),
        0
    },

    { ngx_string("key1"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[1]),
        0
    },

    { ngx_string("key2"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[2]),
        0
    },

    { ngx_string("key3"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[3]),
        0
    },

    { ngx_string("key4"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[4]),
        0
    },

    { ngx_string("key5"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[5]),
        0
    },

    { ngx_string("key6"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[6]),
        0
    },

    { ngx_string("key7"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[7]),
        0
    },

    { ngx_string("key8"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[8]),
        0
    },

    { ngx_string("key9"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[9]),
        0
    },

    { ngx_string("key10"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[10]),
        0
    },

    { ngx_string("key11"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[11]),
        0
    },

    { ngx_string("key12"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[12]),
        0
    },

    { ngx_string("key13"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[13]),
        0
    },

    { ngx_string("key14"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[14]),
        0
    },

    { ngx_string("key15"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[15]),
        0
    },

    { ngx_string("skey"),
        NGX_HTTP_STATSHOUSE_CONF
            |NGX_CONF_1MORE,
        ngx_http_statshouse_key_slot,
        NGX_HTTP_STATSHOUSE_CONF_OFFSET,
        offsetof(ngx_statshouse_conf_t, keys[16]),
        0
    },

    ngx_null_command
};

static ngx_http_module_t  ngx_http_statshouse_module_ctx = {
    NULL,                                    /* preconfiguration */
    ngx_http_statshouse_init,                /* postconfiguration */

    ngx_http_statshouse_create_main_conf,    /* create main configuration */
    NULL,                                    /* init main configuration */

    NULL,                                    /* create server configuration */
    NULL,                                    /* merge server configuration */

    ngx_http_statshouse_create_loc_conf,     /* create location configuration */
    ngx_http_statshouse_merge_loc_conf       /* merge location configuration */
};

ngx_module_t  ngx_http_statshouse_module = {
    NGX_MODULE_V1,
    &ngx_http_statshouse_module_ctx,         /* module context */
    ngx_http_statshouse_commands,            /* module directives */
    NGX_HTTP_MODULE,                         /* module type */
    NULL,                                    /* init master */
    NULL,                                    /* init module */
    NULL,                                    /* init process */
    NULL,                                    /* init thread */
    NULL,                                    /* exit thread */
    NULL,                                    /* exit process */
    NULL,                                    /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_http_variable_t  ngx_http_statshouse_variables[] = {
      ngx_http_null_variable
};


static ngx_int_t
ngx_http_statshouse_init(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t  *cmcf;
    ngx_http_variable_t        *cv, *v;
    ngx_http_handler_pt        *h;

    for (cv = ngx_http_statshouse_variables; cv->name.len; cv++) {
        v = ngx_http_add_variable(cf, &cv->name, cv->flags);
        if (v == NULL) {
            return NGX_ERROR;
        }

        *v = *cv;
    }

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_statshouse_handler;

    return NGX_OK;
}


static void *
ngx_http_statshouse_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_statshouse_main_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_statshouse_main_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    return conf;
}


static void *
ngx_http_statshouse_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_statshouse_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_statshouse_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    /*
     * set by ngx_pcalloc():
     * 
     * conf->confs
     */

    conf->enable = NGX_CONF_UNSET;
    conf->server = NGX_CONF_UNSET_PTR;

    return conf;
}


static char *
ngx_http_statshouse_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_statshouse_loc_conf_t  *prev = parent;
    ngx_http_statshouse_loc_conf_t  *conf = child;
    ngx_statshouse_conf_t           *sconfs, *sconf;
    ngx_uint_t                       i;

    if (prev->confs) {
        if (conf->confs == NULL) {
            conf->confs = prev->confs;
        } else {
            sconfs = prev->confs->elts;

            for (i = 0; i < prev->confs->nelts; i++) {
                sconf = ngx_array_push(conf->confs);
                if (sconf == NULL) {
                    return NGX_CONF_ERROR;
                }

                *sconf = sconfs[i];
            }
        }
    }

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_ptr_value(conf->server, prev->server, NULL);

    return NGX_CONF_OK;
}


static char *
ngx_http_statshouse_metric_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_statshouse_loc_conf_t  *shlc = conf;

    ngx_http_statshouse_conf_ctx_t  *ctx;
    ngx_statshouse_conf_t           *shc;
    ngx_str_t                       *value;
    ngx_conf_t                       save;
    char                            *rv;

    if (cf->module_type != NGX_HTTP_MODULE) {
        return NGX_CONF_ERROR;
    }

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_statshouse_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memcpy(&ctx->http_conf, cf->ctx, sizeof(ngx_http_conf_ctx_t));

    ctx->statshouse_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->statshouse_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    if (shlc->confs == NULL) {
        shlc->confs = ngx_pcalloc(cf->pool, sizeof(ngx_array_t));
        if (shlc->confs == NULL) {
            return NGX_CONF_ERROR;
        }

        if (ngx_array_init(shlc->confs, cf->pool, 1, sizeof(ngx_statshouse_conf_t)) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    shc = ngx_array_push(shlc->confs);
    if (shc == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(shc, sizeof(ngx_statshouse_conf_t));

    value = cf->args->elts;
    shc->name = value[1];

    shc->condition = NGX_CONF_UNSET_PTR;
    shc->timeout = NGX_CONF_UNSET;

    ctx->statshouse_conf[ngx_http_statshouse_module.ctx_index] = shc;

    save = *cf;

    cf->ctx = ctx;
    cf->cmd_type = NGX_HTTP_STATSHOUSE_CONF;

    rv = ngx_conf_parse(cf, NULL);
    *cf = save;

    if (rv != NGX_CONF_OK) {
        return rv;
    }

    if (shc->value.type == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "missed count/unique/value param in statshouse_metric");
        return NGX_CONF_ERROR;
    }

    ngx_conf_init_ptr_value(shc->condition, NULL);
    ngx_conf_init_value(shc->timeout, 0);

    return NGX_CONF_OK;
}


static char *
ngx_http_statshouse_value_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_statshouse_conf_value_t       *field;
    ngx_str_t                         *value;
    ngx_http_compile_complex_value_t   ccv;
    ngx_uint_t                         i;

    field = (ngx_statshouse_conf_value_t *) (p + cmd->offset);

    if (field->type) {
        return "is duplicate";
    }

    value = cf->args->elts;

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    field->complex = ngx_pcalloc(cf->pool, sizeof(ngx_http_complex_value_t));
    if (field->complex == NULL) {
        return NGX_CONF_ERROR;
    }

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = field->complex;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    for (i = 2; i < cf->args->nelts; i++) {
        if (ngx_strncmp(value[i].data, "split", 5) == 0) {
            field->split = 1;
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid property \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    field->type = (ngx_statshouse_stat_type_e) cmd->post;

    return NGX_CONF_OK;
}


static char *
ngx_http_statshouse_key_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_statshouse_conf_key_t         *key;
    ngx_str_t                         *value;
    ngx_http_compile_complex_value_t   ccv;
    ngx_uint_t                         i;

    key = (ngx_statshouse_conf_key_t *) (p + cmd->offset);

    if (key->name.len) {
        return "is duplicate";
    }

    value = cf->args->elts;
    key->name = value[0];

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    key->complex = ngx_pcalloc(cf->pool, sizeof(ngx_http_complex_value_t));
    if (key->complex == NULL) {
        return NGX_CONF_ERROR;
    }

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = key->complex;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    for (i = 2; i < cf->args->nelts; i++) {
        if (ngx_strncmp(value[i].data, "split", 5) == 0) {
            key->split = 1;
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid property \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_statshouse_server_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_statshouse_loc_conf_t   *slcf = conf;
    ngx_http_statshouse_main_conf_t  *smcf;
    ngx_statshouse_server_t          *servers, *server = NULL;
    ngx_url_t                         url;
    ngx_str_t                        *value, s;
    ngx_flag_t                        flush_after_request;
    ngx_int_t                         splits_max;
    ngx_uint_t                        i;
    ssize_t                           buffer_size;
    ngx_msec_t                        flush;

    value = cf->args->elts;

    if (slcf->server != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    if (cf->args->nelts == 2) {
        if (ngx_strncmp(value[1].data, "off", 3) == 0) {
            slcf->enable = 0;

            return NGX_CONF_OK;
        }
    }

    buffer_size = 4 * 1024;
    flush_after_request = 0;
    splits_max = 16;
    flush = 1000;

    ngx_memzero(&url, sizeof(ngx_url_t));
    url.url = value[1];
    url.no_resolve = 0;

    if (ngx_parse_url(cf->pool, &url) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "\"%V\" error parse address", &cmd->name);

        return NGX_CONF_ERROR;
    }

    for (i = 2; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "buffer=", 7) == 0) {

            s.data =  value[i].data + 7;
            s.len = value[i].data + value[i].len - s.data;

            buffer_size = ngx_parse_size(&s);

            if (buffer_size == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid buffer size \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "flush=", 6) == 0) {

            s.data =  value[i].data + 6;
            s.len = value[i].data + value[i].len - s.data;

            flush = ngx_parse_time(&s, 0);

            if (flush == (ngx_msec_t) NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid flush time \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "splits_max=", 6) == 0) {

            s.data =  value[i].data + 11;
            s.len = value[i].data + value[i].len - s.data;

            splits_max = ngx_atoi(s.data, s.len);

            if (splits_max == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid splits_max size \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "flush_after_request", 19) == 0) {

            flush_after_request = 1;
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    smcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_statshouse_module);

    if (smcf->servers == NULL) {
        smcf->servers = ngx_array_create(cf->pool, 1, sizeof(ngx_statshouse_server_t));

        if (smcf->servers == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    servers = smcf->servers->elts;

    for (i = 0; i < smcf->servers->nelts; i++) {
        if (servers[i].addr.addrs->socklen == url.addrs->socklen &&
            ngx_memcmp(servers[i].addr.addrs->sockaddr, url.addrs->sockaddr, url.addrs->socklen) == 0 &&
            servers[i].flush_after_request == flush_after_request &&
            servers[i].splits_max == splits_max &&
            servers[i].buffer_size == buffer_size)
        {
            server = &servers[i];
            break;
        }
    }

    if (server) {
        // found same server
        slcf->server = server;
        slcf->enable = 1;

        return NGX_OK;
    }

    server = ngx_array_push(smcf->servers);
    if (server == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(server, sizeof(ngx_statshouse_server_t));

    server->addr = url;
    server->flush_after_request = flush_after_request;
    server->buffer_size = buffer_size;
    server->splits_max = splits_max;
    server->flush = flush;

    server->buffer = ngx_create_temp_buf(cf->pool, buffer_size);
    if (server->buffer == NULL) {
        return NGX_CONF_ERROR;
    }

    server->splits = ngx_pcalloc(cf->pool, sizeof(ngx_statshouse_stat_t) * server->splits_max);
    if (server->splits == NULL) {
        return NGX_CONF_ERROR;
    }

    slcf->server = server;
    slcf->enable = 1;

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_statshouse_handler(ngx_http_request_t *request)
{
    ngx_http_statshouse_loc_conf_t    *shlc;

    ngx_statshouse_conf_t             *confs;
    ngx_statshouse_stat_t             *stats;
    ngx_uint_t                         i;
    ngx_int_t                          j, n;

    shlc = ngx_http_get_module_loc_conf(request, ngx_http_statshouse_module);
    if (shlc->server == NULL || shlc->confs == NULL || shlc->enable == 0) {
        return NGX_OK;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, request->connection->log, 0,
        "statshouse handler");

    confs = shlc->confs->elts;
    stats = shlc->server->splits;

    for (i = 0; i < shlc->confs->nelts; i++) {
        n = ngx_statshouse_stat_compile(&confs[i], stats, shlc->server->splits_max,
            (ngx_statshouse_complex_value_pt) ngx_http_complex_value, request);
        if (n <= 0) {
            continue;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, request->connection->log, 0,
            "statshouse send %d stats", n);

        for (j = 0; j < n; j++) {
            ngx_statshouse_send(shlc->server, &stats[j], 0);
        }
    }

    ngx_statshouse_flush_after_request(shlc->server);
    return NGX_OK;
}
