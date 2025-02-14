#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include <stdlib.h>

#include "ws2818b.pio.h"  // Certifique-se de que o nome do arquivo está correto

#define LED_COUNT 25
#define LED_PIN 7
#define BUTTON_COUNT 4
#define MAX_SEQUENCE 10

//Pinos de analógico do joystisck
const int vRx = 20; 
const int vRy = 27;
const int ADC_CHANNEL_0 = 0;
const int ADC_CHANNEL_1 = 1;
const int SW = 22;

int last_button = -1;  // Armazena o último botão pressionado

int global_brigthness = 25;
int led_x = 1, led_y = 1;

const uint button_pins[BUTTON_COUNT] = {2, 3, 4, 5};  // Definição dos botões

typedef struct {
    uint8_t G, R, B;
} pixel_t;

pixel_t leds[LED_COUNT];
PIO np_pio;
uint sm;

int sequence[MAX_SEQUENCE];
int player_index = 0;
int sequence_length = 1;

int getLedIndex(int x, int y) {
    if (y % 2 == 0) { 
        return (y * 5) + x;  // Linhas pares (esquerda → direita)
    } else {
        return (y * 5) + (4 - x); // Linhas ímpares (direita → esquerda)
    }
}


void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, true);
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
    pio_sm_set_enabled(np_pio, sm, true);

    for (uint i = 0; i < LED_COUNT; i++)
        leds[i] = (pixel_t){0, 0, 0};  // Inicializa os LEDs desligados
}

void npSetLED(uint index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < LED_COUNT) {
        leds[index] = (pixel_t){g * global_brigthness / 255, r * global_brigthness / 255, b * global_brigthness / 255};
    }
}

void npClear() {
    for (uint i = 0; i < LED_COUNT; i++)
        npSetLED(i, 0, 0, 0);
}

void npWrite() {
    for (uint i = 0; i < LED_COUNT; i++) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(300);
}

void flashRed(uint times) {
    for (uint i = 0; i < times; i++) {
        for (uint j = 0; j < LED_COUNT; j++)
            npSetLED(j, 255, 0, 0);
        npWrite();
        sleep_ms(200);
        npClear();
        npWrite();
        sleep_ms(200);
    }
} 

void joystick_read_axis(uint16_t *eixo_x, uint16_t *eixo_y){
    adc_select_input(ADC_CHANNEL_1);
    sleep_us(2);
    *eixo_x = adc_read();

    adc_select_input(ADC_CHANNEL_0);
    sleep_us(2);
    *eixo_y = adc_read();
} 

void showSequence() {
    npClear();
    npWrite();
    sleep_ms(500);

    // Exibir a sequência verde
    for (int i = 0; i < sequence_length; i++) {
        npSetLED(sequence[i], 0, 255, 0); // LEDs verdes
        npWrite();
        sleep_ms(500);
        npClear();
        npWrite();
        sleep_ms(250);
    }

    // Garante que o LED azul em (1,0) fique aceso desde o início
    int led_azul_index = getLedIndex(1, 0);
    npSetLED(led_azul_index, 50, 50, 50);
    npWrite();
}


void initButtons() {
    for (int i = 0; i < BUTTON_COUNT; i++) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }
}

int readButton() {
    for (int i = 0; i < BUTTON_COUNT; i++) {
        if (!gpio_get(button_pins[i])) {  // Botão pressionado (pull-up ativo)
            sleep_ms(50);  // Debounce
            if (!gpio_get(button_pins[i])) {
                printf("Botão pressionado: %d\n", i);
                return i;
            }
        }
    }
    return -1;
}

void resetGame() {
    sequence_length = 1;
    player_index = 0;  // Reinicia a posição do jogador na sequência

    // Gera uma nova sequência corretamente
    for (int i = 0; i < MAX_SEQUENCE; i++)
        sequence[i] = rand() % LED_COUNT;

    showSequence(); // Exibe a nova sequência
}


void setup_joystick(){
    adc_init();
    adc_gpio_init(vRx);
    adc_gpio_init(vRy);

    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);
}

void setup(){
    stdio_init_all();
    setup_joystick();
}

void checkInput() {
    int button;
    while ((button = readButton()) == -1)
        sleep_ms(10);

    // O LED azul deve ser associado ao clique do botão
    int led_azul_index = getLedIndex(1, 0);

    // Se o botão pressionado for o esperado (LED azul)
    if (button == led_azul_index) {
        printf("Botão correto pressionado!\n");

        // Acender rapidamente o LED azul para dar feedback
        npSetLED(button, 50, 50, 50);
        npWrite();
        sleep_ms(300);

        // Continua normalmente no jogo
        npClear();
        npSetLED(led_azul_index, 50, 50, 50); // Mantém o azul aceso
        npWrite();

        player_index++;
        if (player_index == sequence_length) {
            sequence_length++;
            player_index = 0;
            sleep_ms(500);
            showSequence();
        }
    } else {
        printf("Erro! Botão errado pressionado.\n");
        flashRed(3);
        sleep_ms(1000);
        resetGame();
    }
}

void updateLedPosition() {
    uint16_t eixo_x, eixo_y;
    joystick_read_axis(&eixo_x, &eixo_y);

    int threshold = 1000; // Sensibilidade do joystick
    int max_x = 4, max_y = 4; // Dimensões da matriz

    static int last_x = 0, last_y = 0; // Guarda os últimos valores para debounce

    // Movimenta horizontalmente
    if (eixo_x > 3000 && led_x > 0 && last_x != 1 ) {
        led_x--; 
        last_x = 1; // Marca que o movimento foi feito
    } else if (eixo_x < 1000 && led_x < max_x && last_x != 1) {
        led_x++;
        last_x = -1;
    } else if (eixo_x >= 1000 && eixo_x <= 3000) {
        last_x = 0; // Reseta a marcação quando o joystick volta ao centro
    }

    // Movimenta verticalmente
    if (eixo_y > 3000 && led_y < max_y && last_y == 0) {
        led_y++; 
        last_y = 1;
    } else if (eixo_y < 1000 && led_y > 0 && last_y == 0) {
        led_y--;
        last_y = -1;
    } else if (eixo_y >= 1000 && eixo_y <= 3000) {
        last_y = 0;
    }

    // Atualiza a matriz para refletir a nova posição do LED
    npClear();
    int ledIndex = getLedIndex(led_x, led_y);
    npSetLED(ledIndex, 50, 50, 50); // LED branco indicando a posição
    npWrite();
}

void checkJoystickClick() {
    if (!gpio_get(SW)) {  // Se o botão do joystick for pressionado
        sleep_ms(50);  // Debounce
        if (!gpio_get(SW)) {
            int current_led_index = getLedIndex(led_x, led_y);

            if (player_index < sequence_length && current_led_index == sequence[player_index]) {
                printf("Correto! Avançando na sequência.\n");

                // Mantém o LED verde aceso
                npSetLED(current_led_index, 0, 255, 0);
                npWrite();
                sleep_ms(300);

                player_index++;

                if (player_index == sequence_length) {
                    sequence_length++;
                    player_index = 0;
                    sleep_ms(500);
                    showSequence();  // Exibir nova sequência
                }
            } else {
                printf("Erro! Posição errada.\n");
                flashRed(3);
                sleep_ms(1000);
                resetGame();  // Agora reinicia corretamente o jogo
            }
        }
    }
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    npInit(LED_PIN);
    npClear();
    npWrite();
    setup_joystick();
    //initButtons();
    srand(time_us_64());

    resetGame();

    while (true) {
        updateLedPosition();
        checkJoystickClick();  // Verifica se o jogador clicou corretamente
        sleep_ms(100);
    }    
}
