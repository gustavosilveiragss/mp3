#include "Portal.h"

// Manutencao do cartao que o portal faz sozinha: apagar pasta, sumir com playlist vazia e desfazer troca interrompida.

// Apaga os arquivos da pasta e a pasta. Pega um nome por vez para nao mexer na pasta enquanto a lista anda.
bool Portal::purge(const char* path) {
    char name[256], file[kPathMax + sizeof(name)];
    for (;;) {
        bool found = false, isDir = false;
        dirlist::each(path, [&](const dirlist::Entry& e) {
            strlcpy(name, e.name, sizeof(name));
            isDir = e.dir;
            return !(found = true);
        });
        if (!found)
            break;
        snprintf(file, sizeof(file), "%s/%s", path, name);
        if (isDir || !fs_.remove(file))
            return false;
    }
    return fs_.rmdir(path);
}

// Primeira playlist de /music que satisfaz `test(dir)`, em `dir`; vazio se nenhuma.
template <typename Test>
static void findPlaylist(char* dir, size_t size, Test test) {
    dir[0] = '\0';
    dirlist::each(kMusicDir, [&](const dirlist::Entry& e) {
        if (!dirlist::valid(e, true))
            return true;
        snprintf(dir, size, "%s/%.*s", kMusicDir, int(mrm::kMaxPlaylistLen), e.name);
        if (test(dir))
            return false;
        dir[0] = '\0';
        return true;
    });
}

// Playlist vazia (queda de energia logo depois do mkdir de um envio) ocuparia vaga para sempre.
void Portal::sweepEmpty() {
    char dir[kPathMax];
    for (;;) {
        findPlaylist(dir, sizeof(dir), dirlist::isEmpty);
        if (!dir[0] || !fs_.rmdir(dir))
            return;
    }
}

// Troca interrompida (queda entre os renames) deixa ".NN - titulo.mp3": acha o nome oculto sem destino.
bool Portal::findSwap(const char* dir, char* from, char* to, size_t size) {
    bool found = false;
    dirlist::each(dir, [&](const dirlist::Entry& e) {
        if (e.dir || e.name[0] != '.' || !mrm::validTrackName(e.name + 1))
            return true;
        snprintf(to, size, "%s/%.*s", dir, int(mrm::kMaxTrackLen), e.name + 1);
        if (dirlist::exists(to))
            return true;
        snprintf(from, size, "%s/.%.*s", dir, int(mrm::kMaxTrackLen), e.name + 1);
        return !(found = true);
    });
    return found;
}

// Devolve o nome sem o ponto, uma troca pendente por chamada; repete ate nao sobrar nenhuma.
void Portal::recoverSwap() {
    char dir[kPathMax], from[kPathMax], to[kPathMax];
    for (;;) {
        findPlaylist(dir, sizeof(dir), [&](const char* d) { return findSwap(d, from, to, sizeof(from)); });
        if (!dir[0] || !fs_.rename(from, to))
            return;
    }
}
