#!/bin/sh
set -e
cd "$(dirname "$0")/.."
mkdir -p results
docker compose -f docker/compose.yaml build
docker compose -f docker/compose.yaml run --rm lab1 small
docker compose -f docker/compose.yaml run --rm lab1 all
echo "Done. Everything is in results/"
