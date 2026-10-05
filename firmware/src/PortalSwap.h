#pragma once

#include <PathRules.h>

#include <cstddef>
#include <cstdio>
#include <cstring>

// Troca dos prefixos "NN - " de duas faixas. Funções puras.
namespace trackswap {

constexpr char kTrackPrefixSep[] = " - ";
constexpr size_t kTrackPrefixSepLen = sizeof(kTrackPrefixSep) - 1;

struct SwapNames {
    const char* playlist;
    const char* first;
    const char* second;
    char newFirst[mrm::kMaxTrackLen + 1];
    char newSecond[mrm::kMaxTrackLen + 1];
};

// Tamanho de "NN - " no começo do nome (um ou mais dígitos e o separador), ou 0.
inline size_t numPrefix(const char* name) {
    size_t digits = 0;
    while (name[digits] >= '0' && name[digits] <= '9')
        ++digits;
    const bool hasPrefix = digits > 0 && strncmp(name + digits, kTrackPrefixSep, kTrackPrefixSepLen) == 0;
    return hasPrefix ? digits + kTrackPrefixSepLen : 0;
}

// Sem prefixo numérico nos dois, ou com prefixos iguais, não há o que trocar.
inline bool prefixesCanSwap(const char* first, const char* second) {
    const size_t firstPrefix = numPrefix(first);
    const size_t secondPrefix = numPrefix(second);
    if (firstPrefix == 0 || secondPrefix == 0)
        return false;
    return firstPrefix != secondPrefix || strncmp(first, second, firstPrefix) != 0;
}

// "01 - x" e "02 - y" viram newFirst "02 - x" e newSecond "01 - y". False se truncou ou o nome ficou inválido.
inline bool buildSwappedNames(SwapNames& names) {
    const size_t firstPrefix = numPrefix(names.first);
    const size_t secondPrefix = numPrefix(names.second);

    const int firstLen = snprintf(names.newFirst, sizeof(names.newFirst), "%.*s%s", int(secondPrefix),
                                  names.second, names.first + firstPrefix);
    const int secondLen = snprintf(names.newSecond, sizeof(names.newSecond), "%.*s%s", int(firstPrefix),
                                   names.first, names.second + secondPrefix);

    return firstLen >= 0 && size_t(firstLen) < sizeof(names.newFirst) && secondLen >= 0 &&
           size_t(secondLen) < sizeof(names.newSecond) && mrm::validTrackName(names.newFirst) &&
           mrm::validTrackName(names.newSecond);
}

} // namespace trackswap
