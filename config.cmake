SET(CMAKE_BUILD_TYPE Debug)
include_directories(BEFORE $ENV{SWOOLE_DIR} $ENV{SWOOLE_DIR}/include)
link_directories($ENV{SWOOLE_DIR}/lib)
HHVM_EXTENSION(hhvm_swoole swoole.cpp swoole_timer.cpp swoole_client.cpp)
target_link_libraries(hhvm_swoole swoole)
HHVM_SYSTEMLIB(hhvm_swoole ext_swoole.php)
