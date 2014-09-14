#!/bin/bash

base_dir="$(git rev-parse --show-toplevel)"
for script in "$base_dir"/hooks/*; do
  case "$script" in
    *.*)
      ;;
    *)
      cp -v $script "$base_dir"/.git/hooks
      ;;
  esac
done
