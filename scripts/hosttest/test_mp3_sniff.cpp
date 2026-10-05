#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <vector>

#include "Mp3Sniff.h"

using Bytes = std::vector<uint8_t>;

static Bytes frames(uint32_t header, size_t len, int count) {
    Bytes out;
    for (int i = 0; i < count; ++i) {
        const size_t start = out.size();
        for (int shift = 24; shift >= 0; shift -= 8)
            out.push_back(static_cast<uint8_t>(header >> shift));
        out.resize(start + len, 0x55);
    }
    return out;
}

static mrm::UploadVerdict sniff(const Bytes& data, size_t slice, uint32_t* rate = nullptr) {
    mrm::Mp3Sniff s;
    mrm::UploadVerdict verdict = mrm::UploadVerdict::Need;
    for (size_t i = 0; i < data.size() && verdict == mrm::UploadVerdict::Need; i += slice)
        verdict = s.feed(data.data() + i, data.size() - i < slice ? data.size() - i : slice);
    if (rate)
        *rate = s.sampleRate();
    return verdict;
}

int main() {
    using V = mrm::UploadVerdict;
    uint32_t rate = 0;
    const uint32_t v1 = 0xFFFB9000, v2 = 0xFFF38000 /* MPEG-2 22,05 kHz 64 kbps */;
    for (size_t slice : {size_t(1), size_t(7), size_t(4096)}) {
        assert(sniff(frames(v1, 417, 3), slice, &rate) == V::Ok && rate == 44100);
        assert(sniff(frames(v2, 208, 3), slice, &rate) == V::Ok && rate == 22050);
        assert(sniff(Bytes(6000, 0), slice) == V::Bad);
        assert(sniff(frames(v1, 417, 1), slice) == V::Need);
    }
    Bytes tagged = {'I', 'D', '3', 3, 0, 0, 0, 0, 0, 5, 1, 2, 3, 4, 5};
    const Bytes audio = frames(v1, 417, 3);
    tagged.insert(tagged.end(), audio.begin(), audio.end());
    assert(sniff(tagged, 3, &rate) == V::Ok && rate == 44100);
    Bytes badTag = {'I', 'D', '3', 3, 0, 0, 0x80, 0, 0, 5};
    assert(sniff(badTag, 5) == V::Bad);
    puts("test_mp3_sniff ok");
}
