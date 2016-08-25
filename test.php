<?php
$serv = new Swoole\Server("127.0.0.1", 9501, SWOOLE_PROCESS);

$serv->on("workerStart", function($serv, $workerId) {
    var_dump($serv);
});

$serv->on("receive", function($serv, $fd, $reactorId, $data) {
    var_dump($serv, $fd, $reactorId, $data);
    $serv->send($fd, "Swoole: $data");
});

$serv->on("connect", function($serv, $fd, $reactorId) {
    echo "Client#$fd connect\n";
    var_dump($serv->getClientInfo($fd));
});

$serv->on("close", function($serv, $fd, $reactorId) {
    echo "Client#$fd close\n";
});

$serv->set(array("hello"));

$serv->start();