
Название
====

ngx_http_statshouse_module - Модуль для отправки статы в statshouse.


Описание
===========

Модуль упаковывает стату в формат stashouse и отправляет на указзанный удрес.


Директивы
==========

* [statshouse_server](#statshouse_server)
* [statshouse_metric](#statshouse_metric)


statshouse_server
-------------------

**syntax:** *statshouse_server* *server-addr* [buffer=*size*] [flush_after_request] | *off*

**default:** no

**context:** *http*, *server*, *location*

Адрес сервера куда отправлять статистику.

* buffer - Размер udp пакета (если не включен flush_after_request).
* flush_after_request - Отправлять статистику после каждого запроса.


statshouse_metric
-------------------

**syntax:** *statshouse_metric* *name* { ... }

**default:** no

**context:** *http*, *server*, *location*

Параметры одной статы отправляемой в statshouse.
* **name** - Имя статистики в statshouse

Также поддерживаются следующие специальные параметры:

> **count**, **value**, **unique** - Отправляет соответвующее значение в statshouse
> **key0**, **key1** ... **key15** - Отпарвляет соответвющий ключ в statshouse
> **skey** - Отправляет string top значение в statshouse
> **condition** - Если выставленно, то стата не отправится если значение будет пустое или "0"
> **timeout** - Таймаут с какой частотой можно отправлять стату (в рамках одного воркера)

В одной стате возможно только один из параметров: *count*, *value*, *unique*

Примеры:
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
