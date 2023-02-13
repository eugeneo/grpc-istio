#include <iostream>
#include <optional>

#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/health_check_service_interface.h"

#include "proto/echo-service.grpc.pb.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

ABSL_FLAG(std::string, id, "<id_unset>", "Instance id for logging purposes");
ABSL_FLAG(std::string, bind_address, "0.0.0.0:4004", "Address to listen on");
ABSL_FLAG(absl::optional<std::string>, backend_url, absl::nullopt,
          "Specify backend URL if this is a t1 instance");

class T1ServiceImpl final : public EchoService::Service {
public:
  explicit T1ServiceImpl(absl::string_view id, const std::string &backend_url)
      : id_(id) {
    auto channel =
        grpc::CreateChannel(backend_url, grpc::InsecureChannelCredentials());
    backend_ = EchoService::NewStub(channel);
  }

private:
  grpc::Status SendEcho(grpc::ServerContext *context,
                        const EchoRequest *request,
                        EchoResponse *response) override {
    std::cout << "T1 Request: \"" << request->query() << "\" from "
              << context->peer() << std::endl;
    EchoRequest backend_req;
    EchoResponse backend_res;
    grpc::ClientContext ctx;
    backend_req.set_query(request->query());
    auto status = backend_->SendEcho(&ctx, backend_req, &backend_res);
    response->set_query(request->query());
    if (!status.ok()) {
      response->set_instance_id(id_);
      response->set_error(status.error_message());
    } else {
      response->set_instance_id(backend_res.instance_id() + " <- " + id_);
      *(response->mutable_response()) = backend_res;
    }
    return grpc::Status::OK;
  }

  const std::string id_;
  std::unique_ptr<EchoService::Stub> backend_;
};

class T2ServiceImpl final : public EchoService::Service {
public:
  explicit T2ServiceImpl(absl::string_view id) : id_(id) {}

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

void RunT1Server(const std::string &t2_url, absl::string_view address,
                 absl::string_view id) {
  T1ServiceImpl service(id, t2_url);
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  grpc::ServerBuilder builder;
  builder.AddListeningPort(std::string(address),
                           grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  std::cout << "Instance #" << id << " is listening on " << address
            << " and proxies " << t2_url << std::endl;
  server->Wait();
}

void RunT2Server(absl::string_view address, absl::string_view id) {
  T2ServiceImpl service(id);
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  grpc::ServerBuilder builder;
  builder.AddListeningPort(std::string(address),
                           grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  std::cout << "Instance #" << id << " is listening on " << address
            << std::endl;
  server->Wait();
}

int main(int argc, char *argv[]) {
  absl::ParseCommandLine(argc, argv);
  auto is_proxy = absl::GetFlag(FLAGS_backend_url);
  if (is_proxy.has_value()) {
    RunT1Server(*is_proxy, absl::GetFlag(FLAGS_bind_address),
                absl::GetFlag(FLAGS_id));
  } else {
    RunT2Server(absl::GetFlag(FLAGS_bind_address), absl::GetFlag(FLAGS_id));
  }
  return 0;
}
