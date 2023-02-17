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

grpc::Status FillResponse(
    GetCookieResponse *response,
    const std::map<std::string, std::pair<std::string, size_t>> &other_hosts,
    int requests) {
  for (const auto &host_data : other_hosts) {
    auto *host = response->add_other_hosts();
    host->set_host(host_data.first);
    host->set_cookie(host_data.second.first);
    host->set_count(host_data.second.second);
  }
  response->set_requests(requests);
  return grpc::Status::OK;
}

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
      auto status = backend->SendEcho(&ctx, backend_req, &backend_res);
      auto response = responses->add_responses();
      response->set_peer(context->peer());
      for (const auto &name_value : ctx.GetServerInitialMetadata()) {
        auto metadata = response->add_initial_metadata();
        metadata->set_name(
            std::string(name_value.first.data(), name_value.first.length()));
        metadata->set_value(
            std::string(name_value.second.data(), name_value.second.length()));
      }
      for (const auto &name_value : ctx.GetServerTrailingMetadata()) {
        auto metadata = response->add_trailing_metadata();
        metadata->set_name(
            std::string(name_value.first.data(), name_value.first.length()));
        metadata->set_value(
            std::string(name_value.second.data(), name_value.second.length()));
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

  grpc::Status GetCookies(grpc::ServerContext *context,
                          const GetCookieRequest *request,
                          GetCookieResponse *response) {
    auto channel = grpc::CreateChannel(request->cluster_url(),
                                       grpc::InsecureChannelCredentials());
    auto stub = EchoService::NewStub(channel);
    std::map<std::string, std::pair<std::string, size_t>> hosts;
    for (int i = 0; i < request->max_requests(); ++i) {
      grpc::ClientContext ctx;
      EchoRequest req;
      req.set_query(absl::StrFormat("Request #%d to url %s", i + 1,
                                    request->cluster_url()));
      EchoResponse res;
      auto status = stub->SendEcho(&ctx, req, &res);
      if (!status.ok()) {
        auto error = response->mutable_error();
        error->set_code(status.error_code());
        error->set_message(status.error_message());
        error->set_details(status.error_details());
        return FillResponse(response, hosts, i);
      } else {
        const auto &instance_id = res.instance_id();
        const auto &metadata = ctx.GetServerInitialMetadata();
        auto it = metadata.find("set-cookie");
        absl::optional<std::string> cookie;
        if (it != metadata.end()) {
          cookie = std::string(it->second.data(), it->second.size());
        }
        if (instance_id.find(request->host_substring()) != std::string::npos) {
          response->set_host(instance_id);
          if (cookie.has_value()) {
            response->set_cookie(*cookie);
          } else {
            std::cerr << "No cookie for host " << instance_id << std::endl;
          }
          return FillResponse(response, hosts, i);
        } else {
          hosts[instance_id].first = cookie.has_value() ? *cookie : "<missing>";
          hosts[instance_id].second += 1;
        }
      }
    }
    return FillResponse(response, hosts, request->max_requests());
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
    context->AddInitialMetadata("initial-metadata-host-id", "im " + id_);
    context->AddTrailingMetadata("trailing-metadata-host-id", "tm " + id_);
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
