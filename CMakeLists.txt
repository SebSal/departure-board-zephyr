cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
# Default board; "west build -b <board>" still overrides it.
if(NOT DEFINED BOARD AND NOT DEFINED ENV{BOARD})
  set(BOARD departure_board_bat/esp32c6/hpcore)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(departure_board)

target_sources(app PRIVATE
  src/main.c
  src/hw/hw.c
  src/net/http_client.c
  src/net/ntp.c
  src/net/wifi.c
  src/ui/ui.c
  src/transport/transport.c
  src/transport/vrs.c
  src/transport/motis.c
  src/util/json_util.c
  src/util/time_util.c
  src/config/shell.c
  src/config/config.c
)

target_include_directories(app PRIVATE src lib/jsmn)

# Root certificates for the TLS peer check, turned into C array bodies for http_client.c.
set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated)
foreach(cert isrg_root_x1 isrg_root_ye)
  generate_inc_file_for_target(app certs/${cert}.der ${gen_dir}/${cert}.der.inc)
endforeach()

# Add Git version information if tagged with v.
execute_process(
  COMMAND git describe --tags --exact-match --match "v*" --dirty
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE APP_GIT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
# If the above fails, fall back to a hash of current commit.
if(NOT APP_GIT_VERSION)
  execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE APP_GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()
if(NOT APP_GIT_VERSION)
  set(APP_GIT_VERSION "unknown")
endif()
target_compile_definitions(app PRIVATE APP_GIT_VERSION="${APP_GIT_VERSION}")

# Update the tag if something in the .git directory changes.
foreach(git_path HEAD index refs/tags)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git/${git_path})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                 ${CMAKE_CURRENT_SOURCE_DIR}/.git/${git_path})
  endif()
endforeach()
