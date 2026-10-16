#!/bin/sh
set -e
cd "$(dirname "$0")"
mkdir -p results
docker compose build
docker compose run --rm lab1 small
docker compose run --rm lab1 all
echo "Done. Everything is in results/"
