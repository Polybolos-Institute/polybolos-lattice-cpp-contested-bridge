#include "anduril_lattice_rest/client.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

static std::string Env(const char* k) {
  const char* v = std::getenv(k);
  return (v && *v) ? std::string(v) : std::string();
}

int main() {
  anduril_lattice_rest::Client c;
  c.SetEndpoint(Env("LATTICE_ENDPOINT"));
  c.SetCredentials(Env("LATTICE_CLIENT_ID"), Env("LATTICE_CLIENT_SECRET"),
                   Env("LATTICE_ENV_TOKEN"));
  const auto missing = c.MissingConfig();
  if (!missing.empty()) {
    std::cerr << "missing config\n";
    return 2;
  }
  std::cout << "endpoint_host=" << c.EndpointHost()
            << " tls=" << (c.UsesTls() ? "1" : "0") << "\n";
  const bool tok = c.FetchToken();
  std::cout << "FetchToken=" << (tok ? "OK" : "FAIL") << "\n";
  if (!tok) {
    return 3;
  }
  std::cout << "token_len=" << c.AccessToken().size() << "\n";
  const std::string entity =
      "{"
      "\"entityId\":\"polybolos-probe-001\","
      "\"description\":\"Polybolos contested probe\","
      "\"isLive\":true,"
      "\"createdTime\":\"2026-07-30T00:00:00Z\","
      "\"expiryTime\":\"2099-01-01T00:00:00Z\","
      "\"aliases\":{\"name\":\"Polybolos probe\"},"
      "\"milView\":{"
      "\"disposition\":\"DISPOSITION_FRIENDLY\","
      "\"environment\":\"ENVIRONMENT_AIR\""
      "},"
      "\"location\":{\"position\":{"
      "\"latitudeDegrees\":32.9,"
      "\"longitudeDegrees\":-96.8,"
      "\"altitudeHaeMeters\":1000"
      "}},"
      "\"ontology\":{"
      "\"template\":\"TEMPLATE_TRACK\","
      "\"platformType\":\"UNKNOWN AIR VEHICLE\""
      "},"
      "\"provenance\":{"
      "\"dataType\":\"polybolos_probe\","
      "\"integrationName\":\"polybolos-lattice-cpp-contested-bridge\","
      "\"sourceUpdateTime\":\"2026-07-30T00:00:00Z\""
      "},"
      "\"dataClassification\":{"
      "\"default\":{\"level\":\"CLASSIFICATION_LEVELS_UNCLASSIFIED\"}"
      "}"
      "}";
  const auto put = c.PutEntityJson(entity);
  std::cout << "PutEntity status=" << put.status_code
            << " ok=" << (put.ok ? "1" : "0")
            << " body_len=" << put.body.size() << "\n";
  if (!put.body.empty() && put.body.size() < 400) {
    std::cout << "PutEntity body=" << put.body << "\n";
  }
  return put.ok ? 0 : 4;
}
