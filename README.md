
Module name
====

ngx_http_statshouse_module - Module for sending stats into `statshouse`.


Description
===========

Module packs stats data in `statshouse` format and sends to the specified address.


Directives
==========

* [statshouse_server](#statshouse_server)
* [statshouse_metric](#statshouse_metric)


statshouse_server
-------------------

**syntax:** *statshouse_server* *server-addr* [name=*name*] [buffer=*size*] [aggregate=*size*] [aggregate_values=*num*] [splits_max=*num*] [flush_after_request] | *off*

**default:** no

**context:** *http*, *server*, *location*

Server address to send stats. The directive may be specified several times
on the same level; stats are sent to all configured servers (fan-out).
Servers defined on an outer level (*http*, *server*) are inherited by inner
levels and merged with the servers defined there.

* `name` - optional server name, used to address this server from a
  `statshouse_metric` block via the `server` parameter. Server names must
  be unique; a duplicate name is a configuration error.
* `buffer` - udp packet size (if `flush_after_request` is not turned on).
* `aggregate` - size of the aggregation buffer (0 - no aggregation).
* `aggregate_values` - max number of aggregated values (default 24).
* `splits_max` - max number of stats compiled at once (default 16).
* `flush_after_request` - Send stats after every request.

An identical `statshouse_server` (same address, name and parameters)
specified on different levels refers to the same server.


statshouse_metric
-------------------

**syntax:** *statshouse_metric* *name* { ... }

**default:** no

**context:** *http*, *server*, *location*

Parameters of a single stat sent to `statshouse`.
* **name** - stat name in `statshouse`

Also following special parameters are supported:

> **count**, **value**, **unique** - sends matching value to `statshouse`
> **key0**, **key1** ... **key15** - sends matching key to `statshouse`
> **skey** - sends string top value to 'statshouse'
> **condition** - If condition is set and value is empty or "0", then stat would not be sent
> **timeout** - timeout of how often stat could be sent (within a worker)
> **server** - one or more names of `statshouse_server` servers this stat
> is sent to; if not set, the stat is sent to all servers of the given
> location. An unknown server name is a configuration error.

Only one of these parameters allowed in a single stat: *count*, *value*, *unique*

Examples:
==========

    statshouse_metric nginx_request_time {
        value $request_time;

        key1 $hostname;
        key2 $server_name;

        key3 $request_method;
        key4 $server_protocol;

        key5 $status;
    }

    statshouse_metric nginx_ssl_handshake_time {
        value $ssl_handshake_time;
        condition $ssl_protocol;

        key1 $ssl_protocol;
        key2 $ssl_cipher;
        key3 $ssl_early_data;
        key4 $ssl_session_reused;
    }

    statshouse_metric nginx_request_referer {
        count 1;
        condition $map_referer_domain;

        key1 $server_name;
        key2 $request_method;
        key3 $status;

        skey $map_referer_domain;
    }

Multiple servers example:

    statshouse_server 127.0.0.1:13301 name=local buffer=4k;
    statshouse_server 10.0.0.1:13301 name=remote;

    # sent to all servers of the location (local and remote)
    statshouse_metric nginx_request_time {
        value $request_time;
    }

    # sent only to the server named "remote"
    statshouse_metric nginx_request_time_remote {
        value $request_time;

        server remote;
    }

    # sent only to the servers named "local" and "remote"
    statshouse_metric nginx_request_time_both {
        value $request_time;

        server local remote;
    }
