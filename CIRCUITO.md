# Circuito do mp3

Player de audio no ESP32 classico (WROOM-32, DevKit 38 pinos) + DAC I2S PCM5102A + amp de
fone + microSD + OLED + 5 botoes + 18650 com carga USB-C. ESP32 classico (nao C3) por
causa do Bluetooth A2DP. Esquema visual em [`perfboard/`](perfboard/).

## Componentes

| Peca | Componente | Valor / modelo | Nota |
|---|---|---|---|
| ESP32 DevKit | placa principal | WROOM-32, 38 pinos | A2DP + I2S |
| DAC | DAC I2S | PCM5102A (GY-PCM5102A) | saida line-level |
| Amp de fone | amp estereo ground-referenced | PAM8908 / TPA6132 | driva 32ohm, NAO usar amp de alto-falante |
| microSD | modulo cartao | SPI (VSPI) | biblioteca de musicas |
| OLED | display | SSD1306 0.96" I2C | navegacao |
| 5x botao | tactil momentario | - | play, next, prev, vol+, vol- |
| LED | LED 5mm + R 330 | - | status |
| Jack P2 | 3.5mm estereo femea | painel | saida de fone |
| MT3608 | boost | ajustado 5.0V | rail limpo pro DAC/amp |
| TP4056 | carregador USB-C | com protecao DW01 | carrega desligado |
| Bateria | LiPo 18650 | 3.7V | protegida |
| Chave | chave deslizante | SS-12D00 | liga/desliga |

## Ligacoes

### DAC I2S (PCM5102A)

| PCM5102A | ESP |
|---|---|
| BCK | GPIO26 |
| LCK (LRCK) | GPIO25 |
| DIN | GPIO27 |
| SCK | GND (DAC gera clock interno do BCK) |
| FMT | GND (formato I2S) |
| XSMT | 3V3 (un-mute, LOW = mudo) |
| FLT | GND (latencia normal) |
| DEMP | GND (de-enfase off) |
| VIN | 5V (LDO da placa faz 3.3V limpo) |
| GND | GND |

### Cadeia de audio (DAC -> amp -> jack)

| De | Para |
|---|---|
| PCM5102A LOUT / ROUT | amp LIN / RIN |
| PCM5102A GND | amp GND |
| 5V | amp VCC |
| amp EN | VCC (habilitado) |
| amp LOUT / ROUT | jack P2 tip / ring |
| amp GND | jack P2 sleeve |

### microSD (VSPI)

| microSD | ESP |
|---|---|
| SCK | GPIO18 |
| MOSI | GPIO23 |
| MISO | GPIO19 |
| CS | GPIO17 |
| VCC | 3V3 (modulo mini SPI e 3.3V nativo, NAO alimentar em 5V) |
| GND | GND |

### OLED (I2C)

| OLED | ESP |
|---|---|
| SDA | GPIO21 |
| SCL | GPIO22 |
| VCC | 3V3 |
| GND | GND |

### Botoes (INPUT_PULLUP, outro lado no GND)

| Botao | GPIO |
|---|---|
| play/pause | GPIO32 |
| next | GPIO33 |
| prev | GPIO13 |
| vol+ | GPIO14 |
| vol- | GPIO4 |

### LED, bateria, energia

| De | Para |
|---|---|
| GPIO16 | LED anodo via 330R, catodo no GND |
| divisor 2x100k (batt+ chaveado) | GPIO35 (ADC1, input-only) |
| Bateria 18650 + / - | TP4056 B+ / B- |
| TP4056 OUT+ | chave -> MT3608 VIN+ |
| MT3608 VOUT+ (5.0V) | ESP32 5V/VIN |
| TP4056 OUT- | MT3608 GND -> ESP GND (star ground) |

Bulk 470-1000uF no 5V (anti-brownout) + 100nF no 5V e no 3V3. 10uF+100nF no VIN do DAC
(ferrite opcional em serie pra rail limpo). GPIO35 e input-only, ok pra ADC de bateria.

## Case

Aparafusavel, abre pra trocar cartao e bateria. Jack P2 e 5 botoes panel-mount na frente,
cada fio com service loop + hot glue. OLED atras de janela. Slot do microSD acessivel por
fresta lateral. TP4056 USB-C e chave em recortes laterais. Amp e DAC perto um do outro,
ground em estrela no no do MT3608. ESP32 soquetado (barra femea) pra reflashar.
