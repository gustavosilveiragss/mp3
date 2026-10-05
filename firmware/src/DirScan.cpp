#include "DirScan.h"

#include <sys/stat.h>

namespace {

template <typename T>
void shift(T* list, uint8_t from, uint8_t count, int delta) {
    memmove(list + from + delta, list + from, count * sizeof(T));
}

} // namespace

namespace dirlist {
namespace {

struct Collected {
    const Out& out;
    bool forward;
    uint8_t count;
};

bool insideBound(const Scan& scan, const char* name) {
    if (!*scan.bound)
        return true;
    const int comparison = order(name, scan.bound);
    return scan.forward ? comparison > 0 : comparison < 0;
}

uint8_t sortedPosition(const Collected& collected, const char* name) {
    uint8_t position = collected.count;
    while (position > 0 && order(collected.out.names[position - 1], name) > 0)
        --position;

    return position;
}

// Lista cheia: sai o nome do extremo que não interessa. Devolve false se o novo é quem sai.
bool makeRoom(Collected& collected, uint8_t& position) {
    const bool newcomerIsFarthest = collected.forward ? position == collected.count : position == 0;
    if (newcomerIsFarthest)
        return false;

    if (!collected.forward) {
        shift(collected.out.names, 1, collected.count - 1, -1);

        if (collected.out.sizes)
            shift(collected.out.sizes, 1, collected.count - 1, -1);

        --position;
    }

    --collected.count;
    return true;
}

void collect(Collected& collected, const char* dir, const Entry& entry) {
    uint8_t position = sortedPosition(collected, entry.name);
    if (collected.count == collected.out.cap && !makeRoom(collected, position))
        return;

    const uint8_t tail = collected.count - position;
    shift(collected.out.names, position, tail, +1);

    if (collected.out.sizes)
        shift(collected.out.sizes, position, tail, +1);

    strlcpy(collected.out.names[position], entry.name, kName);

    if (collected.out.sizes)
        collected.out.sizes[position] = sizeOf(dir, entry.name);

    ++collected.count;
}

} // namespace

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
    Collected collected{out, scan.forward, 0};
    each(scan.dir, [&](const Entry& entry) {
        if (valid(entry, scan.dirs) && insideBound(scan, entry.name))
            collect(collected, scan.dir, entry);

        return true;
    });

    return collected.count;
}

} // namespace dirlist
