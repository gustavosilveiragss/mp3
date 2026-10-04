#pragma once

#include <Arduino.h>
#include <dirent.h>

#include <PathRules.h>

#include "Media.h"

// Varredura de pastas por opendir/readdir: nao abre arquivo, entao nao gasta slot do cartao (o Arduino abre um
// fopen por entrada) e anda bem mais rapido em cartao cheio. Os caminhos sao relativos a raiz da midia.
namespace dirlist {

constexpr size_t kName = mrm::kMaxTrackLen + 1; // cabe nome de faixa e de playlist
constexpr size_t kPath = 128;                   // "/music/<playlist>/<faixa>" com folga (maior caso: 104)

struct Entry {
    const char* name;
    bool dir;
};

// Ordem da biblioteca inteira e do portal: sem diferenciar caixa, com desempate pelos bytes.
int order(const char* a, const char* b);

// Faixas sao .mp3 de nome valido, playlists sao pastas de nome valido.
inline bool valid(const Entry& e, bool dirs) {
    return e.dir == dirs && (dirs ? mrm::validPlaylistName(e.name) : mrm::validTrackName(e.name));
}

// Chama fn(Entry) para cada entrada da pasta; fn devolve false para parar.
template <typename Fn>
void each(const char* dir, Fn fn) {
    char path[kPath];
    snprintf(path, sizeof(path), "%s%s", media::mountPoint(), dir);
    DIR* d = opendir(path);
    if (!d)
        return;
    for (const dirent* e = readdir(d); e; e = readdir(d)) {
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..") && !fn(Entry{e->d_name, e->d_type == DT_DIR}))
            break;
    }
    closedir(d);
}

bool exists(const char* dir);
bool isEmpty(const char* dir);
uint32_t sizeOf(const char* dir, const char* name);                    // bytes do arquivo, 0 se nao achar
uint16_t count(const char* dir, bool dirs, uint32_t* bytes = nullptr); // validos, e a soma dos tamanhos se `bytes`

// O que varrer: `bound` vazio nao limita; senao vale so o que vem depois (forward) ou antes dele.
struct Scan {
    const char* dir;
    bool dirs;
    const char* bound;
    bool forward;
};

// Onde guardar: ate `cap` nomes (e tamanhos, se `sizes`), sempre em ordem crescente.
struct Out {
    char (*names)[kName];
    uint32_t* sizes;
    uint8_t cap;
};

// Os `cap` menores nomes validos depois de `bound` (forward) ou os maiores antes dele. Uma passada pelo diretorio,
// sem guardar a lista inteira.
uint8_t chunk(const Scan& scan, const Out& out);

} // namespace dirlist
