#include <iostream>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"

#include "proto/echo-service.grpc.pb.h"

void call(const std::unique_ptr<FrontendService::Stub> &client,
          absl::string_view query, absl::string_view url, size_t reqs) {
  std::cout << absl::StrFormat("Calling %s %d times", url, reqs) << std::endl;
  FrontendResponses res;
  grpc::ClientContext ctx;
  FrontendRequest req;
  req.set_backend_url(std::string(url));
  req.set_num_requests(reqs);
  auto status = client->CallBackend(&ctx, req, &res);
  if (status.ok()) {
    std::string msg;
    google::protobuf::TextFormat::PrintToString(res, &msg);
    std::cout << "Success:\n" << msg << std::endl;
  } else {
    std::cerr << "Error: [" << status.error_code() << "] "
              << status.error_message() << "(" << status.error_details() << ")"
              << std::endl;
  }
}

int main(int argc, char *argv[]) {
  const char *address = "localhost:4004";
  if (argc > 1) {
    address = argv[1];
  }
  std::cout << "Connecting to " << address << std::endl;
  auto channel =
      grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
  auto client = FrontendService::NewStub(std::move(channel));
  call(client, "q1", "xds:///echo-backend-service:4004", 6);
  return 0;
}
