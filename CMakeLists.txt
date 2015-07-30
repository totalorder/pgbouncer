cmake_minimum_required(VERSION 3.2)
project(pgbouncer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
src/main.c
src/admin.c
src/client.c
src/dnslookup.c
src/janitor.c
src/loader.c
src/objects.c
src/pktbuf.c
src/pooler.c
src/proto.c
src/sbuf.c
src/server.c
src/stats.c
src/system.c
src/takeover.c
src/util.c
src/varcache.c
src/sharding.c)

include_directories(
include/
lib/
)
add_executable(pgbouncer ${SOURCE_FILES})