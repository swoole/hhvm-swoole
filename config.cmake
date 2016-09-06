SET(CMAKE_BUILD_TYPE Debug)
HHVM_EXTENSION(hhvm_swoole swoole.cpp swoole_timer.cpp swoole_client.cpp)
target_link_libraries(hhvm_swoole swoole)
HHVM_SYSTEMLIB(hhvm_swoole ext_swoole.php)