
<?php
/**
 * hhvm -vDynamicExtensions.0=./hhvm_swoole.so test.php
 */

$serv = new Swoole\Server("127.0.0.1", 9501, SWOOLE_BASE);

$serv->on("workerStart", function($serv, $workerId) {
    //var_dump($serv);
    $serv->tick(1000, function() {
        echo "hello\n";
    });
});

$serv->on("receive", function($serv, $fd, $reactorId, $data) {
    //var_dump($serv, $fd, $reactorId, $data);
    $serv->send($fd, "Swoole: $data");
    $serv->task("hello world", -1, function($serv, $task_id, $result) {
        echo "task#{$task_id} is finished\n";
        var_dump($result);
    });
});

$serv->on("connect", function($serv, $fd, $reactorId) {
    echo "Client#$fd connect\n";
});

$serv->on("close", function($serv, $fd, $reactorId) {
    echo "Client#$fd close\n";
});

$serv->on("task", function($serv, $task_id, $from_id, $data) {
    var_dump($task_id, $from_id, $data);
    return array("tt" => time(), "data" => "hhvm");
});

$serv->on("finish", function($serv, $task_id, $result) {
    var_dump($task_id, $result);
});

$serv->set(array("worker_num" => 1));

$serv->start();
