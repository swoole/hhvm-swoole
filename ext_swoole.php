<?hh
class Swoole_Server {
  <<__Native>>
  function __construct(string $host, int $port, int $mode=3, int $type = 1);
  <<__Native>>
  function on(string $eventType, callable $callback) : bool;
  <<__Native>>
  function set(array $settings): bool;
  <<__Native>>
  function start(): bool;
  <<__Native>>
  function send(int $fd, string $data): bool;
  <<__Native>>
  function sendto(string $address, int $port, string $data, int $server_socket = -1): bool;
  <<__Native>>
  function close(int $fd, bool $reset = false): bool;
  <<__Native>>
  function sendfile(int $fd, string $file, int $offset = 0): bool;
  <<__Native>>
  function exist(int $fd): bool;
  <<__Native>>
  function getClientInfo(int $fd, int $reactorId = 0, bool $noCheckConnection = false): array;
  <<__Native>>
  function task(mixed $data, int $dst_worker_id = -1, callable $callback = null): bool;
}

namespace Swoole
{
  class Server extends \Swoole_server{

  }
}