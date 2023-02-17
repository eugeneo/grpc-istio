#!/bin/bash

KUBERNATES_NAMESPACE=${KUBERNATES_NAMESPACE:-k8s-grpc-istio-test}
FRONTEND_SERVICE=${FRONTEND_SERVICE:-echo-frontend-service}
SERVER_IMAGE_TAG=${SERVER_IMAGE_TAG:-grpc-istio-test:latest}
SERVER_CONTAINER_NAME=${SERVER_CONTAINER_NAME:-grpc-istio-test}
SERVER_HOST=${SERVER_HOST:-127.0.0.1}
SERVER_PORT=${SERVER_PORT:-4004}

build_server=0
deploy_server=0
print_url=0
start_client=0
start_k8_client=0
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
        --print-url)
            print_url=1
            ;;
        --start-client)
            start_client=1
            ;;
        --start-k8-client)
            start_k8_client=1
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
    docker build -t ${SERVER_IMAGE_TAG} -f src-server/Dockerfile bazel-bin/src-server/
    kubectl rollout restart -f kubernates/echo-deployments.yaml
fi

if [ $start_server -eq 1 ]; then
    echo "Starting gRPC server"
    docker run --name ${SERVER_CONTAINER_NAME} -it \
      -p ${SERVER_HOST}:${SERVER_PORT}:4004 ${SERVER_IMAGE_TAG}
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
    kubectl apply -f kubernates
fi

if [ $undeploy_server -eq 1 ]; then
    echo "Undeploying server"
    kubectl delete -f kubernates
fi

if [ $print_url -eq 1 ]; then
    minikube -n ${KUBERNATES_NAMESPACE} service ${FRONTEND_SERVICE} --url
fi

if [ $start_k8_client -eq 1 ]; then
    backend_url_prefixed=$(minikube -n ${KUBERNATES_NAMESPACE} service ${FRONTEND_SERVICE} --url)
    backend_url=${backend_url_prefixed#http://}
    echo "Running a client to connect to ${backend_url}"
    bazel run //src-client:echo-client -- ${backend_url}
fi
