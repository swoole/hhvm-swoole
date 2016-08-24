<?hh
class Swoole_Server {
  <<__Native>>
  function __construct(string $host, int $port, int $mode =1, int $type = 3);
  <<__Native>>
  function on(string $eventType, mixed $callback) : bool;
  <<__Native>>
  function set(array $settings): bool;
  <<__Native>>
  function start(): bool;
}