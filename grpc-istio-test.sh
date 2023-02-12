#!/bin/bash

SERVER_IMAGE_TAG=${SERVER_IMAGE_TAG:-grpc-istio-test:latest}
SERVER_CONTAINER_NAME=${SERVER_CONTAINER_NAME:-grpc-istio-test}
SERVER_HOST=${SERVER_HOST:-127.0.0.1}
SERVER_PORT=${SERVER_PORT:-4004}

build_server=0
start_client=0
start_server=0
stop_server=0

while [ "$1" != "" ]; do
    case $1 in
        --build-server)
            build_server=1
            ;;
        --start-client)
            start_client=1
            ;;
        --start-server)
            start_server=1
            ;;
        --stop-server)
            stop_server=1
            ;;
        *)
            exit 1
    esac
    shift
done

if [ $build_server -eq 1 ]; then
    echo "Building Docker image"
    bazel build //src-server:echo_server
    docker build -t ${SERVER_IMAGE_TAG} -f src-server/Dockerfile bazel-bin/src-server
fi

if [ $start_server -eq 1 ]; then
    echo "Starting gRPC server"
    docker run --name ${SERVER_CONTAINER_NAME} -it -p ${SERVER_HOST}:${SERVER_PORT}:4004 ${SERVER_IMAGE_TAG}
fi

if [ $stop_server -eq 1 ]; then
    echo "Stopping the server"
    docker container rm -f ${SERVER_CONTAINER_NAME}
fi

if [ $start_client -eq 1 ]; then
    echo "Connecting to ${SERVER_HOST}:${SERVER_PORT}"
    bazel run //src-client:echo-client ${SERVER_HOST}:${SERVER_PORT}
fi
