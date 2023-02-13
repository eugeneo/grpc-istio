#!/bin/bash

KUBERNATES_NAMESPACE=${KUBERNATES_NAMESPACE:-k8s-grpc-istio-test}

SERVER_IMAGE_TAG=${SERVER_IMAGE_TAG:-grpc-istio-test:latest}
SERVER_CONTAINER_NAME=${SERVER_CONTAINER_NAME:-grpc-istio-test}
SERVER_HOST=${SERVER_HOST:-127.0.0.1}
SERVER_PORT=${SERVER_PORT:-4004}

build_server=0
deploy_server=0
start_client=0
start_server=0
stop_server=0
undeploy_server=0

eval $(minikube docker-env)

while [ "$1" != "" ]; do
    case $1 in
        --build-server)
            build_server=1
            ;;
        --deploy-server)
          deploy_server=1
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
        --undeploy-server)
            undeploy_server=1
            ;;
        *)
            exit 1
    esac
    shift
done

if [ $build_server -eq 1 ]; then
    bazel build //src-server:echo-server
    echo "Building Docker image"
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

if [ $deploy_server -eq 1 ]; then
    echo "Deploying server to Kubernates"
    kubectl create namespace ${KUBERNATES_NAMESPACE}
    kubectl -n ${KUBERNATES_NAMESPACE} apply -f kubernates/server.yaml
    # Wait for the pod to be ready. If success, get URL from minikube
    # kubectl -n ${KUBERNATES_NAMESPACE} wait --for=condition=Ready pod/${SERVER_CONTAINER_NAME}
    # minikube service ${SERVER_CONTAINER_NAME} -n ${KUBERNATES_NAMESPACE} --url
fi

if [ $undeploy_server -eq 1 ]; then
    echo "Undeploying server"
    kubectl -n ${KUBERNATES_NAMESPACE} delete -f kubernates/server.yaml
fi
