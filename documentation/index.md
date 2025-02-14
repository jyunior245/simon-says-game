# Documentação do Jogo de Memória com LEDs e Joystick

## Introdução
Este projeto implementa um jogo de memória inspirado no estilo "Simon Says", utilizando uma Raspberry Pi Pico, um joystick analógico e uma matriz de LEDs WS2812B. O objetivo do jogo é fazer o jogador memorizar e reproduzir uma sequência de luzes, utilizando o joystick para controlar a seleção de LEDs.

## Hardware Utilizado
- Raspberry Pi Pico: Controlador principal do projeto, gerenciando o código e a interface com os componentes externos.
- Matriz de LEDs WS2812B (5x5): Matriz de LEDs endereçáveis usada para mostrar as sequências de luzes.
- Joystick analógico com botão: Utilizado para navegar e selecionar LEDs na matriz. O botão também é usado para iniciar ou confirmar a sequência.
- Resistores para pull-up nos botões: Para garantir um valor lógico estável quando o botão é pressionado.

## Dependências
O código faz uso das bibliotecas: 
- `stdio.h`: Para funções de entrada e saída, como impressão de mensagens no console.
- `pico/stdlib.h`: Contém funções padrão da Raspberry Pi Pico, como manipulação de GPIOs e temporizações.
- `hardware/pio.h` e `hardware/gpio.h`: Para controlar os GPIOs e o PIO (Input/Output Processor) da Raspberry Pi Pico, necessários para enviar dados aos LEDs WS2812B.
- `hardware/adc.h`: Para ler os valores analógicos do joystick e interpretar os movimentos.
- `ws2818b.pio.h`: Para controle dos LEDs WS2812B via PIO, que permite enviar sinais de controle para LEDs endereçáveis com alta eficiência.

## Configuração dos Pinos
A tabela abaixo descreve a alocação de pinos da Raspberry Pi Pico para cada componente:

| Componente  | Pino Pico |
|-------------|----------|
| LED WS2812B | GP7      |
| Joystick VRx | GP20 (ADC0) |
| Joystick VRy | GP27 (ADC1) |
| Joystick Botão | GP22 |

## Estruturas de Dados
### `pixel_t`
A estrutura `pixel_t` é utilizada para armazenar as cores de cada LED na matriz. Cada LED possui três componentes de cor (vermelho, verde e azul), que podem ser configurados individualmente.
```c
typedef struct {
    uint8_t G, R, B;
} pixel_t;
```
Cada LED é representado por três valores de cor: verde (`G`), vermelho (`R`) e azul (`B`).


Aqui, o formato RGB é utilizado, pois é uma representação comum para cores e é amplamente suportada por LEDs endereçáveis como os WS2812B.
### `leds[LED_COUNT]`
O array `leds[]` contém o estado de cada LED na matriz. Cada elemento do array é uma estrutura `pixel_t` que define a cor do LED correspondente.
.
```c
pixel_t leds[LED_COUNT];
```
Esse array armazena as cores de todos os LEDs que compõem a matriz de 5x5. Ele serve como o buffer temporário, onde as cores dos LEDs são armazenadas antes de serem enviadas para a matriz real de LEDs.

### `sequence[MAX_SEQUENCE]`
O array `sequence[]` armazena a sequência de LEDs que o jogador deve memorizar. Cada posição contém um número que representa um LED específico
```c
int sequence[MAX_SEQUENCE];
```
Este array é fundamental para o funcionamento do jogo, pois nele são armazenados os índices dos LEDs que o jogador deve memorizar e reproduzir corretamente. A sequência é gerada aleatoriamente e a dificuldade aumenta conforme o jogo avança.

### `player_index` e `sequence_length`
- `player_index`: Controla a posição atual do jogador na sequência. Esse índice é incrementado conforme o jogador acerta a sequência.
- `sequence_length`: Define o tamanho da sequência que o jogador deve memorizar. Inicialmente, começa em 1 e aumenta conforme o jogador completa o desafio.
```c
int player_index = 0;
int sequence_length = 1;
```
Essas variáveis permitem que o jogo acompanhe o progresso do jogador e ajuste a dificuldade com base na sequência que o jogador está tentando repetir.

## Funções Principais e sua Explicação

### `npInit(uint pin)`
Essa função é responsável por inicializar o controlador PIO (Input/Output Processor) da Raspberry Pi Pico para controlar os LEDs WS2812B. O PIO permite uma comunicação altamente eficiente com os LEDs endereçáveis, como o WS2812B, pois ele pode enviar dados em alta velocidade sem sobrecarregar o processador principal da Pico.
```c
void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, true);
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
    pio_sm_set_enabled(np_pio, sm, true);
}
```
Aqui, a função pio_add_program carrega o programa PIO necessário para controlar os LEDs. O PIO é configurado para usar um "estado de máquina" (state machine) que gerencia a transmissão dos dados de controle dos LEDs, fazendo a operação ser mais rápida e eficiente. Ao inicializar o PIO dessa maneira, o código aproveita as capacidades de baixo nível da Raspberry Pi Pico para comunicar-se com os LEDs sem depender do processador central.

### `npSetLED(uint index, uint8_t r, uint8_t g, uint8_t b)`
Esta função define a cor de um LED específico na matriz de LEDs, armazenando essa cor no array `leds[]`. A cor é representada nos componentes RGB, e o LED correspondente é atualizado com os valores fornecidos.
```c
void npSetLED(uint index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < LED_COUNT) {
        leds[index] = (pixel_t){g, r, b};
    }
}
```
Essa função permite alterar dinamicamente a cor de qualquer LED da matriz, o que é essencial para mostrar as sequências de memória no jogo. A troca de cores é feita de forma eficiente, modificando diretamente os valores no buffer `leds[]`, que é posteriormente enviado para os LEDs reais.

### `npClear()`
Essa função desliga todos os LEDs da matriz, atribuindo o valor zero (apagando) para cada LED no array `leds[]`.
```c
void npClear() {
    for (uint i = 0; i < LED_COUNT; i++)
        npSetLED(i, 0, 0, 0);
}
```
A função `npClear()` é útil para limpar a matriz de LEDs antes de mostrar uma nova sequência ou quando o jogador erra a sequência. A limpeza dos LEDs permite que o jogo tenha um comportamento previsível, apagando todos os LEDs para um novo ciclo.
### `npWrite()`
Essa função envia os dados de cor dos LEDs armazenados no array `leds[]` para os LEDs reais através do PIO. Para cada LED, os valores RGB são enviados para o PIO, que por sua vez os transmite para os LEDs WS2812B.
```c
void npWrite() {
    for (uint i = 0; i < LED_COUNT; i++) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(300);
}
```
A função `npWrite()` envia os dados de cada LED para o controlador PIO de forma síncrona. Os dados são enviados sequencialmente, e o PIO processa cada valor (G, R e B) para garantir que a cor desejada seja aplicada aos LEDs. A pausa de 300 microssegundos após o envio dos dados garante que os LEDs recebam a atualização corretamente.

### `showSequence()`
Esta função exibe a sequência de LEDs que o jogador deve memorizar. Cada LED da sequência é aceso por um tempo determinado e depois apagado, permitindo que o jogador tenha tempo para memorizar a ordem.
```c
void showSequence() {
    npClear();
    npWrite();
    sleep_ms(500);

    for (int i = 0; i < sequence_length; i++) {
        npSetLED(sequence[i], 0, 255, 0);
        npWrite();
        sleep_ms(500);
        npClear();
        npWrite();
        sleep_ms(250);
    }
}
```
Cada LED da sequência é aceso na ordem correta e pisca por um curto período. O tempo entre cada LED aceso permite que o jogador observe e memorize a sequência. A escolha da cor verde é feita para destacar a sequência e diferenciá-la de outras ações no jogo.

### `resetGame()`
A função `resetGame()` reinicia o jogo, gerando uma nova sequência aleatória de LEDs e reiniciando as variáveis de controle do jogo, como o tamanho da sequência e o índice do jogador.
```c
void resetGame() {
    sequence_length = 1;
    player_index = 0;
    for (int i = 0; i < MAX_SEQUENCE; i++)
        sequence[i] = rand() % LED_COUNT;
    showSequence();
}
```
Sempre que o jogo é reiniciado, uma nova sequência de LEDs aleatórios é gerada, e o tamanho da sequência começa com 1. O jogador precisa memorizar e reproduzir essa sequência corretamente. O aumento da sequência a cada rodada dificulta o jogo conforme o progresso do jogador.

### `setup_joystick()`

A função `setup_joystick()` configura os pinos do joystick para leitura analógica. Como o joystick possui dois eixos e um botão, é necessário configurar os pinos adequadamente para garantir a correta leitura dos valorea.

```c

void setup_joystick() {
    adc_init(); 
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN); 
    gpio_init(JOYSTICK_BUTTON_PIN); 
    gpio_set_dir(JOYSTICK_BUTTON_PIN, GPIO_IN); 
}
```

Essa função configura os pinos do joystick para que os valores dos eixos possam ser lidos corretamente. O pino do botão é configurado como uma entrada digital, enquanto os pinos dos eixos X e Y são configurados para leituras analógicas.

### `updateLedPosition()`

Esta função é responsável por atualizar a posição do LED na matriz com base no movimento do joystick. O joystick analógico possui dois eixos, `VRx` e `VRy`, que variam os valores de entrada para mover um cursor imaginário. A posição do LED é ajustada com base nessas leituras.

```C
void updateLedPosition() {
    int x = read_joystick_x(); 
    int y = read_joystick_y(); 

    int ledIndex = (y / 100) * 5 + (x / 100);

    npSetLED(ledIndex, 255, 0, 0); 
    npWrite();  /
}
```
Essa função realiza a leitura dos eixos `VRx` e `VRy` do joystick, mapeando a posição para os índices da matriz 5x5. O valor obtido é usado para determinar qual LED será destacado, acendendo-o com a cor vermelha. Isso permite que o jogador mova um "cursor" na matriz e selecione LEDs para tentar replicar a sequência.

### `checkJoystickClick()`
O objetivo dessa função é verificar se o botão do joystick foi pressionado. Quando pressionado, ela registra a escolha do jogador e verifica se a posição selecionada corresponde à posição na sequência do jogo.

```C
void checkJoystickClick() {
    if (is_button_pressed()) {  
        int selectedLed = getSelectedLed();

        if (selectedLed == sequence[player_index]) {
            player_index++; 
            if (player_index == sequence_length) {
                sequence_length++; 
                resetGame(); 
            }
        } else {
            resetGame(); 
    }
}
```

### Loop Principal (`main()`)
O loop principal do jogo inicializa os componentes, gera a sequência e continuamente verifica as entradas do jogador.
```c
int main() {
    stdio_init_all();
    npInit(LED_PIN);
    npClear();
    npWrite();
    setup_joystick();
    srand(time_us_64());
    resetGame();
    while (true) {
        updateLedPosition();
        checkJoystickClick();
        sleep_ms(100);
    }    
}
```
O loop principal é responsável por manter o jogo em execução, permitindo que o jogador interaja com os LEDs usando o joystick. A cada ciclo, a posição do LED é atualizada com base no movimento do joystick e as entradas são verificadas para confirmar as escolhas do jogador.

## Resumo do Fluxo do Jogo

O jogo segue o seguinte fluxo:

### 1. Inicialização:

- O hardware (Pico, LEDs, joystick) é configurado.
- Uma sequência de LEDs aleatória é gerada.

### 2. Exibição da Sequência:

- Os LEDs piscam, exibindo a sequência que o jogador deve memorizar.
- O jogador tem que observar e lembrar a ordem dos LEDs.

### 3. Interação do Jogador:

- O jogador move o joystick para selecionar um LED da matriz.
- Quando o botão do joystick é pressionado, a seleção é comparada com a sequência.

### 4. Verificação e Progresso:

- Se a escolha do jogador estiver correta, o índice é incrementado e a sequência aumenta.
- Se o jogador errar, o jogo é reiniciado.

Esse fluxo permite que o jogador tenha uma experiência de jogo envolvente e desafiadora, à medida que a sequência se torna mais longa e difícil de memorizar com o tempo.



