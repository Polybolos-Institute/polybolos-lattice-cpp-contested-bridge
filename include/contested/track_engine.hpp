#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace contested {

struct TrackState {
  std::string track_id;
  double lat = 0.0;
  double lon = 0.0;
  double alt_m = 0.0;
  double vel_mps = 0.0;
  double heading_deg = 0.0;
  bool confirmed = false;
  int coast_frames = 0;
  int publish_ok = 0;
  int publish_fail = 0;
  int publish_403 = 0;
  int denied_frames = 0;
};

struct FrameStats {
  double fuse_us = 0.0;
  bool link_up = true;
  int tracks = 0;
};

struct RunReport {
  int frames = 0;
  int link_up_frames = 0;
  int link_down_frames = 0;
  int puts_ok = 0;
  int puts_fail = 0;
  int puts_403 = 0;
  double fuse_us_p50 = 0.0;
  double fuse_us_p95 = 0.0;
  double fuse_us_mean = 0.0;
  double put_ms_p50 = 0.0;
  double put_ms_p95 = 0.0;
  double put_ms_mean = 0.0;
  double wall_s = 0.0;
  std::vector<TrackState> tracks;
};

/// Constant-velocity track coast under contested (denied) link windows.
/// Edge-native: fusion/coast continues when publish is denied.
class TrackEngine {
 public:
  void Reset(int num_tracks = 3);
  void Reset(int num_tracks, const std::string& id_prefix);
  FrameStats Step(double dt_s, bool link_up);
  [[nodiscard]] const std::vector<TrackState>& Tracks() const { return tracks_; }
  [[nodiscard]] std::vector<TrackState>& TracksMutable() { return tracks_; }
  [[nodiscard]] std::string EntityJson(const TrackState& t) const;

 private:
  std::vector<TrackState> tracks_;
  std::uint64_t frame_ = 0;
};

RunReport Summarize(const std::vector<double>& fuse_us,
                    const std::vector<TrackState>& tracks, int link_up,
                    int link_down);

}  // namespace contested
