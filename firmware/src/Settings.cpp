#include "Settings.h"

#include <Preferences.h>

namespace {
constexpr uint32_t kSaveDelayMs = 2000;
constexpr char kNvs[] = "mp3";
constexpr char kHex[] = "0123456789abcdef";

void bdaToHex(const uint8_t* bda, char* out) {
    for (uint8_t i = 0; i < 6; ++i) {
        out[2 * i] = kHex[bda[i] >> 4];
        out[2 * i + 1] = kHex[bda[i] & 15];
    }
    out[12] = '\0';
}

bool hexToBda(const char* hex, uint8_t* bda) {
    if (strlen(hex) != 12)
        return false;
    for (uint8_t i = 0; i < 12; ++i) {
        const char* p = strchr(kHex, hex[i]);
        if (!p || !*p)
            return false;
        const uint8_t v = p - kHex;
        bda[i / 2] = i % 2 ? (bda[i / 2] & 0xF0) | v : v << 4;
    }
    return true;
}
} // namespace

void Settings::begin() {
    config_.load();
    const JsonDocument& doc = config_.doc();
    volume_ = constrain(doc["volume"] | int(volume_), 0, kMaxVolume);
    track_ = constrain(doc["track"] | 0, 0, UINT16_MAX);
    const char* playlist = doc["playlist"] | "";
    if (mrm::validPlaylistName(playlist))
        strlcpy(playlist_, playlist, sizeof(playlist_));
    const char* trackName = doc["trackName"] | "";
    if (mrm::validTrackName(trackName))
        strlcpy(trackName_, trackName, sizeof(trackName_));
    mrm::Locale::fromCode(doc["lang"] | "", lang_);
    for (uint8_t i = 0; i < kGames; ++i)
        best_[i] = constrain(doc["best"][i] | 0, 0, UINT16_MAX);
    seen_ = constrain(doc["seen"] | 0, 0, 255);
    screenOff_ = doc["screenOff"] | false;
    lockEnabled_ = doc["lock"] | false;
    bluetooth_ = doc["bluetooth"] | true;
    strlcpy(device_, doc["device"] | "", sizeof(device_));
    hasBda_ = hexToBda(doc["bda"] | "", bda_);
    loadHot();
}

// A NVS vence o config.json, que pode ter ficado para tras (gravacao segurada e reset pelo EN).
void Settings::loadHot() {
    Preferences p;
    if (!p.begin(kNvs, true))
        return;
    volume_ = min<uint8_t>(p.getUChar("vol", volume_), kMaxVolume);
    char playlist[sizeof(playlist_)];
    if (p.getString("pl", playlist, sizeof(playlist)) && mrm::validPlaylistName(playlist)) {
        strlcpy(playlist_, playlist, sizeof(playlist_));
        track_ = p.getUShort("track", track_);
        char trackName[sizeof(trackName_)];
        if (p.getString("tn", trackName, sizeof(trackName)) && mrm::validTrackName(trackName))
            strlcpy(trackName_, trackName, sizeof(trackName_));
    }
    if (p.isKey("dev"))
        p.getString("dev", device_, sizeof(device_));
    if (p.isKey("bda"))
        hasBda_ = p.getBytes("bda", bda_, sizeof(bda_)) == sizeof(bda_);
    p.end();
}

void Settings::saveHot() {
    Preferences p;
    if (!p.begin(kNvs))
        return;
    // So grava o que mudou: cada put novo gasta uma entrada da pagina, mesmo com o valor igual.
    if (p.getUChar("vol", 255) != volume_)
        p.putUChar("vol", volume_);
    char old[sizeof(device_)];
    if (!p.getString("pl", old, sizeof(old)) || strcmp(old, playlist_) != 0)
        p.putString("pl", playlist_);
    if (!p.isKey("track") || p.getUShort("track") != track_)
        p.putUShort("track", track_);
    char oldTrack[sizeof(trackName_)];
    if (!p.getString("tn", oldTrack, sizeof(oldTrack)) || strcmp(oldTrack, trackName_) != 0)
        p.putString("tn", trackName_);
    if (!p.getString("dev", old, sizeof(old)) || strcmp(old, device_) != 0)
        p.putString("dev", device_);
    uint8_t bda[6];
    if (hasBda_ && (p.getBytes("bda", bda, sizeof(bda)) != sizeof(bda) || memcmp(bda, bda_, sizeof(bda)) != 0))
        p.putBytes("bda", bda_, sizeof(bda_));
    else if (!hasBda_ && p.isKey("bda"))
        p.remove("bda");
    p.end();
}

void Settings::update() {
    if (millis() - changedAt_ < kSaveDelayMs)
        return;
    if (hot_) {
        hot_ = false;
        saveHot();
    }
    if (!dirty_ || defer_)
        return;
    dirty_ = false;
    JsonDocument& doc = config_.doc();
    doc["volume"] = volume_;
    doc["playlist"] = playlist_;
    doc["track"] = track_;
    doc["trackName"] = trackName_;
    doc["lang"] = mrm::Locale::code(lang_);
    JsonArray best = doc["best"].to<JsonArray>();
    for (uint8_t i = 0; i < kGames; ++i)
        best.add(best_[i]);
    doc["seen"] = seen_;
    doc["screenOff"] = screenOff_;
    doc["lock"] = lockEnabled_;
    doc["bluetooth"] = bluetooth_;
    doc["device"] = device_;
    char hex[13];
    bdaToHex(bda_, hex);
    doc["bda"] = hasBda_ ? hex : "";
    config_.save();
#if MP3_AUDIO_DEBUG
    Serial.printf("aud %lu config salvo\n", millis());
#endif
}

void Settings::setVolume(uint8_t volume) {
    volume = min<uint8_t>(volume, kMaxVolume);
    if (volume == volume_)
        return;
    volume_ = volume;
    touch(true);
}

void Settings::setPosition(const char* playlist, uint16_t track, const char* trackName) {
    if (!mrm::validPlaylistName(playlist) || (track == track_ && strcmp(playlist, playlist_) == 0 && strcmp(trackName, trackName_) == 0))
        return;
    strlcpy(playlist_, playlist, sizeof(playlist_));
    strlcpy(trackName_, trackName, sizeof(trackName_));
    track_ = track;
    touch(true);
}

void Settings::setLang(mrm::Lang lang) {
    if (lang == lang_)
        return;
    lang_ = lang;
    touch();
}

bool Settings::setBest(uint8_t game, uint16_t score) {
    if (game >= kGames || score <= best_[game])
        return false;
    best_[game] = score;
    touch();
    return true;
}

void Settings::setTutorialSeen(uint8_t game) {
    if (game >= kGames || tutorialSeen(game))
        return;
    seen_ |= 1u << game;
    touch();
}

void Settings::setScreenOff(bool on) {
    if (on == screenOff_)
        return;
    screenOff_ = on;
    touch();
}

void Settings::setLockEnabled(bool on) {
    if (on == lockEnabled_)
        return;
    lockEnabled_ = on;
    touch();
}

void Settings::setBluetooth(bool on) {
    if (on == bluetooth_)
        return;
    bluetooth_ = on;
    touch();
}

void Settings::setDevice(const char* name, const uint8_t* bda) {
    const bool sameBda = bda ? hasBda_ && memcmp(bda, bda_, sizeof(bda_)) == 0 : !hasBda_;
    if (strcmp(name, device_) == 0 && sameBda)
        return;
    strlcpy(device_, name, sizeof(device_));
    hasBda_ = bda != nullptr;
    if (bda)
        memcpy(bda_, bda, sizeof(bda_));
    touch(true);
}

void Settings::touch(bool hot) {
    dirty_ = true;
    hot_ = hot_ || hot;
    changedAt_ = millis();
}
