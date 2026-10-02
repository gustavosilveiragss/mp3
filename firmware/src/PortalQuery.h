#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// Le "k=v&k=v" do cabecalho X-Q (o WebServer nao entrega a query de um POST raw). Valores vem em
// percent-encoding UTF-8 (o cliente usa encodeURIComponent, '+' e literal). Funcoes puras.
namespace portal {

enum class Param : uint8_t { Missing,
                             Ok,
                             Bad };

inline int hexValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

inline const char* findValue(const char* query, const char* key) {
    const size_t len = strlen(key);
    while (*query) {
        if (strncmp(query, key, len) == 0 && query[len] == '=')
            return query + len + 1;
        const char* amp = strchr(query, '&');
        if (!amp)
            break;
        query = amp + 1;
    }
    return nullptr;
}

// Decodifica o valor de `key` em out (cap inclui o '\0'). Bad: % quebrado, byte nulo ou sem espaco.
inline Param queryText(const char* query, const char* key, char* out, size_t cap) {
    const char* v = findValue(query, key);
    if (!v)
        return Param::Missing;
    size_t n = 0;
    for (; *v && *v != '&'; ++v) {
        unsigned char c = static_cast<unsigned char>(*v);
        if (c == '%') {
            const int hi = hexValue(v[1]);
            const int lo = hi < 0 ? -1 : hexValue(v[2]);
            if (lo < 0)
                return Param::Bad;
            c = static_cast<unsigned char>(hi << 4 | lo);
            v += 2;
        }
        if (c == 0 || n + 1 >= cap)
            return Param::Bad;
        out[n++] = static_cast<char>(c);
    }
    out[n] = '\0';
    return Param::Ok;
}

// Inteiro decimal sem sinal; vazio ou com lixo ou acima de 32 bits e Bad.
inline Param queryUint(const char* query, const char* key, uint32_t& out) {
    const char* v = findValue(query, key);
    if (!v)
        return Param::Missing;
    uint64_t n = 0;
    size_t digits = 0;
    for (; *v && *v != '&'; ++v, ++digits) {
        if (*v < '0' || *v > '9')
            return Param::Bad;
        n = n * 10 + static_cast<uint64_t>(*v - '0');
        if (n > UINT32_MAX)
            return Param::Bad;
    }
    if (digits == 0)
        return Param::Bad;
    out = static_cast<uint32_t>(n);
    return Param::Ok;
}

} // namespace portal
