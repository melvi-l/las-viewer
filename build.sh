#!/usr/bin/env bash
set -e

SRC_DIR="./src"
BUILD_DIR="./build"
HOST_EXEC="app"
LIB_NAME="libapp.so"

COMMON_LIBS="-lvulkan -lglfw -ldl"

SUBCMD="${1:-all}"
MODE="${2:-debug}"

cflags_for_mode() {
  case "$1" in
    debug)
      echo "-g3 -O0 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wno-missing-field-initializers -DVK_ENABLE_VALIDATION -DHOT_RELOAD" # -DDEBUG -DVK_ENABLE_VALIDATION 
      ;;
    debug-sanitize)
      echo "-g3 -O0 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wno-missing-field-initializers -DVK_ENABLE_VALIDATION -fanalyzer -fsanitize=address,undefined -fno-omit-frame-pointer -DDEBUG -DVK_ENABLE_VALIDATION -DHOT_RELOAD"
      ;;
    release)
      echo "-O2 -DNDEBUG"
      ;;
    *)
      printf 'unknown mode: %s\n' "$1" >&2
      return 1
      ;;
  esac
}

build_host() {
  local cflags; cflags=$(cflags_for_mode "$MODE")
  local t0=$SECONDS
  g++ $cflags $extra "$SRC_DIR/host/main.cpp" -o "$BUILD_DIR/$HOST_EXEC" \
    $COMMON_LIBS -std=c++20 -I"$SRC_DIR" -Iexternal
  printf '[host]    built %s (%s, %ds)\n' "$HOST_EXEC" "$MODE" "$((SECONDS - t0))"
}

build_lib() {
  local cflags; cflags=$(cflags_for_mode "$MODE")
  local t0=$SECONDS
  g++ $cflags -shared -fPIC "$SRC_DIR/lib/main.cpp" -o "$BUILD_DIR/$LIB_NAME" \
    $COMMON_LIBS -std=c++20 -I"$SRC_DIR" -Iexternal
  printf '[lib]     built %s (%s, %ds)\n' "$LIB_NAME" "$MODE" "$((SECONDS - t0))"
}

run_host() {
  printf '[run]     %s\n' "$HOST_EXEC"
  "$BUILD_DIR/$HOST_EXEC"
}

mkdir -p "$BUILD_DIR"

case "$SUBCMD" in
  all)
    build_host
    build_lib
    run_host
    ;;
  build)
    build_host
    build_lib
    ;;
  host)
    build_host
    ;;
  lib)
    build_lib
    ;;
  run)
    run_host
    ;;
  *)
    cat <<EOF >&2
Usage: $0 <command> [mode]

Commands:
  all       
  build     
  host      
  lib       
  run       

Modes:

Examples:
  $0                    
EOF
    exit 1
    ;;
esac

