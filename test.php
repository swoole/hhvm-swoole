<?php
$serv = new swoole_server("127.0.0.1", 9501, 4);

$serv->on("receive", function($fd, $reactorId) {
    echo "hello world\n";
});

//var_dump($serv);exit;

$serv->set(array("hello"));

$serv->start();