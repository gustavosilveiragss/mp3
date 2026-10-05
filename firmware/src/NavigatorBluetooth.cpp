#include "Navigator.h"

void Navigator::selectBluetooth() {
    switch (cursor_.pos) {
    case menu::kBluetoothToggle:
        toggleBluetooth();
        break;

    case menu::kBluetoothPick:
        settings_.setBluetooth(true); // escolher dispositivo liga o Bluetooth
        btAt_ = stamp();
        openPicker();
        break;

    case menu::kBluetoothForget: {
        const bool radioOn = settings_.bluetooth();

        audio_.setEnabled(true); // os pareamentos só se apagam com o rádio ligado
        audio_.forgetAll();
        audio_.setEnabled(radioOn);

        settings_.setDevice("");
        break;
    }

    default:
        back();
        break;
    }
}

void Navigator::selectDevice() {
    const uint8_t pos = cursor_.pos;

    if (pos == menu::kPickerRescan) {
        audio_.scan();
    } else if (pos == menu::kPickerBack) {
        back();
    } else if (pos - menu::kPickerFirstDevice < audio_.deviceCount()) {
        char name[sizeof(Audio::Device::name)];
        strlcpy(name, audio_.devices()[pos - menu::kPickerFirstDevice].name, sizeof(name)); // a lista some ao conectar

        pickedFromList_ = true;
        connectTo(name);
    }
}

// Volta ao último fone. Sem ele guardado, quem está no menu Bluetooth escolhe um e o resto vai para a Home.
void Navigator::reconnect() {
    pickedFromList_ = false;

    if (settings_.device()[0])
        connectTo(settings_.device(), settings_.deviceBda());
    else if (screen_ == Screen::Bluetooth)
        openPicker();
    else
        enter(Screen::Home);
}

// Quem escolheu da lista volta a ela. A reconexão automática cai na Home, nunca sozinha numa config.
void Navigator::connectFailed() {
    pickedFromList_ ? openPicker() : enter(Screen::Home);
}

void Navigator::openPicker() {
    audio_.setEnabled(true);
    audio_.scan();
    enter(Screen::Picker);
}

void Navigator::connectTo(const char* name, const uint8_t* bda) {
    audio_.setEnabled(true);
    audio_.connect(name, bda);
    enter(Screen::Connecting);
}

// Na Home, sem dispositivo e com Bluetooth ligado, tenta o último de tempos em tempos sem trocar de tela.
void Navigator::retryConnection() {
    if (!settings_.bluetooth() || !settings_.device()[0])
        return;
    const uint32_t now = millis();

    if (audio_.link() != Audio::Link::Idle) {
        retryAt_ = now; // o intervalo conta a partir do fim da tentativa, não do início
        return;
    }

    if (now - retryAt_ < kRetryMs)
        return;

    retryAt_ = now;
    audio_.connect(settings_.device(), settings_.deviceBda());
}

void Navigator::toggleBluetooth() {
    btAt_ = stamp();

    if (settings_.bluetooth()) {
        settings_.setBluetooth(false);
        resumeAfterWifi_ = false;

        player_.pause();
        audio_.setEnabled(false);
    } else {
        settings_.setBluetooth(true);
        audio_.setEnabled(true);
        reconnect();
    }
}
