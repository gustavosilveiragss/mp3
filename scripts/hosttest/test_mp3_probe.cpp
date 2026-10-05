#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Mp3Probe.cpp"

using Bytes = std::vector<uint8_t>;

static size_t readFrom(void* ctx, uint32_t offset, uint8_t* buf, size_t n) {
    const Bytes& data = *static_cast<const Bytes*>(ctx);
    if (offset >= data.size())
        return 0;
    const size_t got = n < data.size() - offset ? n : data.size() - offset;
    memcpy(buf, data.data() + offset, got);
    return got;
}

static bool probe(const Bytes& data, mrm::Mp3Info& info) {
    return mrm::probeMp3(readFrom, const_cast<Bytes*>(&data), static_cast<uint32_t>(data.size()), info);
}

// version: 3 = MPEG-1, 2 = MPEG-2, 0 = MPEG-2.5
static uint32_t header(uint32_t version, uint32_t bitrateIdx, uint32_t rateIdx, bool mono, bool padding = false) {
    return 0xFFE00000u | version << 19 | 1u << 17 | 1u << 16 | bitrateIdx << 12 | rateIdx << 10 | (padding ? 1u << 9 : 0u) | (mono ? 3u << 6 : 0u);
}

static void putHeader(Bytes& out, uint32_t h) {
    for (int shift = 24; shift >= 0; shift -= 8)
        out.push_back(static_cast<uint8_t>(h >> shift));
}

static void putBe32(Bytes& out, size_t at, uint32_t v) {
    for (int k = 0; k < 4; ++k)
        out[at + k] = static_cast<uint8_t>(v >> (24 - 8 * k));
}

static Bytes frames(uint32_t h, size_t len, int count) {
    Bytes out;
    for (int i = 0; i < count; ++i) {
        const size_t start = out.size();
        putHeader(out, h);
        out.resize(start + len, 0x55);
    }
    return out;
}

static Bytes id3v2(size_t payload) {
    Bytes tag = {'I', 'D', '3', 3, 0, 0, 0, 0, static_cast<uint8_t>(payload >> 7 & 0x7F), static_cast<uint8_t>(payload & 0x7F)};
    tag.resize(10 + payload, 0);
    return tag;
}

struct Case {
    uint32_t version, bitrateIdx, rateIdx;
    bool mono;
    uint32_t rate, kbps, len;
};

static void testHeaders() {
    // len = (144 | 72) * kbps * 1000 / rate; v2/v2.5 usam 72.
    const Case cases[] = {
        {3, 9, 1, false, 48000, 128, 384},   {3, 9, 0, false, 44100, 128, 417}, {3, 5, 2, true, 32000, 64, 288},
        {3, 13, 0, true, 44100, 256, 835},   {2, 8, 0, false, 22050, 64, 208},  {2, 4, 1, true, 24000, 32, 96},
        {2, 12, 2, false, 16000, 128, 576},  {0, 8, 0, false, 11025, 64, 417},  {0, 6, 1, true, 12000, 48, 288},
        {0, 2, 2, true, 8000, 16, 144},
    };
    for (const Case& c : cases) {
        Bytes data = frames(header(c.version, c.bitrateIdx, c.rateIdx, c.mono), c.len, 3);
        mrm::Mp3Info info;
        assert(probe(data, info));
        assert(info.sampleRate == c.rate);
        assert(info.kbps == c.kbps);
        assert(info.channels == (c.mono ? 1 : 2));
        assert(info.dataOffset == 0 && info.firstFrame == 0);
        assert(info.audioBytes == data.size());
        assert(!info.hasToc);
        assert(info.durationMs == uint64_t(data.size()) * 8 / c.kbps);
    }
}

static void testPadding() {
    Bytes data = frames(header(3, 9, 0, false, true), 418, 3);
    mrm::Mp3Info info;
    assert(probe(data, info));
    assert(info.sampleRate == 44100 && info.kbps == 128);
}

static void testId3Skip() {
    Bytes data = id3v2(300);
    const Bytes audio = frames(header(3, 9, 0, false), 417, 3);
    data.insert(data.end(), audio.begin(), audio.end());
    mrm::Mp3Info info;
    assert(probe(data, info));
    assert(info.dataOffset == 310 && info.firstFrame == 310);
    assert(info.audioBytes == audio.size());
    assert(info.durationMs == uint64_t(audio.size()) * 8 / 128);
}

static void testId3v1Excluded() {
    Bytes data = frames(header(3, 9, 0, false), 417, 3);
    const size_t audio = data.size();
    Bytes tag(128, 0);
    memcpy(tag.data(), "TAG", 3);
    data.insert(data.end(), tag.begin(), tag.end());
    mrm::Mp3Info info;
    assert(probe(data, info));
    assert(info.durationMs == audio * 8 / 128);
    assert(info.audioBytes == data.size());
}

static Bytes xingFrame(const char* tag, uint32_t flags, uint32_t frameCount, uint32_t byteCount, const uint8_t* toc, uint32_t h, size_t len) {
    Bytes f = frames(h, len, 1);
    const size_t at = 4 + 32; // MPEG-1 estereo
    memcpy(&f[at], tag, 4);
    putBe32(f, at + 4, flags);
    size_t p = at + 8;
    if (flags & 1) {
        putBe32(f, p, frameCount);
        p += 4;
    }
    if (flags & 2) {
        putBe32(f, p, byteCount);
        p += 4;
    }
    if (flags & 4)
        memcpy(&f[p], toc, 100);
    return f;
}

static void testXingToc() {
    uint8_t toc[100];
    for (int i = 0; i < 100; ++i)
        toc[i] = static_cast<uint8_t>(i * 255 / 100);
    const uint32_t h = header(3, 9, 0, false);
    Bytes data = xingFrame("Xing", 7, 1000, 417 * 1000, toc, h, 417);
    const Bytes audio = frames(h, 417, 3);
    data.insert(data.end(), audio.begin(), audio.end());
    mrm::Mp3Info info;
    assert(probe(data, info));
    assert(info.dataOffset == 417 && info.firstFrame == 0);
    // 1000 frames x 1152 / 44100 s; a taxa media (data.size) diverge muito, entao a tabela fica
    assert(info.durationMs == 26122);
    assert(info.hasToc);
    assert(info.audioBytes == data.size()); // bytes do Xing (417000) excede o arquivo
    assert(info.toc[50] == toc[50]);
    // busca interpolada pela tabela
    assert(mrm::mp3OffsetAt(info, 0) == info.dataOffset);
    const uint32_t mid = mrm::mp3OffsetAt(info, info.durationMs / 2);
    assert(mid == uint64_t(info.audioBytes) * toc[50] / 256);
    assert(mrm::mp3OffsetAt(info, info.durationMs * 2) == mrm::mp3OffsetAt(info, info.durationMs));
}

static void testInfoCbrDropsToc() {
    uint8_t toc[100];
    for (int i = 0; i < 100; ++i)
        toc[i] = static_cast<uint8_t>(i * 255 / 100);
    const uint32_t h = header(3, 9, 0, false);
    // 1000 frames CBR: a duracao do Xing bate com a taxa media -> a tabela e descartada
    const uint32_t total = 1001 * 417;
    Bytes data = xingFrame("Info", 7, 1000, total, toc, h, 417);
    const Bytes audio = frames(h, 417, 1000);
    data.insert(data.end(), audio.begin(), audio.end());
    mrm::Mp3Info info;
    assert(probe(data, info));
    assert(info.dataOffset == 417);
    assert(!info.hasToc);
    assert(info.durationMs == 26122);
    assert(info.audioBytes == total);
}

static void testXingWithoutFrames() {
    uint8_t toc[100] = {};
    const uint32_t h = header(3, 9, 0, false);
    Bytes data = xingFrame("Xing", 2 | 4, 0, 1668, toc, h, 417);
    const Bytes audio = frames(h, 417, 3);
    data.insert(data.end(), audio.begin(), audio.end());
    mrm::Mp3Info info;
    assert(probe(data, info));
    assert(info.dataOffset == 417);
    assert(!info.hasToc);
    assert(info.durationMs == uint64_t(data.size() - 417) * 8 / 128);
    assert(info.audioBytes == 1668);
}

static void testVbri() {
    const uint32_t h = header(3, 9, 0, false);
    Bytes data = frames(h, 417, 1);
    memcpy(&data[36], "VBRI", 4);
    putBe32(data, 36 + 14, 2000);
    const Bytes audio = frames(h, 417, 3);
    data.insert(data.end(), audio.begin(), audio.end());
    mrm::Mp3Info info;
    assert(probe(data, info));
    assert(info.dataOffset == 417);
    assert(info.durationMs == 52244); // 2000 x 1152 / 44.1 kHz
    assert(!info.hasToc);
}

static void testMpeg2XingOffset() {
    // MPEG-2 estereo: side info de 17 bytes, "Xing" em 4 + 17
    const uint32_t h = header(2, 8, 0, false);
    Bytes data = frames(h, 208, 1);
    memcpy(&data[21], "Xing", 4);
    putBe32(data, 25, 1);
    putBe32(data, 29, 100);
    const Bytes audio = frames(h, 208, 3);
    data.insert(data.end(), audio.begin(), audio.end());
    mrm::Mp3Info info;
    assert(probe(data, info));
    assert(info.dataOffset == 208);
    assert(info.durationMs == 2612); // 100 frames x 576 / 22050
}

static void testLinearSeek() {
    Bytes data = frames(header(3, 9, 0, false), 417, 10);
    mrm::Mp3Info info;
    assert(probe(data, info));
    assert(mrm::mp3OffsetAt(info, 0) == 0);
    const uint32_t half = mrm::mp3OffsetAt(info, info.durationMs / 2);
    assert(half == data.size() / 2);
    assert(mrm::mp3OffsetAt(info, info.durationMs + 5000) == data.size() - 1);
}

static void testGarbage() {
    mrm::Mp3Info info;
    assert(!probe(Bytes(2000, 0x00), info));
    assert(!probe(Bytes(2000, 0xFF), info));
    assert(!probe(Bytes(), info));
    assert(!probe(Bytes{0xFF, 0xFB, 0x90}, info));
    // sync valido mas camada II, versao reservada, bitrate livre e invalido, taxa reservada
    assert(!probe(frames(0xFFFD9000u, 417, 3), info));
    assert(!probe(frames(0xFFE89000u, 417, 3), info));
    assert(!probe(frames(header(3, 0, 0, false), 417, 3), info));
    assert(!probe(frames(header(3, 15, 0, false), 417, 3), info));
    assert(!probe(frames(header(3, 9, 3, false), 417, 3), info));
    // frame sem sucessor valido
    Bytes lone = frames(header(3, 9, 0, false), 417, 1);
    lone.resize(1000, 0);
    assert(!probe(lone, info));
    // frame unico que termina o arquivo vale
    assert(probe(frames(header(3, 9, 0, false), 417, 1), info));
    // sync depois de lixo
    Bytes shifted(100, 0);
    const Bytes audio = frames(header(3, 9, 0, false), 417, 3);
    shifted.insert(shifted.end(), audio.begin(), audio.end());
    assert(probe(shifted, info) && info.dataOffset == 100);
    // taxa diferente no segundo frame nao confirma
    Bytes mixed = frames(header(3, 9, 0, false), 417, 1);
    const Bytes other = frames(header(3, 9, 1, false), 384, 1);
    mixed.insert(mixed.end(), other.begin(), other.end());
    mixed.resize(mixed.size() + 500, 0);
    assert(!probe(mixed, info));
}

static void testBadId3() {
    mrm::Mp3Info info;
    Bytes data = id3v2(10);
    data[6] = 0x80; // tamanho nao synchsafe
    const Bytes audio = frames(header(3, 9, 0, false), 417, 3);
    data.insert(data.end(), audio.begin(), audio.end());
    assert(!probe(data, info));
    Bytes huge = id3v2(10);
    huge[8] = 0x7F; // tag maior que o arquivo
    assert(!probe(huge, info));
}

int main() {
    testHeaders();
    testPadding();
    testId3Skip();
    testId3v1Excluded();
    testXingToc();
    testInfoCbrDropsToc();
    testXingWithoutFrames();
    testVbri();
    testMpeg2XingOffset();
    testLinearSeek();
    testGarbage();
    testBadId3();
    puts("test_mp3_probe ok");
}
