load("@io_bazel_rules_docker//cc:image.bzl", "cc_image")

# https://docs.bazel.build/versions/master/be/c-cpp.html#cc_binary
cc_binary(
  name = "echo-server",
  srcs = ["server_main.cc"],
  deps = [
    "@com_google_absl//absl/flags:flag",
    "@com_google_absl//absl/flags:parse",
    "@com_github_grpc_grpc//:grpc++",
    "@com_github_grpc_grpc//:grpc++_reflection",
    "//proto",
  ],
)
