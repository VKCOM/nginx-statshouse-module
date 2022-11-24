
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

**syntax:** *statshouse_server* *server-addr* [buffer=*size*] [flush_after_request] | *off*

**default:** no

**context:** *http*, *server*, *location*

Server address to send stats.

* `buffer` - udp packet size (if `flush_after_request` is not turned on).
* `flush_after_request` - Send stats after every request.


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
