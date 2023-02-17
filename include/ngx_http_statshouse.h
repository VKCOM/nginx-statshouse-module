/* Copyright 2022 V Kontakte LLC
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _NGX_HTTP_STATSHOUSE_H_INCLUDED_
#define _NGX_HTTP_STATSHOUSE_H_INCLUDED_

#include <ngx_core.h>
#include <ngx_http.h>

#include <ngx_statshouse_stat.h>


ngx_int_t  ngx_http_statshouse_send(ngx_http_request_t *request, ngx_str_t *phase);
ngx_int_t  ngx_http_statshouse_send_stat(ngx_http_request_t *request, ngx_statshouse_stat_t *stat);


#endif
