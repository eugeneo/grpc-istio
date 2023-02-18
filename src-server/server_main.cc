#include <iostream>
#include <optional>

#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/health_check_service_interface.h"

#include "proto/echo-service.grpc.pb.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

ABSL_FLAG(std::string, id, "<id_unset>", "Instance id for logging purposes");
ABSL_FLAG(std::string, bind_address, "0.0.0.0:4004", "Address to listen on");
ABSL_FLAG(bool, frontend, false,
          "Specify backend URL if this is a t1 instance");

class FrontendServiceImpl final : public FrontendService::Service {
public:
  explicit FrontendServiceImpl(absl::string_view id) : id_(id) {}

private:
  grpc::Status CallBackend(grpc::ServerContext *context,
                           const FrontendRequest *request,
                           FrontendResponses *responses) override {
    std::cout << absl::StrFormat("Calling backend %s %d times",
                                 request->backend_url(),
                                 request->num_requests())
              << std::endl;
    auto channel = grpc::CreateChannel(request->backend_url(),
                                       grpc::InsecureChannelCredentials());
    auto backend = EchoService::NewStub(channel);
    for (size_t n = 0; n < request->num_requests(); n++) {
      EchoRequest backend_req;
      EchoResponse backend_res;
      grpc::ClientContext ctx;
      backend_req.set_query(absl::StrFormat("Request #%d", n + 1));
      for (int i = 0; i < request->metadata_size(); ++i) {
        const auto &metadata = request->metadata(i);
        ctx.AddMetadata(metadata.name(), metadata.value());
      }
      auto status = request->method() == FrontendRequest_Method_echo1
                        ? backend->SendEcho(&ctx, backend_req, &backend_res)
                        : backend->SendEcho2(&ctx, backend_req, &backend_res);
      auto response = responses->add_responses();
      response->set_peer(context->peer());
      const auto &initial_metadata = ctx.GetServerInitialMetadata();
      auto it = initial_metadata.find("set-cookie");
      if (it != initial_metadata.end()) {
        const auto &cookie = it->second;
        response->set_cookie(std::string(cookie.data(), cookie.size()));
      }
      if (!status.ok()) {
        auto error = response->mutable_response()->mutable_error();
        error->set_code(status.error_code());
        error->set_message(status.error_message());
        error->set_details(status.error_details());
      } else {
        *(response->mutable_response()->mutable_response()) = backend_res;
      }
    }
    return grpc::Status::OK;
  }

  const std::string id_;
  std::unique_ptr<EchoService::Stub> backend_;
};

class BackendServiceImpl final : public EchoService::Service {
public:
  explicit BackendServiceImpl(absl::string_view id) : id_(id) {}

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

  grpc::Status SendEcho2(grpc::ServerContext *context,
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

void RunFrontendServer(absl::string_view address, absl::string_view id) {
  FrontendServiceImpl service(id);
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  grpc::ServerBuilder builder;
  builder.AddListeningPort(std::string(address),
                           grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  std::cout << "Frontend instance " << id << " is listening on " << address
            << std::endl;
  server->Wait();
}

void RunBackendServer(absl::string_view address, absl::string_view id) {
  BackendServiceImpl service(id);
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

int main(int argc, char *argv[], char **envp) {
  for (auto i = envp; *i != nullptr; ++i) {
    std::cout << *i << std::endl;
  }
  absl::ParseCommandLine(argc, argv);
  if (absl::GetFlag(FLAGS_frontend)) {
    RunFrontendServer(absl::GetFlag(FLAGS_bind_address),
                      absl::GetFlag(FLAGS_id));
  } else {
    RunBackendServer(absl::GetFlag(FLAGS_bind_address),
                     absl::GetFlag(FLAGS_id));
  }
  return 0;
}
