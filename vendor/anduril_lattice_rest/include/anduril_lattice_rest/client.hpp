#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace anduril_lattice_rest {

struct HttpResult {
  int status_code = 0;
  std::string body;
  bool ok = false;
};

/// Thin WinHTTP Lattice client: OAuth + entity PUT + optional SSE.
/// Supports https:// (Sandboxes) and http:// (local mock Lattice).
/// Door-only helper. No C2 / ROE / class map. Not an Anduril product.
class Client {
 public:
  void SetEndpoint(std::string host_port);
  void SetCredentials(std::string client_id, std::string client_secret,
                      std::string sandbox_bearer);

  [[nodiscard]] std::vector<std::string> MissingConfig() const;
  [[nodiscard]] const std::string& EndpointHost() const { return host_; }
  [[nodiscard]] const std::string& AccessToken() const { return access_token_; }
  [[nodiscard]] const std::string& SandboxBearer() const {
    return sandbox_bearer_;
  }
  [[nodiscard]] bool UsesTls() const { return use_tls_; }

  bool FetchToken();
  bool EnsureToken();
  /// Drop cached OAuth access token (forces FetchToken on next EnsureToken).
  void ClearAccessToken();
  [[nodiscard]] bool IsTokenValid() const;

  /// PUT /api/v1/entities with raw Entity JSON (entityId in body).
  HttpResult PutEntityJson(const std::string& entity_json);

  /// POST /api/v1/entities/stream (SSE). Calls on_event for each data payload.
  /// Stops after max_seconds or when on_event returns false.
  bool StreamEntities(
      double max_seconds,
      const std::function<bool(const std::string& data_json)>& on_event);

 private:
  std::string host_;
  bool use_tls_ = true;
  std::uint16_t port_ = 443;
  std::string client_id_;
  std::string client_secret_;
  std::string sandbox_bearer_;
  std::string access_token_;
  std::chrono::steady_clock::time_point token_expiry_{};

  void RefreshEndpointParts();
};

}  // namespace anduril_lattice_rest
