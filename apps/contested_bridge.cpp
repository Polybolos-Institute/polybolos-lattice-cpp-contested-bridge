#include "anduril_lattice_rest/client.hpp"
#include "contested/track_engine.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string EnvOr(const char* key, const char* fallback) {
  const char* v = std::getenv(key);
  return (v && *v) ? std::string(v) : std::string(fallback);
}

bool EnvFlag(const char* key) {
  const char* v = std::getenv(key);
  return v && (*v == '1' || *v == 't' || *v == 'T' || *v == 'y' || *v == 'Y');
}

bool LinkUpForFrame(int frame, int up, int down) {
  const int cycle = up + down;
  if (cycle <= 0) {
    return true;
  }
  return (frame % cycle) < up;
}

double Percentile(std::vector<double> xs, double p) {
  if (xs.empty()) {
    return 0.0;
  }
  std::sort(xs.begin(), xs.end());
  if (xs.size() == 1) {
    return xs[0];
  }
  const double k = (xs.size() - 1) * (p / 100.0);
  const std::size_t f = static_cast<std::size_t>(k);
  const std::size_t c = (std::min)(f + 1, xs.size() - 1);
  if (f == c) {
    return xs[f];
  }
  return xs[f] + (xs[c] - xs[f]) * (k - static_cast<double>(f));
}

void PrintReport(const contested::RunReport& r, bool dry_run) {
  std::cout << "\n=== CONTESTED BRIDGE EVIDENCE ===\n";
  std::cout << "mode: " << (dry_run ? "dry-run (no Lattice PUT)" : "live/mock PUT")
            << "\n";
  std::cout << "frames: " << r.frames << "  link_up: " << r.link_up_frames
            << "  link_down: " << r.link_down_frames
            << "  wall_s=" << r.wall_s << "\n";
  std::cout << "fuse_us p50=" << r.fuse_us_p50 << " p95=" << r.fuse_us_p95
            << " mean=" << r.fuse_us_mean << "\n";
  std::cout << "put_ms p50=" << r.put_ms_p50 << " p95=" << r.put_ms_p95
            << " mean=" << r.put_ms_mean << "\n";
  std::cout << "puts ok=" << r.puts_ok << " fail=" << r.puts_fail
            << " http_403=" << r.puts_403 << "\n";
  if (r.puts_ok + r.puts_fail > 0) {
    const double rate =
        100.0 * static_cast<double>(r.puts_ok) /
        static_cast<double>(r.puts_ok + r.puts_fail);
    std::cout << "put_success_pct=" << rate << "\n";
  }
  for (const auto& t : r.tracks) {
    std::cout << "  track " << t.track_id
              << " confirmed=" << (t.confirmed ? "yes" : "no")
              << " coast_frames=" << t.coast_frames
              << " denied_frames=" << t.denied_frames
              << " put_ok=" << t.publish_ok << "\n";
  }
  std::cout << "policy: edge fusion continues when link is denied; "
               "publish resumes when link returns. no client publish throttle.\n";
  std::cout << "disclaimer: Independent Polybolos sample. Not an Anduril "
               "product. Advisory tracks only. No weapons authority.\n";
}

}  // namespace

int main(int argc, char** argv) {
  bool dry_run = EnvFlag("CONTESTED_DRY_RUN");
  int frames = 120;
  int up = 20;
  int down = 10;
  int tracks_n = 3;
  std::string id_prefix = "polybolos-contested";
  const double dt = 0.05;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--dry-run") {
      dry_run = true;
    } else if (a == "--frames" && i + 1 < argc) {
      frames = std::atoi(argv[++i]);
    } else if (a == "--up" && i + 1 < argc) {
      up = std::atoi(argv[++i]);
    } else if (a == "--down" && i + 1 < argc) {
      down = std::atoi(argv[++i]);
    } else if (a == "--tracks" && i + 1 < argc) {
      tracks_n = std::atoi(argv[++i]);
    } else if (a == "--prefix" && i + 1 < argc) {
      id_prefix = argv[++i];
    } else if (a == "--help") {
      std::cout
          << "contested_bridge [--dry-run] [--frames N] [--up N] [--down N] "
             "[--tracks N] [--prefix ID]\n"
          << "Env: LATTICE_ENDPOINT LATTICE_CLIENT_ID LATTICE_CLIENT_SECRET "
             "LATTICE_ENV_TOKEN\n";
      return 0;
    }
  }
  if (tracks_n < 1) {
    tracks_n = 1;
  }
  if (tracks_n > 50) {
    tracks_n = 50;
  }

  contested::TrackEngine engine;
  engine.Reset(tracks_n, id_prefix);

  anduril_lattice_rest::Client client;
  if (!dry_run) {
    client.SetEndpoint(EnvOr("LATTICE_ENDPOINT", ""));
    client.SetCredentials(EnvOr("LATTICE_CLIENT_ID", ""),
                          EnvOr("LATTICE_CLIENT_SECRET", ""),
                          EnvOr("LATTICE_ENV_TOKEN", ""));
    const auto missing = client.MissingConfig();
    if (!missing.empty()) {
      std::cerr << "Missing config:";
      for (const auto& m : missing) {
        std::cerr << " " << m;
      }
      std::cerr << "\nUse --dry-run for offline contested fusion evidence.\n";
      return 2;
    }
    if (!client.EnsureToken()) {
      std::cerr << "OAuth/token failed against " << client.EndpointHost()
                << "\n";
      return 3;
    }
  }

  std::vector<double> fuse_us;
  std::vector<double> put_ms;
  int link_up = 0;
  int link_down = 0;
  const auto wall0 = std::chrono::steady_clock::now();

  for (int f = 0; f < frames; ++f) {
    const bool up_now = LinkUpForFrame(f, up, down);
    if (up_now) {
      ++link_up;
    } else {
      ++link_down;
    }
    const contested::FrameStats st = engine.Step(dt, up_now);
    fuse_us.push_back(st.fuse_us);

    if (!up_now) {
      continue;
    }
    for (auto& t : engine.TracksMutable()) {
      if (dry_run) {
        ++t.publish_ok;
        continue;
      }
      const auto t0 = std::chrono::steady_clock::now();
      const auto res = client.PutEntityJson(engine.EntityJson(t));
      const auto t1 = std::chrono::steady_clock::now();
      put_ms.push_back(
          std::chrono::duration<double, std::milli>(t1 - t0).count());
      if (res.ok) {
        ++t.publish_ok;
      } else if (res.status_code == 403) {
        ++t.publish_403;
        ++t.publish_fail;
      } else {
        ++t.publish_fail;
      }
    }
  }

  const auto wall1 = std::chrono::steady_clock::now();
  contested::RunReport report =
      contested::Summarize(fuse_us, engine.Tracks(), link_up, link_down);
  report.wall_s = std::chrono::duration<double>(wall1 - wall0).count();
  report.put_ms_p50 = Percentile(put_ms, 50);
  report.put_ms_p95 = Percentile(put_ms, 95);
  double put_sum = 0.0;
  for (double v : put_ms) {
    put_sum += v;
  }
  report.put_ms_mean = put_ms.empty() ? 0.0 : put_sum / put_ms.size();
  PrintReport(report, dry_run);

  int confirmed = 0;
  for (const auto& t : report.tracks) {
    if (t.confirmed) {
      ++confirmed;
    }
  }
  if (report.link_down_frames <= 0) {
    std::cerr << "FAIL: contested schedule produced no denial frames\n";
    return 4;
  }
  if (confirmed < 1) {
    std::cerr << "FAIL: no confirmed tracks after contested run\n";
    return 5;
  }
  std::cout << "RESULT: PASS (edge tracks survived contested windows)\n";
  return 0;
}
