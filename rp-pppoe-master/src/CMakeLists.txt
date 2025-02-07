cmake_minimum_required(VERSION 3.10)
project(rp-pppoe)

add_library(rp-pppoe
    common.c
    control_socket.c
    debug.c
    discovery.c
    if.c
    md5.c
    plugin.c
    ppp.c
    pppoe.c
    relay.c
)

target_include_directories(rp-pppoe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
