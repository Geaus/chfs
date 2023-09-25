#!/bin/bash

PARENT_DIR="$(dirname "$(realpath "$0")")"
source $PARENT_DIR/set-env.sh

tar_file="$ROOT_PATH/handin.tgz"

tar -czvf "$tar_file" -C "$(dirname "$ROOT_PATH")" "$(basename "$ROOT_PATH")"/daemons "$(basename "$ROOT_PATH")"/src
