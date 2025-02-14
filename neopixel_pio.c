#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include <stdlib.h>

#include "ws2818b.pio.h" 

#define LED_COUNT 25
#define LED_PIN 7
#define MAX_SEQUENCE 10

// Pinos do joystick
const int vRx = 20; 
const int vRy = 27;
const int ADC_CHANNEL_0 = 0;

const int ADC_CHANNEL_1 = 1;
const int SW = 22;

// Controle de brilho e posição do LED
int global_brightness = 40;
int led_x = 1, led_y = 1;

// Estrutura para armazenar cores RGB dos LEDs
typedef struct {
    uint8_t G, R, B;
} pixel_t;

pixel_t leds[LED_COUNT]; // Buffer de LEDs
PIO np_pio;
uint sm;

// Sequência do jogo
int sequence[MAX_SEQUENCE];
int player_index = 0;
int sequence_length = 1;

// Calcula o índice de um LED na matriz 5x5
int getLedIndex(int x, int y) {
    if (y % 2 == 0) {
        return (y * 5) + x;  // Linhas pares (esquerda → direita)
    } else {
        return (y * 5) + (4 - x); // Linhas ímpares (direita → esquerda)
    }
}

// Inicializa os LEDs Neopixel
void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, true);
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
    pio_sm_set_enabled(np_pio, sm, true);
    
    for (uint i = 0; i < LED_COUNT; i++)
        leds[i] = (pixel_t){0, 0, 0};  // Inicializa LEDs apagados
}

// Configura um LED com determinada cor
void npSetLED(uint index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < LED_COUNT) {
        leds[index] = (pixel_t){g * global_brightness / 255, r * global_brightness / 255, b * global_brightness / 255};
    }
}

// Apaga todos os LEDs
void npClear() {
    for (uint i = 0; i < LED_COUNT; i++)
        npSetLED(i, 0, 0, 0);
}

// Envia os dados dos LEDs para o Neopixel
void npWrite() {
    for (uint i = 0; i < LED_COUNT; i++) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(300);
}

// Faz os LEDs piscarem em vermelho para indicar erro
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

// Lê os valores dos eixos do joystick
void joystick_read_axis(uint16_t *eixo_x, uint16_t *eixo_y){
    adc_select_input(ADC_CHANNEL_1);
    sleep_us(2);
    *eixo_x = adc_read();

    adc_select_input(ADC_CHANNEL_0);
    sleep_us(2);
    *eixo_y = adc_read();
} 

// Exibe a sequência de LEDs
void showSequence() {
    npClear();
    npWrite();
    sleep_ms(500);

    for (int i = 0; i < sequence_length; i++) {
        npSetLED(sequence[i], 0, 255, 0); // LEDs verdes
        npWrite();
        sleep_ms(500);
        npClear();
        npWrite();
        sleep_ms(250);
    }
}

// Reinicia o jogo gerando uma nova sequência
void resetGame() {
    sequence_length = 1;
    player_index = 0;
    
    for (int i = 0; i < MAX_SEQUENCE; i++)
        sequence[i] = rand() % LED_COUNT;

    showSequence();
}

// Configura o joystick
void setup_joystick(){
    adc_init();
    adc_gpio_init(vRx);
    adc_gpio_init(vRy);
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);
}

// Atualiza a posição do LED com o joystick
void updateLedPosition() {
    uint16_t eixo_x, eixo_y;
    joystick_read_axis(&eixo_x, &eixo_y);

    int max_x = 4, max_y = 4; // Dimensões da matriz

    if (eixo_x > 3000 && led_x > 0){
        led_x--;
    } 
    if (eixo_x < 1000 && led_x < max_x){
        led_x++;
    }
    if (eixo_y > 3000 && led_y < max_y){
        led_y++;
    }
    if (eixo_y < 1000 && led_y > 0){ 
        led_y--;
    }

    npClear();
    int ledIndex = getLedIndex(led_x, led_y);
    npSetLED(ledIndex, 50, 50, 50); // Indica posição do LED
    npWrite();
}

// Verifica se o botão do joystick foi pressionado
void checkJoystickClick() {
    if (!gpio_get(SW)) {
        sleep_ms(50);
        if (!gpio_get(SW)) {
            int current_led_index = getLedIndex(led_x, led_y);

            if (player_index < sequence_length && current_led_index == sequence[player_index]) {
                npSetLED(current_led_index, 0, 255, 0); // LED verde
                npWrite();
                sleep_ms(300);

                player_index++;
                if (player_index == sequence_length) {
                    sequence_length++;
                    player_index = 0;
                    sleep_ms(500);
                    showSequence();
                }
            } else {
                flashRed(3);
                sleep_ms(1000);
                resetGame();
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
    srand(time_us_64());

    resetGame();

    while (true) {
        updateLedPosition();
        checkJoystickClick();
        sleep_ms(100);
    }    
}
