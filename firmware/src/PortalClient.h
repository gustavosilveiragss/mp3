#pragma once

#include <WebServer.h>

#include <type_traits>
#include <utility>

// Incluir so em um .cpp: a instanciacao explicita abaixo tem ligacao interna.
namespace {

// O WebServer le o corpo do raw com o timeout do seu _currentClient (1 s) e client() devolve uma
// copia, entao setTimeout/stop por ali nao chegam ao socket. O membro e protected: o truque do
// ponteiro de membro em instanciacao explicita acessa sem alterar a lib.
// Tipo do cliente do WebServer: WiFiClient no Arduino 2, NetworkClient no 3.
using HttpClient = std::decay_t<decltype(std::declval<WebServer&>().client())>;

struct ClientTag {
    using Type = HttpClient WebServer::*;
    friend Type clientMember(ClientTag);
};
template <typename Tag, typename Tag::Type Member>
struct Reach {
    friend typename Tag::Type clientMember(Tag) { return Member; }
};
template struct Reach<ClientTag, &WebServer::_currentClient>;

HttpClient& clientOf(WebServer& server) {
    return server.*clientMember(ClientTag{});
}

// Espera por dados no corpo: WiFiClient (Arduino 2) recebe segundos, o Stream do Arduino 3 recebe ms.
// Passar segundos ao Stream dava 5 ms e o WebServer abortava o envio na primeira pausa.
void setRecvTimeout(HttpClient& client, uint32_t seconds) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    client.setTimeout(seconds * 1000);
#else
    client.setTimeout(seconds);
#endif
}

} // namespace
