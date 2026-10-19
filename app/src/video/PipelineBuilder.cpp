#include "PipelineBuilder.h"

#include <cctype>

namespace video {

namespace {

const char* kSink =
    " ! videoconvert ! video/x-raw,format=RGBx"
    " ! appsink name=sink max-buffers=1 drop=true sync=false emit-signals=false";

bool startsWith(const std::string& s, const std::string& p) { return s.compare(0, p.size(), p) == 0; }

bool parsePort(const std::string& s, int& port) {
    if (s.empty() || s.size() > 5) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    port = std::stoi(s);
    return port > 0 && port <= 65535;
}

// gst_parse_launch'a giden metne kullanıcı girdisi gömüleceği için tırnak,
// boşluk ve '!' gibi boru hattı söz dizimi karakterleri reddedilir.
bool safeLocation(const std::string& s) {
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == '"' || c == '\'' || c == '!' ||
            c == '\\') {
            return false;
        }
    }
    return true;
}

}  // namespace

std::optional<PipelineSpec> buildPipeline(const std::string& uri) {
    if (startsWith(uri, "udp://")) {
        std::string rest = uri.substr(6);
        std::string address = "0.0.0.0";
        const auto colon = rest.rfind(':');
        if (colon != std::string::npos) {
            address = rest.substr(0, colon);
            rest = rest.substr(colon + 1);
        }
        int port = 0;
        if (!parsePort(rest, port) || address.empty() || !safeLocation(address)) return std::nullopt;
        return PipelineSpec{
            "udpsrc address=" + address + " port=" + std::to_string(port) +
                " caps=\"application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96\""
                " ! rtpjitterbuffer latency=50 ! rtph264depay ! h264parse ! avdec_h264" + kSink,
            "udp"};
    }
    if (startsWith(uri, "rtsp://")) {
        if (uri.size() <= 7 || !safeLocation(uri)) return std::nullopt;
        return PipelineSpec{"rtspsrc location=" + uri +
                                " latency=100 ! rtph264depay ! h264parse ! avdec_h264" + kSink,
                            "rtsp"};
    }
    if (uri == "test://" || uri == "test") {
        return PipelineSpec{
            "videotestsrc is-live=true pattern=ball ! video/x-raw,width=640,height=360,framerate=30/1" +
                std::string(kSink),
            "test"};
    }
    return std::nullopt;
}

}  // namespace video
