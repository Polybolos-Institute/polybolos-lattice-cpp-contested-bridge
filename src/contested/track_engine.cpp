#include "contested/track_engine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>

namespace contested {
namespace {

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

}  // namespace

void TrackEngine::Reset(int num_tracks) {
  Reset(num_tracks, "polybolos-contested");
}

void TrackEngine::Reset(int num_tracks, const std::string& id_prefix) {
  tracks_.clear();
  frame_ = 0;
  for (int i = 0; i < num_tracks; ++i) {
    TrackState t;
    t.track_id = id_prefix + "-" + std::to_string(i + 1);
    t.lat = 32.90 + 0.01 * i;
    t.lon = -96.75 - 0.02 * i;
    t.alt_m = 3000.0 + 100.0 * i;
    t.vel_mps = 120.0 + 10.0 * i;
    t.heading_deg = 45.0 + 15.0 * i;
    t.confirmed = false;
    tracks_.push_back(t);
  }
}

FrameStats TrackEngine::Step(double dt_s, bool link_up) {
  using clock = std::chrono::steady_clock;
  const auto t0 = clock::now();

  constexpr double kMetersPerDegLat = 111320.0;
  for (auto& t : tracks_) {
    const double h = t.heading_deg * 3.14159265358979323846 / 180.0;
    const double dn = t.vel_mps * std::cos(h) * dt_s;
    const double de = t.vel_mps * std::sin(h) * dt_s;
    t.lat += dn / kMetersPerDegLat;
    t.lon += de / (kMetersPerDegLat * std::cos(t.lat * 3.14159265358979323846 / 180.0));
    if (!link_up) {
      t.coast_frames += 1;
      t.denied_frames += 1;
    } else if (t.coast_frames > 0 || frame_ > 5) {
      t.confirmed = true;
    }
  }

  const auto t1 = clock::now();
  FrameStats s;
  s.fuse_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
  s.link_up = link_up;
  s.tracks = static_cast<int>(tracks_.size());
  ++frame_;
  return s;
}

std::string TrackEngine::EntityJson(const TrackState& t) const {
  std::ostringstream o;
  o.setf(std::ios::fixed);
  o.precision(7);
  o << "{"
    << "\"entityId\":\"" << t.track_id << "\","
    << "\"description\":\"Polybolos contested track\","
    << "\"isLive\":true,"
    << "\"createdTime\":\"2026-07-30T00:00:00Z\","
    << "\"expiryTime\":\"2099-01-01T00:00:00Z\","
    << "\"aliases\":{\"name\":\"Polybolos contested track\"},"
    << "\"milView\":{"
    << "\"disposition\":\"DISPOSITION_FRIENDLY\","
    << "\"environment\":\"ENVIRONMENT_AIR\""
    << "},"
    << "\"location\":{\"position\":{"
    << "\"latitudeDegrees\":" << t.lat << ","
    << "\"longitudeDegrees\":" << t.lon << ","
    << "\"altitudeHaeMeters\":" << t.alt_m
    << "}},"
    << "\"ontology\":{"
    << "\"template\":\"TEMPLATE_TRACK\","
    << "\"platformType\":\"UNKNOWN AIR VEHICLE\""
    << "},"
    << "\"provenance\":{"
    << "\"dataType\":\"polybolos_contested\","
    << "\"integrationName\":\"polybolos-lattice-cpp-contested-bridge\","
    << "\"sourceUpdateTime\":\"2026-07-30T00:00:00Z\""
    << "},"
    << "\"dataClassification\":{"
    << "\"default\":{\"level\":\"CLASSIFICATION_LEVELS_UNCLASSIFIED\"}"
    << "}"
    << "}";
  return o.str();
}

RunReport Summarize(const std::vector<double>& fuse_us,
                    const std::vector<TrackState>& tracks, int link_up,
                    int link_down) {
  RunReport r;
  r.frames = static_cast<int>(fuse_us.size());
  r.link_up_frames = link_up;
  r.link_down_frames = link_down;
  r.fuse_us_p50 = Percentile(fuse_us, 50);
  r.fuse_us_p95 = Percentile(fuse_us, 95);
  double sum = 0.0;
  for (double v : fuse_us) {
    sum += v;
  }
  r.fuse_us_mean = fuse_us.empty() ? 0.0 : sum / fuse_us.size();
  r.tracks = tracks;
  for (const auto& t : tracks) {
    r.puts_ok += t.publish_ok;
    r.puts_fail += t.publish_fail;
    r.puts_403 += t.publish_403;
  }
  return r;
}

}  // namespace contested
