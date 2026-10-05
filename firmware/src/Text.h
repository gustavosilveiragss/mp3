#pragma once

#include <marmota.h>

// Catálogo de textos da tela e do portal: cada constante tem os dois idiomas e quem escolhe é a mrm::Locale.
namespace Txt {
using mrm::HintDown;
using mrm::HintOk;

inline constexpr mrm::Text HintBack{"volta", "back"};

inline constexpr mrm::Text HintOff{"desliga", "off"};
inline constexpr mrm::Text HintCancel{"cancela", "cancel"};

inline constexpr mrm::Text Back{"Voltar", "Back"};
inline constexpr mrm::Text On{"ligado", "on"};
inline constexpr mrm::Text Off{"desligado", "off"};
inline constexpr mrm::Text Settings{"Configurações", "Settings"};
inline constexpr mrm::Text Language{"Idioma", "Language"};
inline constexpr mrm::Text Exit{"Sair", "Exit"};

inline constexpr mrm::Text NoMusic{"biblioteca vazia", "empty library"};
inline constexpr mrm::Text Tracks{"faixas", "tracks"};
inline constexpr mrm::Text Library{"Biblioteca", "Library"};
inline constexpr mrm::Text Games{"Jogos", "Games"};
inline constexpr mrm::Text HoldSettings{"A+B segura: config", "hold A+B: settings"};
inline constexpr mrm::Text HoldSend{"A+B segura: enviar", "hold A+B: send music"};
inline constexpr mrm::Text HoldConnect{"A+B segura: conectar", "hold A+B: connect"};

inline constexpr mrm::Text ExtraFeatures{"Funcionalidades extras", "Extra features"};
inline constexpr mrm::Text ScreenOff{"Tela apaga sozinha", "Auto screen off"};
inline constexpr mrm::Text ButtonLock{"Bloqueio de botões", "Button lock"};
inline constexpr mrm::Text ScreenOffHelp{"apaga a tela após 10 s", "screen off after 10 s"};
inline constexpr mrm::Text ButtonLockHelp{"A+B 3x trava e destrava", "A+B 3x locks and unlocks"};
inline constexpr mrm::Text LockedToast{"Bloqueado", "Locked"};
inline constexpr mrm::Text UnlockedToast{"Desbloqueado", "Unlocked"};
inline constexpr mrm::Text UnlockHint{"A+B 3x desbloqueia", "A+B 3x unlocks"};

inline constexpr mrm::Text Bluetooth{"Bluetooth", "Bluetooth"};
inline constexpr mrm::Text PickDevice{"Escolher dispositivo", "Choose device"};
inline constexpr mrm::Text ForgetDevice{"Esquecer dispositivos", "Forget all devices"};
inline constexpr mrm::Text ForgottenToast{"Todos esquecidos", "All forgotten"};
inline constexpr mrm::Text NoDevice{"nenhum", "none"};
inline constexpr mrm::Text Devices{"Dispositivos Bluetooth", "Bluetooth devices"};
inline constexpr mrm::Text ScanAgain{"buscar de novo", "scan again"};
inline constexpr mrm::Text Scanning{"buscando", "scanning"};
inline constexpr mrm::Text Connecting{"conectando", "connecting"};
inline constexpr mrm::Text ConnectingTo{"conectando a", "connecting to"};
inline constexpr mrm::Text NoDeviceTitle{"sem dispositivo", "no device"};
inline constexpr mrm::Text BtOffTitle{"desconectado", "disconnected"};
inline constexpr mrm::Text BtOffBody{"Bluetooth desligado", "Bluetooth is off"};

inline constexpr mrm::Text Wifi{"Wi-Fi", "Wi-Fi"};
inline constexpr mrm::Text UploadMusic{"Enviar músicas", "Send music"};
inline constexpr mrm::Text AutoOpens{"a página abre sozinha", "the page opens by itself"};
inline constexpr mrm::Text OrGoTo{"ou vá em 192.168.4.1", "or go to 192.168.4.1"};
inline constexpr mrm::Text Receiving{"recebendo", "receiving"};
inline constexpr mrm::Text Connected{"conectado", "connected"};
inline constexpr mrm::Text PickOnPhone{"escolha no celular", "pick on your phone"};
inline constexpr mrm::Text Received{"recebido", "received"};
inline constexpr mrm::Text SendMore{"pode enviar mais", "you can send more"};
inline constexpr mrm::Text Total{"total", "total"};

inline constexpr mrm::Text StorageError{"mídia com erro", "storage error"};
inline constexpr mrm::Text RestartOr{"reinicie ou", "restart or"};
inline constexpr mrm::Text ReplaceCard{"troque o cartão", "replace the card"};
inline constexpr mrm::Text Storage{"Armazenamento", "Storage"};
inline constexpr mrm::Text SdCard{"Cartão SD", "SD card"};
inline constexpr mrm::Text NoSd{"Sem cartão SD", "No SD card"};
inline constexpr mrm::Text Internal{"Memória interna", "Internal memory"};
inline constexpr mrm::Text FreeOf{"livres de", "free of"};
} // namespace Txt
