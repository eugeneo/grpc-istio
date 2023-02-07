#include <iostream>

#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/health_check_service_interface.h"

#include "proto/echo-service.grpc.pb.h"

class EchoServiceImpl final : public EchoService::Service {
public:
  explicit EchoServiceImpl(const std::string &id) : id_(id) {}

private:
  grpc::Status SendEcho(grpc::ServerContext *context,
                        const EchoRequest *request,
                        EchoResponse *response) override {
    std::cout << "Request: \"" << request->query() << "\" from "
              << context->peer() << std::endl;
    response->set_instance_id(id_);
    response->set_query(request->query());
    return grpc::Status::OK;
  }

  const std::string id_;
};

void RunServer(const std::string &address, const std::string &id) {
  EchoServiceImpl service(id);
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  grpc::ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  std::cout << "Instance #" << id << " is listening on " << address
            << std::endl;
  server->Wait();
}

int main() {
  RunServer("127.0.0.1:4004", "id");
  return 0;
}
