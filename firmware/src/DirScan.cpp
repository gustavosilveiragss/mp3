#include "DirScan.h"

#include <sys/stat.h>

namespace {

template <typename T>
void shift(T* list, uint8_t from, uint8_t count, int delta) {
    memmove(list + from + delta, list + from, count * sizeof(T));
}

} // namespace

namespace dirlist {

// Sem diferenciar caixa, com desempate pelos bytes: a mesma ordem em toda a biblioteca e no portal.
int order(const char* a, const char* b) {
    const int c = strcasecmp(a, b);
    return c ? c : strcmp(a, b);
}

bool exists(const char* dir) {
    char path[kPath];
    snprintf(path, sizeof(path), "%s%s", media::mountPoint(), dir);
    struct stat st;
    return stat(path, &st) == 0;
}

bool isEmpty(const char* dir) {
    bool empty = true;
    each(dir, [&](const Entry&) { return empty = false; });
    return empty;
}

uint32_t sizeOf(const char* dir, const char* name) {
    char path[kPath + kName];
    snprintf(path, sizeof(path), "%s%s/%s", media::mountPoint(), dir, name);
    struct stat st;
    return stat(path, &st) == 0 ? static_cast<uint32_t>(st.st_size) : 0;
}

uint16_t count(const char* dir, bool dirs, uint32_t* bytes) {
    uint16_t n = 0;
    if (bytes)
        *bytes = 0;
    each(dir, [&](const Entry& e) {
        if (valid(e, dirs)) {
            ++n;
            if (bytes)
                *bytes += sizeOf(dir, e.name);
        }
        return true;
    });
    return n;
}

uint8_t chunk(const Scan& scan, const Out& out) {
    const bool forward = scan.forward;
    const char* bound = scan.bound;
    char (*names)[kName] = out.names;
    uint32_t* sizes = out.sizes;
    const uint8_t cap = out.cap;
    uint8_t n = 0;
    each(scan.dir, [&](const Entry& e) {
        if (!valid(e, scan.dirs) || (*bound && (forward ? order(e.name, bound) <= 0 : order(e.name, bound) >= 0)))
            return true;
        uint8_t at = n;
        while (at > 0 && order(names[at - 1], e.name) > 0)
            --at;
        if (n == cap) { // cheio: sai o nome do extremo que nao interessa
            if (forward ? at == n : at == 0)
                return true;
            if (!forward) {
                shift(names, 1, n - 1, -1);
                if (sizes)
                    shift(sizes, 1, n - 1, -1);
                --at;
            }
            --n;
        }
        shift(names, at, n - at, +1);
        if (sizes)
            shift(sizes, at, n - at, +1);
        strlcpy(names[at], e.name, kName);
        if (sizes)
            sizes[at] = sizeOf(scan.dir, e.name);
        ++n;
        return true;
    });
    return n;
}

} // namespace dirlist
