# Circuito do mp3

Player de audio Bluetooth no ESP32 classico (WROOM-32, DevKit 38 pinos) + microSD + OLED +
2 botoes (gestos, ver CONTROLES.md) + 18650 com carga USB-C. Sem DAC, amp nem jack. ESP32 classico (nao C3) por
causa do Bluetooth A2DP. Esquema visual em [`perfboard/`](perfboard/).

## Componentes

| Peca | Componente | Valor / modelo | Nota |
|---|---|---|---|
| ESP32 DevKit | placa principal | WROOM-32, 38 pinos | A2DP |
| microSD | modulo cartao | SPI (VSPI) | biblioteca de musicas |
| OLED | display | SSD1306 0.96" I2C | navegacao |
| 5x botao | tactil 6x6 momentaneo | - | play, next, prev, vol+, vol-; como no pin |
| LED | LED 5mm + R 330 | - | status |
| MT3608 | boost | ajustado 5.0V | 5V pro VIN do ESP |
| TP4056 | carregador USB-C | com protecao DW01 | carrega desligado |
| Bateria | LiPo 18650 | 3.7V | protegida |
| Chave | chave deslizante | SS-12D00 | liga/desliga; como no pin |

## Ligacoes

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
| A | GPIO33 |
| B | GPIO32 |

Acoes por gesto em `CONTROLES.md`.

### LED, bateria, energia

| De | Para |
|---|---|
| GPIO16 | LED anodo via 330R, catodo no GND |
| divisor 2x100k (batt+ chaveado) | GPIO35 (ADC1, input-only) |
| Bateria 18650 + / - | TP4056 B+ / B- |
| TP4056 OUT+ | chave -> MT3608 VIN+ |
| MT3608 VOUT+ (5.0V) | ESP32 5V/VIN |
| TP4056 OUT- | MT3608 GND -> ESP GND (star ground) |

Bulk 470-1000uF no 5V (anti-brownout) + 100nF no 5V e no 3V3.  GPIO35 e input-only, ok pra ADC de bateria.

## Prototipo em breadboard (fazer antes da perfboard)

Sem prazo, minimo de solda, mesma pinagem das tabelas acima. ESP32 + microSD + OLED + 2 botoes
+ LED + divisor, com jumpers dupont e USB do ESP como alimentacao. Bateria, TP4056, chave e
MT3608 testados a parte (MT3608 em 5,0 V medido com multimetro antes de ligar o ESP). O DevKit de
38 pinos costuma nao deixar furo livre na breadboard: usar duas breadboards ou placa de expansao.
Perfboard e case sao as ultimas etapas: antes disso, tudo validado em breadboard, incluindo
temperatura (bateria, TP4056, MT3608), e medidas em `medidas/index.html`. Checklist e ordem em
`PLANO-3-DIAS.md`, lista de compras do prototipo em `COMPRAS.md`.

## Case

Mesmo principio de fixacao descrito em `PLANO-3-DIAS.md`: a placa nao e parafusada nem presa por
espuma. Ela desliza em trilhos impressos nas paredes longas (ranhura ~1,75 mm), com batente na
ponta do USB-C e na tampa, e fica travada por cola quente nas pontas e no soquete do ESP.
Botoes 6x6 soldados na placa com tampa impressa na face de cima, chave SS-12D00 em bolsao na
parede da ponta. TP4056 USB-C soldado na borda da placa, porta no recorte com batente atras.
Bateria em bolso proprio, longe do MT3608 e do TP4056, com fendas de ventilacao em volta
(bateria e TP4056 esquentam, ver Termica em `PLANO-3-DIAS.md`). Material PETG. OLED atras de janela, microSD por fresta lateral.
Ground em estrela no no do MT3608. ESP32 soquetado (barra femea) pra reflashar.
