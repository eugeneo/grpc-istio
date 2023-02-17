#include <iostream>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"

#include "proto/echo-service.grpc.pb.h"

using google::protobuf::TextFormat;

constexpr absl::string_view kDefaultServiceAddress =
    "xds:///echo-backend-service:4004";

absl::optional<std::string> GetCookie(FrontendResponse res) {
  for (const auto &metadata : res.initial_metadata()) {
    if (metadata.name() == "set-cookie") {
      return metadata.value();
    }
  }
  return absl::nullopt;
}

absl::StatusOr<std::map<std::string, std::string>> GetCookies(
    const std::unique_ptr<FrontendService::Stub> &client, absl::string_view url,
    size_t reqs,
    std::map<const absl::string_view, const absl::string_view> metadata) {
  std::cout << absl::StrFormat("Calling %s %d times", url, reqs) << std::endl;
  FrontendResponses res;
  grpc::ClientContext ctx;
  FrontendRequest req;
  req.set_backend_url(std::string(url));
  req.set_num_requests(reqs);
  for (const auto &header : metadata) {
    auto *metadata_entry = req.add_metadata();
    metadata_entry->set_name(std::string(header.first));
    metadata_entry->set_value(std::string(header.second));
  }
  auto status = client->CallBackend(&ctx, req, &res);
  std::string errors;
  if (status.ok()) {
    std::map<std::string, std::string> host_cookie;
    std::string msg;
    TextFormat::PrintToString(res, &msg);
    std::cout << "Success:\n" << msg << std::endl;
    for (const auto &response : res.responses()) {
      const auto &cookie = GetCookie(response);
      if (cookie.has_value()) {
        host_cookie.emplace(response.response().response().instance_id(),
                            *cookie);
      }
    }
    return host_cookie;
  } else {
    return absl::InternalError(
        absl::StrFormat("[%d] %s (%s)", status.error_code(),
                        status.error_details(), status.error_details()));
  }
}

absl::optional<absl::string_view>
FindHostCookie(const std::map<std::string, std::string> &hosts_cookies,
               absl::string_view host_substring) {
  for (const auto &host_cookie : hosts_cookies) {
    if (absl::string_view(host_cookie.first).find(host_substring) !=
        std::string::npos) {
      return host_cookie.second;
    }
  }
  return absl::nullopt;
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
  auto cookies = GetCookies(client, kDefaultServiceAddress, 100, {});
  if (!cookies.ok()) {
    std::cerr << cookies.status() << std::endl;
    return 1;
  }
  auto cookie = FindHostCookie(*cookies, "g2");
  if (!cookie.has_value()) {
    std::cerr << "Cookie for a host was not found" << cookie.has_value()
              << std::endl;
    return 1;
  }
  std::cout << *cookie << std::endl;
  auto status =
      GetCookies(client, kDefaultServiceAddress, 100, {{"cookie", *cookie}});
  if (!cookies.ok()) {
    std::cerr << cookies.status() << std::endl;
    return 1;
  }
  for (const auto &host_header : *status) {
    std::cout << "!" << host_header.first << ": " << host_header.second
              << std::endl;
  }
  return 0;
}
