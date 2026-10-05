#pragma once

#include <cstddef>
#include <string>

// So o que JsonOut usa: captura o corpo enviado em pedacos.
class WebServer {
public:
    std::string body;
    void sendContent(const char* data, size_t len) { body.append(data, len); }
};
