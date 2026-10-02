# Controles: 2 botões, gestos

Decisão (2026-10-01): o player tem 2 botões, A e B, no lugar de 5. Todas as ações saem de gestos: toque, toque duplo, segurar e os dois juntos. Implementação em `firmware/src/Keypad.*`, pinos em `pins.h`.

## Por que 2 e não 1

- Volume precisa de duas direções com repetição ao segurar. Com 1 botão a direção teria de vir de contagem de toques ou de alternância, o que obriga a olhar o OLED para saber o sentido.
- Com 2 botões cada tecla tem um sentido fixo: A é "baixo/pausa", B é "alto/próxima". O corpo aprende em minutos, sem olhar.
- Um terceiro botão não compra nada: o que sobra (playlist, Wi-Fi) é raro e cabe em combinação.

## Mapa de gestos

| Gesto | Ação | Quando dispara |
|---|---|---|
| A toque | play/pause | 250 ms depois de soltar (espera um possível toque duplo) |
| A toque duplo | faixa anterior | 250 ms depois do segundo toque |
| A segurar | volume − | aos 400 ms, repete a cada 150 ms |
| B toque | próxima faixa | ao soltar, sem espera |
| B segurar | volume + | aos 400 ms, repete a cada 150 ms |
| A+B toque | próxima playlist | ao soltar os dois (ambos soltos em menos de 1,5 s) |
| A+B segurar 1,5 s | abre a Config (Wi-Fi, Bluetooth, idioma) | aos 1,5 s, ainda segurando |

Por que assim:
- Ações frequentes ficam em toque único. A pausa tem 250 ms de atraso, aceitável; a próxima faixa não tem atraso, e pular duas faixas é dois toques em B (B não tem toque duplo, justamente para isso).
- Ações raras e que interrompem a música (trocar de playlist, config) exigem os dois botões, o que quase não acontece por acidente.
- A é o pino de acordar do `lightSleep` (ver `PLANO-3-DIAS.md`): pausa longa dorme e A acorda.

## Regras do decodificador

1. Debounce de 30 ms por tecla.
2. Combinação A+B: a segunda tecla chega até 200 ms depois da primeira, com nenhuma das duas em "segurar". A janela (`kChordWindowMs`) é estimativa: medir na bancada com o caso de rolar o dedo de A para B. Toque pendente de A é descartado ao entrar na combinação (a combinação vence).
3. Durante a combinação nada mais dispara. `OpenMenu` exige as duas seguradas sem interrupção por 1,5 s (soltar uma e reapertar cancela). `NextPlaylist` sai quando as duas são soltas com até 300 ms de diferença e o Wi-Fi não disparou. Uma tecla solta cedo e a outra segurada além disso não dá nada.
4. Fora da combinação as teclas são independentes. Segurar A e tocar B dá Next; segurar as duas por separado (B depois de 200 ms) alterna os dois volumes. Não é caso de uso e é risco aceito: nada impede uma tecla de agir enquanto a outra segura.
5. Toque de B nunca cancela o toque pendente de A (e vice-versa não existe: B não tem toque pendente).
6. Segurar além de 400 ms no meio de uma rajada de A descarta a rajada e vira volume.
7. Eventos saem por fila de 4; o `loop()` consome um por volta. `repeatAt` é recalculado a partir de `now`, então um loop lento não gera rajada atrasada.

## Telas e menus (`Navigator`)

`Keypad` só decodifica gestos. O `Navigator` decide a tela e o que cada gesto significa nela. `Player` cuida da música (faixa, volume, playlist) e só recebe gestos na tela Home.

| Tela | Como chega | A | B | A+B |
|---|---|---|---|---|
| Intro (3,8 s) | boot | nada | nada | nada |
| Conectando | boot com fone guardado (até 15 s), ou escolha na lista | nada | cancela (volta à Home) | cancela (volta à Home) |
| Home (player) | sempre depois da intro: conectou, falhou, cancelou ou não há fone guardado | gestos da tabela acima; sem fone, play/próxima/anterior só mostram o aviso "sem dispositivo" | idem | segurar 1,5 s abre a Config |
| Config | A+B longo na Home (nunca abre sozinha) | desce (segurar repete) | entra | Sair |
| Bluetooth | Config | desce | liga/desliga, escolher dispositivo, voltar | volta |
| Dispositivos | Bluetooth, Escolher dispositivo | desce | conecta, busca de novo, volta | volta |
| Biblioteca | Config | desce | escolhe a playlist e volta para a Home (Voltar fica no topo; cada linha mostra o nº de faixas à direita) | volta |
| Wi-Fi | Config | desce | liga o portal; ligado, B e A+B desligam (também durante um envio: o arquivo em curso é descartado) | volta (se ligado, desliga) |
| Funcionalidades extras | Config | desce | liga ou desliga a opção em foco | volta |
| Armazenamento | Config (entre Funcionalidades extras e Jogos) | nada | volta | volta |
| Jogos | Config | desce | abre o jogo | volta |
| Jogo: tutorial (2 páginas: controles e regras) | primeira vez de cada jogo, ou Como jogar na pausa | página anterior | próxima página; na última, jogar (ou voltar à pausa) | sai do jogo (ou volta à pausa) |
| Jogo: partida | depois da contagem 3-2-1 | gesto do jogo, segurado | gesto do jogo, segurado | as duas segurando 0,7 s pausam |
| Jogo: pausa e fim | pausa ou fim da partida | desce | confirma (Continuar, Como jogar, Sair, Jogar de novo) | continua (pausa) ou sai (fim) |
| Idioma | Config | desce | escolhe e volta | volta |

Nos menus o toque de A sai na hora (sem esperar o toque duplo) e a barra de topo mostra as instruções no lugar do status. O Home mostra o status: Bluetooth, Wi-Fi e bateria (quando houver).

Armazenamento: tela só de leitura, sem status. Com cartão SD (build `MP3_SD=1` e cartão montado) mostra "Cartão SD", livre e total e a barra de uso; sem ele, "Sem cartão SD" e a "Memória interna" (LittleFS) com livre, total, barra e quantidade de faixas (se couber). Tamanhos em B/KB/MB/GB com uma casa (vírgula em pt-BR, ponto em en); se "livres de" não couber, usa "livre / total". O FS é lido só ao entrar na tela e ao desligar o portal.

Regras:
- Sair sempre funciona. Sem dispositivo conectado a Home mostra o status na barra (Bluetooth piscando) e tenta o último dispositivo a cada 10 s, sem trocar de tela. Tocar (play, próxima, anterior) sem dispositivo não troca de tela: mostra o aviso "sem dispositivo" por um instante.
- No boot, depois da intro: com dispositivo guardado e Bluetooth ligado tenta conectar (15 s, cancela com B ou A+B); depois vai sempre para a Home, conectando, falhando ou sem dispositivo guardado. Nenhuma falha ou ausência abre Config, Bluetooth ou lista de dispositivos sozinha.
- Home sem fone (Bluetooth ligado): mensagem "sem dispositivo" e dica "A+B segura: conectar", Bluetooth piscando na barra. Biblioteca vazia: "biblioteca vazia" e "A+B segura: add músicas" (leva à Config, Wi-Fi). Sem os dois: título "biblioteca vazia", "sem dispositivo" e a dica alternando a cada 3 s entre enviar e conectar.
- Com o Bluetooth desligado a Home só mostra a mensagem e só a Config responde.
- Ligar o Wi-Fi desliga o Bluetooth e pausa a música; desligar o Wi-Fi (B, A+B, "terminar" da página ou ociosidade) religa o Bluetooth, se estava ligado, e reconecta ao último dispositivo em segundo plano (Bluetooth piscando na Home; sem o fone, a retentativa de 10 s continua). Se tocava antes do Wi-Fi, volta a tocar quando o fone reconectar (até 60 s); se não tocava, continua parado. Se o portal mexeu no cartão, a biblioteca é lida de novo ao desligar (volta à playlist guardada, ou à primeira se ela sumiu).
- Portal ligado: rede aberta "marmota mp3 XXXX", um celular por vez, a página abre sozinha (ou 192.168.4.1). O OLED mostra esperando, conectado, recebendo (3/12, barra do arquivo e da fila) e recebido. O portal desliga sozinho após 10 min sem celular conectado nem requisição, ou pelo botão "terminar" da página. A tela não apaga e o envio não trava os botões.
- Nomenclatura: a Biblioteca (tudo em `/music`) contém playlists (uma pasta cada), e as playlists contêm faixas. A Config tem a linha "Biblioteca" (en "Library"), que lista as playlists.
- Portal, fluxo de envio: a biblioteca é a seção principal. Cada playlist é um cartão (nome, faixas, tamanho) com "+ adicionar músicas", que abre o seletor já com aquele destino; tocar no cartão expande as faixas. No topo, "+ nova playlist" (nome com validação ao vivo: livre, já existe, caractere proibido, mais de 24 bytes, limite de 16) e "enviar pasta" (a pasta vira playlist; se já existe uma com o mesmo nome, sem diferenciar caixa como o FAT, a fila pergunta "adicionar à existente" (padrão) ou "criar nova \"x 2\""). A fila sempre mostra a linha de destino (existente com nº de faixas, nova, ou "escolha uma playlist" com o envio desligado), "trocar" abre o seletor de destino (chips das playlists e campo "buscar ou criar"), e o botão diz o que fará ("enviar 5 músicas para rock"). "manter ordem (01, 02…)" fica ligado por padrão. A playlist nova só nasce no primeiro upload; nenhuma rota nova. Com 7 ou mais playlists aparece o filtro.
- Portal, gerência do que já está no cartão: grupo "soltos" (.mp3 na raiz de `/music` e na raiz da mídia, até 64) com mover e apagar; por faixa ↑ ↓ (reordenar), mover para… (mesmo seletor de destino, inclusive nos soltos), renomear, apagar (2º toque em 3 s); por playlist renomear e apagar. Mover nunca sobrescreve, respeita 16 playlists e 128 faixas e remove a playlist de origem se esvaziar. Reordenar troca os prefixos "NN - " dos dois nomes e mantém os títulos; precisa de prefixo numérico diferente nos dois. Nomes fora do PathRules (mais de 48 bytes, caracteres proibidos) não aparecem.
- A primeira tecla com o display apagado só acorda o display.
- Idioma pt-BR e en: catálogo em `firmware/src/Text.h`, motor `mrm::Locale` no fmk (v0.12.0). A escolha fica em `config.json`.
- Texto novo precisa caber em 128 px nos dois idiomas: `scripts/layout.py` mede e o gate reprova se estourar.

## Funcionalidades extras (Config, desligadas por padrão)

| Opção | O que faz |
|---|---|
| Tela apaga sozinha | Parado por 10 s, a tela apaga e a música continua. A próxima tecla só acende a tela. Desligada (padrão), a tela nunca apaga sozinha |
| Bloqueio de botões | A+B 3 vezes seguidas trava os botões (bolso). Travado, qualquer tecla com a tela acesa só mostra o aviso "A+B 3x desbloqueia"; com a tela apagada nada acende (o bolso não acende a tela). A+B 3 vezes de novo destrava |

Detalhes do bloqueio:
- Só vale na Home. Com a opção ligada, A+B espera 0,7 s para contar os toques: 1 ou 2 toques trocam de playlist (com esse atraso, e a tela acende), 3 travam ou destravam. Com a tela apagada, o A+B também conta, sem precisar acordar a tela antes.
- O bloqueio não é salvo: ao reiniciar, os botões estão livres.
- Na tela Extras, o título vira a explicação curtíssima da linha em foco (a tela é pequena demais para um texto à parte).

## Jogos (Config, Jogos)

Snake, Impacto, Breakout, Torre, Ritmo e Órbita, todos com 2 botões. O tutorial (objetivo, o que cada tecla faz e uma mini animação) aparece só na primeira vez de cada jogo e depois fica em Como jogar, na pausa. Pausar: as duas teclas segurando 0,7 s (ou 20 s sem tocar, para poupar bateria). Depois de pausar ou de morrer, os botões só valem depois de soltos, para o gesto não escolher uma opção sozinho. Cada jogo guarda o recorde.

| Jogo | A | B |
|---|---|---|
| Snake | vira para a esquerda | vira para a direita |
| Impacto | sobe (tiro automático) | desce |
| Breakout | raquete para a esquerda | raquete para a direita |
| Torre | toque solta o bloco | toque freia o bloco (3 por partida) |
| Ritmo | toque na pista da esquerda, segurar sustenta nota longa | idem na pista da direita |
| Órbita | segurar gira o ponto para a esquerda (toque dá um passo) | segurar gira para a direita (toque dá um passo) |

## Limitações assumidas

- Pausar tem 250 ms de atraso (espera o duplo). `kDoubleGapMs` sai do teste de bancada: se incomodar, baixar até ~200 ms.
- Volume não tem passo único: o menor movimento é segurar 400 ms (um passo) e a direção só muda soltando e segurando a outra tecla.
- Prev só por A duplo; playlist só "próxima" (circular). Sem aleatório, repetir ou buscar dentro da faixa.
- Desligar fica na chave SS-12D00. Pausa longa entra em `lightSleep` e A acorda.
- O período do `loop()` precisa ficar em ~15 ms ou menos para debounce, janela de 200 ms e repeat valerem. O `main` redesenha o OLED a cada 50 ms e não usa `delay`. Medido: `loop max` de 1 a 4 ms. Se passar de 15 ms com áudio, SD e Bluetooth reais, ler o `Keypad` em tarefa ou timer separado.
- Gesto de fronteira: um hold que passa de 400 ms na mesma volta em que o dedo solta vira toque. Com loop rápido o efeito é desprezível.

## Hardware

| Tecla | GPIO | Lado do DevKit | Nota |
|---|---|---|---|
| A | 33 | direito | pull-up interno, outro lado no GND, acorda do light sleep |
| B | 32 | direito | pull-up interno, outro lado no GND |

GPIO13, 14 e 27 ficam livres (eram prev, vol+ e vol-). O GND do botão vem do mesmo trilho. Cada botão é só 2 fios na breadboard.

Efeito no case e na perfboard: 2 furos de botão em vez de 5, tampa mais simples, 3 headers a menos. A perfboard em `perfboard/index.html` ainda desenha 5 e precisa ser refeita na Fase 2.

## Teste de bancada

Os gestos foram validados na breadboard em 2026-10-01 (toque, duplo, triplo, segurar nos dois botões, A+B toque e A+B longo, rolar o dedo de A para B, `loop max` de 1 a 4 ms). Não testados ainda: soltar uma tecla cedo ou reapertar dentro da combinação (A+B).

Agora os gestos se conferem pelas telas: Home (play, faixa, volume, playlist) e menus. Os eventos brutos não saem mais no serial.

## Não fazer agora

- Aceleração do volume, perfis de gesto, configuração por usuário, reconexão por botão.
- Passar o decodificador para o `marmota-fmk`: só se outro device precisar de 2 teclas com gestos.
