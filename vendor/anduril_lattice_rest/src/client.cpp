#include "anduril_lattice_rest/client.hpp"

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

namespace anduril_lattice_rest {
namespace {

std::wstring Utf8ToWide(const std::string& s) {
  if (s.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                    static_cast<int>(s.size()), nullptr, 0);
  if (n <= 0) {
    return {};
  }
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                      w.data(), n);
  return w;
}

std::string FormEncode(const std::string& s) {
  std::ostringstream o;
  for (unsigned char c : s) {
    if (std::isalnum(c) != 0 || c == '-' || c == '_' || c == '.' || c == '~') {
      o << static_cast<char>(c);
    } else {
      static const char* kHex = "0123456789ABCDEF";
      o << '%' << kHex[c >> 4] << kHex[c & 0x0F];
    }
  }
  return o.str();
}

bool ReadHttpResponseBody(HINTERNET hRequest, std::string* out) {
  out->clear();
  for (;;) {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
      return false;
    }
    if (avail == 0) {
      break;
    }
    std::vector<char> buf(static_cast<size_t>(avail));
    DWORD read = 0;
    if (!WinHttpReadData(hRequest, buf.data(),
                         static_cast<DWORD>(buf.size()), &read)) {
      return false;
    }
    out->append(buf.data(), read);
  }
  return true;
}

bool ExtractJsonStringField(const std::string& json, const char* key,
                            std::string* out) {
  const std::string pat = std::string("\"") + key + "\":\"";
  const std::size_t p = json.find(pat);
  if (p == std::string::npos) {
    return false;
  }
  std::size_t i = p + pat.size();
  std::string s;
  while (i < json.size()) {
    if (json[i] == '"') {
      break;
    }
    if (json[i] == '\\' && i + 1 < json.size()) {
      s.push_back(json[i + 1]);
      i += 2;
      continue;
    }
    s.push_back(json[i]);
    ++i;
  }
  *out = std::move(s);
  return !out->empty();
}

bool ExtractJsonIntField(const std::string& json, const char* key, int* out) {
  const std::string pat = std::string("\"") + key + "\":";
  std::size_t p = json.find(pat);
  if (p == std::string::npos) {
    return false;
  }
  p += pat.size();
  while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) {
    ++p;
  }
  *out = static_cast<int>(std::strtol(json.c_str() + p, nullptr, 10));
  return true;
}

void ApplyTls12(HINTERNET hSession) {
#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3 0x00002000
#endif
  DWORD protocols =
      WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
  WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols,
                   sizeof(protocols));
}

}  // namespace

void Client::RefreshEndpointParts() {
  use_tls_ = true;
  port_ = 443;
  std::string t = host_;
  if (t.rfind("http://", 0) == 0) {
    use_tls_ = false;
    port_ = 80;
    t = t.substr(7);
  } else if (t.rfind("https://", 0) == 0) {
    use_tls_ = true;
    port_ = 443;
    t = t.substr(8);
  }
  while (!t.empty() && t.back() == '/') {
    t.pop_back();
  }
  const std::size_t colon = t.rfind(':');
  if (colon != std::string::npos && colon > 0) {
    const int p = std::atoi(t.c_str() + colon + 1);
    if (p > 0 && p < 65536) {
      host_ = t.substr(0, colon);
      port_ = static_cast<std::uint16_t>(p);
      return;
    }
  }
  host_ = t;
}

void Client::SetEndpoint(std::string host_port) {
  host_ = std::move(host_port);
  RefreshEndpointParts();
}

void Client::SetCredentials(std::string client_id, std::string client_secret,
                            std::string sandbox_bearer) {
  client_id_ = std::move(client_id);
  client_secret_ = std::move(client_secret);
  sandbox_bearer_ = std::move(sandbox_bearer);
  access_token_.clear();
  token_expiry_ = {};
}

std::vector<std::string> Client::MissingConfig() const {
  std::vector<std::string> missing;
  if (host_.empty()) {
    missing.emplace_back("LATTICE_ENDPOINT");
  }
  if (client_id_.empty()) {
    missing.emplace_back("LATTICE_CLIENT_ID");
  }
  if (client_secret_.empty()) {
    missing.emplace_back("LATTICE_CLIENT_SECRET");
  }
  if (sandbox_bearer_.empty()) {
    missing.emplace_back("LATTICE_ENV_TOKEN");
  }
  return missing;
}

bool Client::IsTokenValid() const {
  if (access_token_.empty()) {
    return false;
  }
  return std::chrono::steady_clock::now() < token_expiry_;
}

bool Client::FetchToken() {
  if (client_id_.empty() || client_secret_.empty() || sandbox_bearer_.empty() ||
      host_.empty()) {
    return false;
  }

  const std::string body =
      std::string("grant_type=client_credentials&client_id=") +
      FormEncode(client_id_) + "&client_secret=" + FormEncode(client_secret_);

  HINTERNET hSession = WinHttpOpen(
      L"polybolos-lattice-cpp-contested-bridge/0.1",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    return false;
  }
  if (use_tls_) {
    ApplyTls12(hSession);
  }

  HINTERNET hConnect = WinHttpConnect(hSession, Utf8ToWide(host_).c_str(),
                                      static_cast<INTERNET_PORT>(port_), 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return false;
  }

  const DWORD flags = use_tls_ ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET hRequest = WinHttpOpenRequest(
      hConnect, L"POST", L"/api/v1/oauth/token", nullptr, WINHTTP_NO_REFERER,
      nullptr, flags);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  const std::string extra =
      "Content-Type: application/x-www-form-urlencoded\r\n"
      "Anduril-Sandbox-Authorization: Bearer " +
      sandbox_bearer_ + "\r\n";
  const std::wstring wExtra = Utf8ToWide(extra);
  if (!WinHttpAddRequestHeaders(hRequest, wExtra.c_str(),
                                static_cast<DWORD>(wExtra.size()),
                                WINHTTP_ADDREQ_FLAG_ADD)) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  const BOOL sent = WinHttpSendRequest(
      hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
      reinterpret_cast<LPVOID>(const_cast<char*>(body.data())),
      static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
  if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
    const DWORD err = GetLastError();
    std::cerr << "[FetchToken] send/recv failed last_error=" << err << "\n";
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  DWORD status = 0;
  DWORD sz = sizeof(status);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz,
                      WINHTTP_NO_HEADER_INDEX);
  std::string resp;
  ReadHttpResponseBody(hRequest, &resp);
  const DWORD send_err = GetLastError();
  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);

  if (status != 200) {
    // Best-effort diagnostics for operators (no secrets).
    std::cerr << "[FetchToken] http_status=" << status
              << " body_len=" << resp.size()
              << " last_error=" << send_err << "\n";
    if (!resp.empty() && resp.size() < 200) {
      std::cerr << "[FetchToken] body=" << resp << "\n";
    }
    return false;
  }
  std::string token;
  if (!ExtractJsonStringField(resp, "access_token", &token)) {
    return false;
  }
  int expires_in = 1800;
  ExtractJsonIntField(resp, "expires_in", &expires_in);
  if (expires_in < 60) {
    expires_in = 60;
  }
  access_token_ = std::move(token);
  token_expiry_ = std::chrono::steady_clock::now() +
                  std::chrono::seconds(expires_in - 30);
  return true;
}

bool Client::EnsureToken() {
  if (IsTokenValid()) {
    return true;
  }
  // Require real OAuth access_token. Do not fall back to Sandboxes Bearer
  // as Authorization (that masks TLS/OAuth failures and yields PUT fails).
  return FetchToken();
}

void Client::ClearAccessToken() {
  access_token_.clear();
  token_expiry_ = {};
}

HttpResult Client::PutEntityJson(const std::string& entity_json) {
  HttpResult result;
  if (!EnsureToken()) {
    result.body = "EnsureToken failed";
    return result;
  }

  HINTERNET hSession = WinHttpOpen(
      L"polybolos-lattice-cpp-contested-bridge/0.1",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    result.body = "WinHttpOpen failed";
    return result;
  }
  if (use_tls_) {
    ApplyTls12(hSession);
  }
  HINTERNET hConnect = WinHttpConnect(hSession, Utf8ToWide(host_).c_str(),
                                      static_cast<INTERNET_PORT>(port_), 0);
  if (!hConnect) {
    result.body = "WinHttpConnect failed";
    WinHttpCloseHandle(hSession);
    return result;
  }
  const DWORD flags = use_tls_ ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET hRequest = WinHttpOpenRequest(
      hConnect, L"PUT", L"/api/v1/entities", nullptr, WINHTTP_NO_REFERER,
      nullptr, flags);
  if (!hRequest) {
    result.body = "WinHttpOpenRequest failed";
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
  }

  const std::string headers =
      "Content-Type: application/json\r\n"
      "Authorization: Bearer " +
      access_token_ +
      "\r\n"
      "Anduril-Sandbox-Authorization: Bearer " +
      sandbox_bearer_ + "\r\n";
  const std::wstring wHeaders = Utf8ToWide(headers);
  WinHttpAddRequestHeaders(hRequest, wHeaders.c_str(),
                           static_cast<DWORD>(wHeaders.size()),
                           WINHTTP_ADDREQ_FLAG_ADD);

  const BOOL sent = WinHttpSendRequest(
      hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
      reinterpret_cast<LPVOID>(const_cast<char*>(entity_json.data())),
      static_cast<DWORD>(entity_json.size()),
      static_cast<DWORD>(entity_json.size()), 0);
  if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
    result.body = "HTTP request failed";
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
  }

  DWORD status = 0;
  DWORD sz = sizeof(status);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz,
                      WINHTTP_NO_HEADER_INDEX);
  ReadHttpResponseBody(hRequest, &result.body);
  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  result.status_code = static_cast<int>(status);
  result.ok = (status >= 200 && status < 300);
  return result;
}

bool Client::StreamEntities(
    double max_seconds,
    const std::function<bool(const std::string& data_json)>& on_event) {
  if (!EnsureToken()) {
    return false;
  }

  HINTERNET hSession = WinHttpOpen(
      L"polybolos-lattice-cpp-contested-bridge/0.1",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    return false;
  }
  if (use_tls_) {
    ApplyTls12(hSession);
  }
  HINTERNET hConnect = WinHttpConnect(hSession, Utf8ToWide(host_).c_str(),
                                      static_cast<INTERNET_PORT>(port_), 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return false;
  }
  const DWORD flags = use_tls_ ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET hRequest = WinHttpOpenRequest(
      hConnect, L"POST", L"/api/v1/entities/stream", nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  const std::string body =
      "{\"heartbeatIntervalMS\":5000,\"preExistingOnly\":false,"
      "\"componentsToInclude\":[\"kinematics\",\"location\",\"ontology\","
      "\"milView\",\"aliases\"]}";
  const std::string headers =
      "Content-Type: application/json\r\n"
      "Accept: text/event-stream\r\n"
      "Authorization: Bearer " +
      access_token_ +
      "\r\n"
      "Anduril-Sandbox-Authorization: Bearer " +
      sandbox_bearer_ + "\r\n";
  const std::wstring wHeaders = Utf8ToWide(headers);
  WinHttpAddRequestHeaders(hRequest, wHeaders.c_str(),
                           static_cast<DWORD>(wHeaders.size()),
                           WINHTTP_ADDREQ_FLAG_ADD);

  const BOOL sent = WinHttpSendRequest(
      hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
      reinterpret_cast<LPVOID>(const_cast<char*>(body.data())),
      static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
  if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  DWORD status = 0;
  DWORD sz = sizeof(status);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz,
                      WINHTTP_NO_HEADER_INDEX);
  if (status < 200 || status >= 300) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  const auto deadline =
      std::chrono::steady_clock::now() +
      std::chrono::milliseconds(static_cast<int>(max_seconds * 1000.0));
  std::string buf;
  bool keep = true;
  while (keep && std::chrono::steady_clock::now() < deadline) {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
      break;
    }
    if (avail == 0) {
      Sleep(50);
      continue;
    }
    std::vector<char> chunk(static_cast<size_t>(avail));
    DWORD read = 0;
    if (!WinHttpReadData(hRequest, chunk.data(),
                         static_cast<DWORD>(chunk.size()), &read) ||
        read == 0) {
      break;
    }
    buf.append(chunk.data(), read);
    for (;;) {
      const std::size_t nl = buf.find('\n');
      if (nl == std::string::npos) {
        break;
      }
      std::string line = buf.substr(0, nl);
      buf.erase(0, nl + 1);
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      if (line.rfind("data:", 0) == 0) {
        std::string data = line.substr(5);
        while (!data.empty() && data.front() == ' ') {
          data.erase(data.begin());
        }
        if (!data.empty() && data != "{}" && on_event) {
          keep = on_event(data);
        }
      }
    }
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return true;
}

}  // namespace anduril_lattice_rest
