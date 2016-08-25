hhvm-swoole
=================
Swoole on HHVM.

Build
====
Build swoole library
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

```shell
git clone https://github.com/swoole/hhvm-swoole.git
cd hhvm-swoole
ln -s ../swoole-src/ swoole
cp swoole/config.h config.h
hphpize
cmake .
make
./test.sh
```

