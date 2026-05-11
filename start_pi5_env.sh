#!/bin/sh
set -e

if [ $# -lt 1 ]; then
  echo "Usage: $0 relative_project_path"
  return 1
fi

project_path="$1"

#if ! docker image inspect lithic3d_pi5 > /dev/null 2>&1; then
  docker build \
    --build-arg UID=$(id -u) \
    --build-arg GID=$(id -g) \
    -f docker/pi5.dockerfile . -t lithic3d_pi5
#fi

docker run -it -u $(id -u):$(id -g) \
  -v "$(pwd)/${project_path}:/game" \
  -v "$(pwd):/lithic3d" \
  lithic3d_pi5 bash
