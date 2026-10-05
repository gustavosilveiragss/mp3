#pragma once

#include <cstddef>

// Códigos HTTP e textos de erro do contrato com portal.html (chave "e" e "last" do JSON).
namespace portalhttp {

enum HttpStatus : int {
    kOk = 200,
    kBadRequest = 400,
    kForbidden = 403,
    kNotFound = 404,
    kMethodNotAllowed = 405,
    kConflict = 409,
    kPayloadTooLarge = 413,
    kUnsupportedMedia = 415,
    kServerError = 500,
    kUnavailable = 503,
    kInsufficientStorage = 507,
};

inline constexpr char kErrName[] = "name";
inline constexpr char kErrIo[] = "io";
inline constexpr char kErrLimit[] = "limit";
inline constexpr char kErrExists[] = "exists";
inline constexpr char kErrSpace[] = "space";
inline constexpr char kErrNotMp3[] = "notmp3";
inline constexpr char kErrAborted[] = "aborted";
inline constexpr char kErrNumber[] = "num";
inline constexpr char kLastOk[] = "ok";
inline constexpr char kHdrType[] = "Content-Type";
inline constexpr char kJsonType[] = "application/json";

inline constexpr size_t kLastMax = sizeof(kErrAborted); // "last" guarda o maior dos textos acima
static_assert(sizeof(kErrNotMp3) <= kLastMax && sizeof(kErrExists) <= kLastMax, "last_ precisa caber todo texto");

} // namespace portalhttp
