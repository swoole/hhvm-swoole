<?php
$serv = new swoole_server("127.0.0.1", 9501);

$serv->on("receive", function($serv, $fd, $reactorId) {
    echo "hello world\n";
});

$serv->set(array("hello"));

$serv->start();