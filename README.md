hhvm-swoole
=================
Swoole on HHVM.

Build libswoole.so
====
```shell
git clone https://github.com/swoole/swoole-src.git
cd swoole-src
phpize
./configure
cmake .
make swoole_shared
cp lib/libswoole.so.1.8.11 /usr/local/lib/libswoole.so
sudo ldconfig
```

Build hhvm_swoole.so
====
```shell
git clone https://github.com/swoole/hhvm-swoole.git
cd hhvm-swoole
ln -s ../swoole-src/ swoole
cp swoole/config.h config.h
hphpize
cmake .
make
```

Run
====
```shell
hhvm -vDynamicExtensions.0=./hhvm_swoole.so tcp_server.php
hhvm -vDynamicExtensions.0=./hhvm_swoole.so udp_server.php
```

Supported features
====
* Swoole\Server->__construct
* Swoole\Server->on (onWorkerStart/onWorkerStop/onConnect/onClose/onReceive/onPacket/onTask/onFinish)
* Swoole\Server->set
* Swoole\Server->start
* Swoole\Server->task
* Swoole\Server->send
* Swoole\Server->sendto
* Swoole\Server->sendfile
* Swoole\Server->close
* Swoole\Server->exist
* Swoole\Server->getClientInfo


