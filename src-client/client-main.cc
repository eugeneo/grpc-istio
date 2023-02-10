#include <iostream>

#include "grpcpp/grpcpp.h"

#include "proto/echo-service.grpc.pb.h"

int main(int argc, char *argv[]) {
  const char *address = "localhost:4004";
  if (argc > 1) {
    address = argv[1];
  }
  std::cout << "Connecting to " << address << std::endl;
  auto channel =
      grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
  auto client = EchoService::NewStub(std::move(channel));
  EchoRequest req;
  req.set_query("q");
  EchoResponse res;
  grpc::ClientContext ctx;
  auto status = client->SendEcho(&ctx, req, &res);
  if (status.ok()) {
    std::cout << "Success: query: \"" << res.query() << "\", backend id: \""
              << res.instance_id() << "\", peer: " << ctx.peer() << std::endl;
  } else {
    std::cerr << "Error: [" << status.error_code() << "] "
              << status.error_message() << "(" << status.error_details() << ")"
              << std::endl;
  }
  return 0;
}
