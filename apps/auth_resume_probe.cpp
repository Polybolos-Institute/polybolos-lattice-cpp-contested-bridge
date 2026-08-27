#include "anduril_lattice_rest/client.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

std::string Env(const char* k) {
  const char* v = std::getenv(k);
  return (v && *v) ? std::string(v) : std::string();
}

std::string TrackJson(const std::string& entity_id, double lat, double lon) {
  return std::string("{") +
         "\"entityId\":\"" + entity_id + "\"," +
         "\"description\":\"Polybolos auth-resume probe\"," +
         "\"isLive\":true," +
         "\"createdTime\":\"2026-07-30T00:00:00Z\"," +
         "\"expiryTime\":\"2099-01-01T00:00:00Z\"," +
         "\"aliases\":{\"name\":\"Polybolos auth-resume\"}," +
         "\"milView\":{"
         "\"disposition\":\"DISPOSITION_FRIENDLY\","
         "\"environment\":\"ENVIRONMENT_AIR\""
         "}," +
         "\"location\":{\"position\":{"
         "\"latitudeDegrees\":" +
         std::to_string(lat) +
         ","
         "\"longitudeDegrees\":" +
         std::to_string(lon) +
         ","
         "\"altitudeHaeMeters\":1200"
         "}}," +
         "\"ontology\":{"
         "\"template\":\"TEMPLATE_TRACK\","
         "\"platformType\":\"UNKNOWN AIR VEHICLE\""
         "}," +
         "\"provenance\":{"
         "\"dataType\":\"polybolos_auth_resume\","
         "\"integrationName\":\"polybolos-lattice-cpp-contested-bridge\","
         "\"sourceUpdateTime\":\"2026-07-30T00:00:00Z\""
         "}," +
         "\"dataClassification\":{"
         "\"default\":{\"level\":\"CLASSIFICATION_LEVELS_UNCLASSIFIED\"}"
         "}"
         "}";
}

void PrintPut(const char* label, const anduril_lattice_rest::HttpResult& r) {
  std::cout << label << " status=" << r.status_code
            << " ok=" << (r.ok ? "1" : "0") << "\n";
}

}  // namespace

int main() {
  anduril_lattice_rest::Client c;
  c.SetEndpoint(Env("LATTICE_ENDPOINT"));
  c.SetCredentials(Env("LATTICE_CLIENT_ID"), Env("LATTICE_CLIENT_SECRET"),
                   Env("LATTICE_ENV_TOKEN"));
  if (!c.MissingConfig().empty()) {
    std::cerr << "missing LATTICE_* config\n";
    return 2;
  }

  std::cout << "=== AUTH RESUME PROBE ===\n";
  std::cout << "phase1: FetchToken\n";
  if (!c.FetchToken()) {
    std::cerr << "FetchToken FAIL\n";
    return 3;
  }
  std::cout << "FetchToken=OK token_len=" << c.AccessToken().size() << "\n";

  const std::string id = "polybolos-auth-resume-001";
  std::cout << "phase2: PutEntity (warm token)\n";
  auto put1 = c.PutEntityJson(TrackJson(id, 32.91, -96.81));
  PrintPut("PutEntity", put1);
  if (!put1.ok) {
    return 4;
  }

  std::cout << "phase3: ClearAccessToken (simulate expiry / process restart)\n";
  c.ClearAccessToken();
  if (c.IsTokenValid()) {
    std::cerr << "ClearAccessToken did not invalidate cache\n";
    return 5;
  }

  std::cout << "phase4: EnsureToken + PutEntity (re-auth)\n";
  if (!c.EnsureToken()) {
    std::cerr << "EnsureToken after clear FAIL\n";
    return 6;
  }
  auto put2 = c.PutEntityJson(TrackJson(id, 32.912, -96.812));
  PrintPut("PutEntity", put2);
  if (!put2.ok) {
    return 7;
  }

  std::cout << "phase5: denial window (no PUT) then resume publish\n";
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  auto put3 = c.PutEntityJson(TrackJson(id, 32.914, -96.814));
  PrintPut("PutEntity", put3);
  if (!put3.ok) {
    return 8;
  }

  std::cout << "RESULT: PASS (OAuth re-auth + entity publish resume)\n";
  std::cout << "disclaimer: Independent Polybolos sample. Not an Anduril "
               "product. Advisory tracks only.\n";
  return 0;
}
