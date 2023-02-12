load("@io_bazel_rules_docker//cc:image.bzl", "cc_image")

# https://docs.bazel.build/versions/master/be/c-cpp.html#cc_binary
cc_binary(
  name = "echo_server",
  srcs = ["server_main.cc"],
  deps = [
    "@com_github_grpc_grpc//:grpc++",
    "@com_github_grpc_grpc//:grpc++_reflection",
    "//proto",
  ],
)

cc_image(
  name = "echo_server_image",
  binary = ":echo_server",
  ports = ["4004:4004"],
  base = "@ubuntu_22_04//image"
)
