#pragma once
// Video kaynağı URI'sinden GStreamer boru hattı metni üretir (GStreamer'a
// bağımlı değil; birim testi GStreamer olmadan da koşar).
//
//   udp://5600  ya da  udp://0.0.0.0:5600   RTP/H.264 (QGC ve çoğu araç varsayılanı)
//   rtsp://host:8554/yol                   RTSP üzerinden H.264
//   test://                                 videotestsrc (ağ gerektirmez)
//
// Tüm boru hatları "sink" adlı appsink ile biter; uygulama kareleri RGBx alır.

#include <optional>
#include <string>

namespace video {

struct PipelineSpec {
    std::string description;
    std::string kind;  // "udp", "rtsp", "test"
};

std::optional<PipelineSpec> buildPipeline(const std::string& uri);

}  // namespace video
