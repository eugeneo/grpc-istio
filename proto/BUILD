package(default_visibility = ["//visibility:public"])

load("@com_github_grpc_grpc//bazel:grpc_build_system.bzl", "grpc_proto_library")

grpc_proto_library(
  name = "proto",
  srcs=["echo-service.proto"],
  use_external = True,
)
