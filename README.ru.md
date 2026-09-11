
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

**syntax:** *statshouse_server* *server-addr* [name=*name*] [buffer=*size*] [aggregate=*size*] [aggregate_values=*num*] [splits_max=*num*] [flush_after_request] | *off*

**default:** no

**context:** *http*, *server*, *location*

Адрес сервера куда отправлять статистику. Директива может быть указана
несколько раз на одном уровне; статистика отправляется на все
сконфигурированные серверы (fan-out). Серверы, определённые на внешнем
уровне (*http*, *server*), наследуются внутренними уровнями и
объединяются с серверами, определёнными на них.

* `name` - необязательное имя сервера, по которому на него можно
  ссылаться из блока `statshouse_metric` с помощью параметра `server`.
  Имена серверов должны быть уникальными; дубликат имени - ошибка
  конфигурации.
* `buffer` - Размер udp пакета (если не включен flush_after_request).
* `aggregate` - размер буфера агрегации (0 - без агрегации).
* `aggregate_values` - максимальное число агрегируемых значений (по умолчанию 24).
* `splits_max` - максимальное число стат, компилируемых за раз (по умолчанию 16).
* `flush_after_request` - Отправлять статистику после каждого запроса.

Одинаковые `statshouse_server` (совпадает адрес, имя и параметры),
указанные на разных уровнях, ссылаются на один и тот же сервер.


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
> **server** - одно или несколько имён `statshouse_server`, на которые
> отправляется эта стата; если не указано, стата отправляется на все
> серверы данной локации. Неизвестное имя сервера - ошибка конфигурации.

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

Пример с несколькими серверами:

    statshouse_server 127.0.0.1:13301 name=local buffer=4k;
    statshouse_server 10.0.0.1:13301 name=remote;

    # отправляется на все серверы локации (local и remote)
    statshouse_metric nginx_request_time {
        value $request_time;
    }

    # отправляется только на сервер с именем "remote"
    statshouse_metric nginx_request_time_remote {
        value $request_time;

        server remote;
    }

    # отправляется только на серверы с именами "local" и "remote"
    statshouse_metric nginx_request_time_both {
        value $request_time;

        server local remote;
    }
