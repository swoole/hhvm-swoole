<?php
$serv = new Swoole\Server("127.0.0.1", 9502, SWOOLE_PROCESS, SWOOLE_SOCK_UDP);

$serv->on("Packet", function($serv, $data, $clientInfo) {
    var_dump($data, $clientInfo);
    $serv->sendto($clientInfo['address'], $clientInfo['port'], "Swoole: $data");
});

$serv->start();