<?hh
class Swoole_Server {
  <<__Native>>
  function __construct(string $host, int $port, int $mode=3, int $type = 3);
  <<__Native>>
  function on(string $eventType, mixed $callback) : bool;
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
}

namespace Swoole
{
  class Server extends \Swoole_server{

  }
}