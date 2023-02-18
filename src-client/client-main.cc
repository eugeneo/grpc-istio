#include <iostream>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"

#include "proto/echo-service.grpc.pb.h"

using google::protobuf::TextFormat;

const std::string kDefaultServiceAddress = "xds:///echo-backend-service:4004";

absl::optional<std::string> GetCookie(FrontendResponse res) {
  if (res.has_cookie()) {
    return res.cookie();
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

bool CheckStatus(grpc::Status status) {
  if (status.ok()) {
    return true;
  }
  std::cerr << absl::StrFormat("Error [%d] %s (%s)", status.error_code(),
                               status.error_message(), status.error_details())
            << std::endl;
  return false;
}

std::map<std::string, std::pair<std::set<std::string>, size_t>>
GetServerCallCounts(const FrontendResponses &res) {
  std::map<std::string, std::pair<std::set<std::string>, size_t>> counts;
  for (const auto &response : res.responses()) {
    auto &r = response.response().response();
    if (response.has_cookie()) {
      counts[r.instance_id()].first.insert(response.cookie());
    }
    counts[r.instance_id()].second += 1;
  }
  return counts;
}

absl::optional<std::vector<std::pair<std::string, std::string>>>
CallBackend(const std::unique_ptr<FrontendService::Stub> &client,
            size_t requests_count, absl::optional<absl::string_view> cookie,
            FrontendRequest_Method method) {
  grpc::ClientContext ctx;
  FrontendRequest req;
  FrontendResponses res;
  req.set_backend_url(kDefaultServiceAddress);
  req.set_num_requests(requests_count);
  req.set_method(method);
  if (cookie.has_value()) {
    auto header = req.add_metadata();
    header->set_name("cookie");
    header->set_value(std::string(*cookie));
  }
  if (!CheckStatus(client->CallBackend(&ctx, req, &res))) {
    return absl::nullopt;
  }
  std::vector<std::pair<std::string, std::string>> result;
  for (const auto &host_count : GetServerCallCounts(res)) {
    std::cout << host_count.first << "\t" << host_count.second.second << "\t"
              << absl::StrJoin(host_count.second.first, ", ") << std::endl;
    std::string cookie =
        host_count.second.first.empty() ? "" : *host_count.second.first.begin();
    result.emplace_back(host_count.first, cookie);
  }

  return result;
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
  const size_t kReqs = 300;
  std::cout << absl::StreamFormat(
                   "Let's send %d requests spread over all backends", kReqs)
            << std::endl;
  auto backends =
      CallBackend(client, kReqs, absl::nullopt, FrontendRequest_Method_echo1);
  if (!backends.has_value() || backends->empty()) {
    return 1;
  }
  const auto &host_cookie = backends->at(backends->size() / 2);
  std::cout << std::endl;
  std::cout << absl::StreamFormat(
                   "Let's send %d requests to %s backend (Echo method)", kReqs,
                   host_cookie.first)
            << std::endl;
  if (!CallBackend(client, kReqs, host_cookie.second,
                   FrontendRequest_Method_echo1)
           .has_value()) {
    return 1;
  }
  std::cout << std::endl;
  std::cout << absl::StreamFormat(
                   "Let's send %d requests to %s backend (Echo2 method)", kReqs,
                   host_cookie.first)
            << std::endl;
  if (!CallBackend(client, kReqs, host_cookie.second,
                   FrontendRequest_Method_echo2)
           .has_value()) {
    return 1;
  }
  return 0;
}
