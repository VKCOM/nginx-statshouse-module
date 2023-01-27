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


ngx_int_t  ngx_stream_statshouse_send(ngx_stream_session_t *session, ngx_str_t *phase);


#endif
