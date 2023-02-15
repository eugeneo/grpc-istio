# https://docs.bazel.build/versions/master/be/c-cpp.html#cc_binary
cc_binary(
  name = "echo-client",
  srcs = ["client-main.cc"],
  deps = [
    "@com_google_absl//absl/strings",
    "@com_google_absl//absl/types:optional",
    "@com_github_grpc_grpc//:grpc++",
    "@com_github_grpc_grpc//:grpc++_reflection",
    "//proto",
  ],
)
