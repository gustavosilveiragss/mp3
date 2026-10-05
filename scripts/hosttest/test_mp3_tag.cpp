#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Mp3Tag.cpp"

using Bytes = std::vector<uint8_t>;

static size_t readFrom(void* ctx, uint32_t offset, uint8_t* buf, size_t n) {
    const Bytes& data = *static_cast<const Bytes*>(ctx);
    if (offset >= data.size())
        return 0;
    const size_t got = n < data.size() - offset ? n : data.size() - offset;
    memcpy(buf, data.data() + offset, got);
    return got;
}

static std::string artistOf(const Bytes& data, bool* found = nullptr) {
    char out[mrm::kMaxArtistLen + 1];
    memset(out, 'x', sizeof(out));
    const bool ok = mrm::readMp3Artist(readFrom, const_cast<Bytes*>(&data), static_cast<uint32_t>(data.size()), out);
    if (found)
        *found = ok;
    assert(ok == (out[0] != '\0'));
    return out;
}

static Bytes bytes(std::initializer_list<int> list) {
    Bytes out;
    for (int v : list)
        out.push_back(static_cast<uint8_t>(v));
    return out;
}

static void append(Bytes& out, const Bytes& more) { out.insert(out.end(), more.begin(), more.end()); }

static void putSize(Bytes& out, uint32_t size, bool synchsafe) {
    if (synchsafe)
        append(out, bytes({int(size >> 21 & 0x7F), int(size >> 14 & 0x7F), int(size >> 7 & 0x7F), int(size & 0x7F)}));
    else
        append(out, bytes({int(size >> 24), int(size >> 16 & 0xFF), int(size >> 8 & 0xFF), int(size & 0xFF)}));
}

static Bytes frame(const char* id, uint8_t version, const Bytes& body, uint8_t flags = 0) {
    Bytes f(id, id + 4);
    putSize(f, static_cast<uint32_t>(body.size()), version == 4);
    f.push_back(0);
    f.push_back(flags);
    append(f, body);
    return f;
}

static Bytes withText(uint8_t encoding, const Bytes& text) {
    Bytes body = {encoding};
    append(body, text);
    return body;
}

static Bytes tag(uint8_t version, uint8_t flags, const Bytes& content, const Bytes& extended = {}) {
    Bytes t = {'I', 'D', '3', version, 0, flags};
    putSize(t, static_cast<uint32_t>(content.size() + extended.size()), true);
    append(t, extended);
    append(t, content);
    return t;
}

static Bytes ascii(const char* s) { return Bytes(s, s + strlen(s)); }

static Bytes v1Tag(const char* artist) {
    Bytes t(128, 0);
    memcpy(t.data(), "TAG", 3);
    memcpy(&t[33], artist, strlen(artist));
    return t;
}

static void testEncodings() {
    for (uint8_t version : {uint8_t(3), uint8_t(4)}) {
        assert(artistOf(tag(version, 0, frame("TPE1", version, withText(0, ascii("Marmota"))))) == "Marmota");
        assert(artistOf(tag(version, 0, frame("TPE1", version, withText(0, bytes({'J', 'o', 0xE3, 'o'}))))) == "Jo\xC3\xA3o");
        assert(artistOf(tag(version, 0, frame("TPE1", version, withText(1, bytes({0xFF, 0xFE, 'A', 0, 0xE9, 0, 'x', 0}))))) == "A\xC3\xA9x");
        assert(artistOf(tag(version, 0, frame("TPE1", version, withText(1, bytes({0xFE, 0xFF, 0, 'B', 0, 'c'}))))) == "Bc");
        assert(artistOf(tag(version, 0, frame("TPE1", version, withText(1, bytes({'H', 0, 'i', 0}))))) == "Hi"); // sem BOM: little-endian
        assert(artistOf(tag(version, 0, frame("TPE1", version, withText(2, bytes({0, 'H', 0, 'i'}))))) == "Hi");
        assert(artistOf(tag(version, 0, frame("TPE1", version, withText(3, bytes({'C', 0xC3, 0xA9, 'u'}))))) == "C\xC3\xA9u");
    }
}

static void testGlyphsAndPunctuation() {
    // fora do Latin-1 vira um unico '?'; aspas e travessos viram ASCII
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(3, bytes({'A', 0xE6, 0x97, 0xA5, 0xE6, 0x9C, 0xAC, 'B'}))))) == "A?B");
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(3, bytes({0xE2, 0x80, 0x98, 'a', 0xE2, 0x80, 0x99, 0xE2, 0x80, 0x9C, 'b', 0xE2, 0x80, 0x9D, 0xE2, 0x80, 0x93, 0xE2, 0x80, 0x94}))))) == "'a'\"b\"--");
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(1, bytes({0xFF, 0xFE, 0x3D, 0xD8, 0x00, 0xDE, 'z', 0}))))) == "?z"); // par substituto
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(1, bytes({0xFF, 0xFE, 0x00, 0xD8, 'z', 0}))))) == "?z");            // substituto solto
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(3, bytes({0xFF, 'a'}))))) == "?a");                                   // UTF-8 invalido
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(0, bytes({' ', ' ', 'A', 0x01, 'b', ' ', ' '}))))) == "Ab");   // espacos e controles
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(0, bytes({'A', 0, 'B'}))))) == "A");                                 // primeiro de varios
}

static void testLengthLimit() {
    const std::string longName(100, 'a');
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(0, ascii(longName.c_str()))))) == std::string(mrm::kMaxArtistLen, 'a'));
    // 2 bytes por caractere: cabe 24 de 2 bytes
    Bytes accents;
    for (int i = 0; i < 60; ++i)
        accents.push_back(0xE9);
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(0, accents)))).size() == mrm::kMaxArtistLen);
}

static void testSizesAndFrames() {
    // frames antes do TPE1, versao 2.3 (tamanho comum) e 2.4 (synchsafe, 200 bytes = 0x01 0x48)
    for (uint8_t version : {uint8_t(3), uint8_t(4)}) {
        Bytes content = frame("TIT2", version, withText(0, Bytes(200, 'T')));
        append(content, frame("TPE1", version, withText(0, ascii("Depois"))));
        assert(artistOf(tag(version, 0, content)) == "Depois");
    }
    // padding antes do TPE1 encerra a busca
    Bytes padded = frame("TIT2", 3, withText(0, ascii("t")));
    append(padded, Bytes(20, 0));
    append(padded, frame("TPE1", 3, withText(0, ascii("Perdido"))));
    bool found = true;
    assert(artistOf(tag(3, 0, padded), &found).empty() && !found);
    // tamanho de frame maior que a tag
    Bytes big = Bytes{'T', 'P', 'E', '1', 0, 0, 0x7F, 0, 0, 0, 0, 'a'};
    assert(artistOf(tag(3, 0, big), &found).empty() && !found);
    // 2.4 com tamanho nao synchsafe
    Bytes bad = frame("TPE1", 4, withText(0, ascii("x")));
    bad[4] = 0x80;
    assert(artistOf(tag(4, 0, bad), &found).empty() && !found);
    // 2.2 e ignorada
    assert(artistOf(tag(2, 0, frame("TPE1", 3, withText(0, ascii("x")))), &found).empty() && !found);
    // mais de 64 frames sem TPE1
    Bytes many;
    for (int i = 0; i < 70; ++i)
        append(many, frame("TXXX", 3, withText(0, ascii("v"))));
    append(many, frame("TPE1", 3, withText(0, ascii("Longe"))));
    assert(artistOf(tag(3, 0, many), &found).empty() && !found);
}

static void testExtendedHeader() {
    // 2.3: tamanho (6) nao conta o proprio campo; 2.4: synchsafe (6) conta
    Bytes ext3 = bytes({0, 0, 0, 6, 0, 0, 0, 0, 0, 0});
    assert(artistOf(tag(3, 0x40, frame("TPE1", 3, withText(0, ascii("Ext3"))), ext3)) == "Ext3");
    Bytes ext4 = bytes({0, 0, 0, 6, 1, 0});
    assert(artistOf(tag(4, 0x40, frame("TPE1", 4, withText(0, ascii("Ext4"))), ext4)) == "Ext4");
    // cabecalho estendido maior que a tag
    bool found = true;
    Bytes huge = bytes({0, 0, 0x7F, 0x7F});
    assert(artistOf(tag(4, 0x40, frame("TPE1", 4, withText(0, ascii("x"))), huge), &found).empty() && !found);
}

static void testFrameFlags() {
    // comprimido (2.3: 0x80) e cifrado (0x40) sao ignorados
    bool found = true;
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(0, ascii("Zip")), 0x80)), &found).empty() && !found);
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(0, ascii("Enc")), 0x40)), &found).empty() && !found);
    // 2.4: compressao 0x08 e cifra 0x04
    assert(artistOf(tag(4, 0, frame("TPE1", 4, withText(0, ascii("Zip")), 0x08)), &found).empty() && !found);
    assert(artistOf(tag(4, 0, frame("TPE1", 4, withText(0, ascii("Enc")), 0x04)), &found).empty() && !found);
    // agrupamento: 2.3 byte extra (0x20); 2.4 byte extra (0x40) e indicador de tamanho (0x01, 4 bytes)
    assert(artistOf(tag(3, 0, frame("TPE1", 3, bytes({7, 0, 'G', 'r'}), 0x20))) == "Gr");
    assert(artistOf(tag(4, 0, frame("TPE1", 4, bytes({7, 0, 0, 0, 0, 0, 'G', 'r'}), 0x41))) == "Gr");
    // unsynchronisation por frame (2.4 0x02) e por tag (0x80): FF 00 vira FF
    Bytes latin = bytes({0xFF, 0, 'x'}); // FF 00 -> FF
    assert(artistOf(tag(4, 0, frame("TPE1", 4, withText(0, latin), 0x02))) == "\xC3\xBF" "x");
    assert(artistOf(tag(3, 0x80, frame("TPE1", 3, withText(0, latin)))) == "\xC3\xBF" "x");
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(0, latin)))) == "\xC3\xBF"); // sem unsync o 00 termina o texto
}

static void testInvalidEncodingAndEmpty() {
    bool found = true;
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(4, ascii("x")))), &found).empty() && !found);
    assert(artistOf(tag(3, 0, frame("TPE1", 3, bytes({0}))), &found).empty() && !found);
    assert(artistOf(tag(3, 0, frame("TPE1", 3, withText(0, ascii("   ")))), &found).empty() && !found);
}

static void testId3v1() {
    Bytes data(500, 0);
    append(data, v1Tag("Antigo"));
    assert(artistOf(data) == "Antigo");
    Bytes latin(300, 0);
    Bytes t = v1Tag("");
    t[33] = 0xE9;
    t[34] = 'x';
    append(latin, t);
    assert(artistOf(latin) == "\xC3\xA9x");
    // v1 sem artista, e v2 sem TPE1 cai para a v1
    bool found = true;
    Bytes noName(300, 0);
    append(noName, v1Tag(""));
    assert(artistOf(noName, &found).empty() && !found);
    Bytes both = tag(3, 0, frame("TIT2", 3, withText(0, ascii("t"))));
    append(both, Bytes(200, 0));
    append(both, v1Tag("Fallback"));
    assert(artistOf(both) == "Fallback");
    // v2 vence a v1
    Bytes pref = tag(3, 0, frame("TPE1", 3, withText(0, ascii("V2"))));
    append(pref, v1Tag("V1"));
    assert(artistOf(pref) == "V2");
}

static void testAbsentOrTruncated() {
    bool found = true;
    assert(artistOf(Bytes(), &found).empty() && !found);
    assert(artistOf(Bytes(50, 0x11), &found).empty() && !found);
    assert(artistOf(bytes({'I', 'D', '3', 3, 0}), &found).empty() && !found);
    Bytes full = tag(3, 0, frame("TPE1", 3, withText(0, ascii("Cortado"))));
    Bytes cut(full.begin(), full.begin() + 15);
    assert(artistOf(cut, &found).empty() && !found);
    Bytes half(full.begin(), full.end() - 3); // texto cortado: tamanho do frame passa do fim
    assert(artistOf(half, &found).empty() && !found);
    // tag com tamanho nao synchsafe
    Bytes bad = full;
    bad[6] = 0x80;
    assert(artistOf(bad, &found).empty() && !found);
}

int main() {
    testEncodings();
    testGlyphsAndPunctuation();
    testLengthLimit();
    testSizesAndFrames();
    testExtendedHeader();
    testFrameFlags();
    testInvalidEncodingAndEmpty();
    testId3v1();
    testAbsentOrTruncated();
    puts("test_mp3_tag ok");
}
