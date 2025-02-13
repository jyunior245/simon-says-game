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
    return (y * 5) + x;  // Ajuste para matriz 5x5
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
        leds[index] = (pixel_t){g, r, b};
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

void joystick_read_axis(uint16_t *eixo_x, uint16_t *eixto_y){
    adc_select_input(ADC_CHANNEL_0);
    sleep_us(2);
    *eixo_x = adc_read();

    adc_select_input(ADC_CHANNEL_1);
    sleep_us(2);
    *eixo_x = adc_read();
} 

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

    // Após a sequência verde, acende o LED azul do último botão pressionado
    if (last_button != -1) {
        npSetLED(last_button, 0, 0, 255);
        npWrite();
    }
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
    
    for (int i = 0; i < MAX_SEQUENCE; i++)
        sequence[i] = rand() % LED_COUNT;

    showSequence();
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

    last_button = button; // Salva o último botão pressionado

    npSetLED(button, 0, 0, 255);  // Mostra azul ao pressionar
    npWrite();

    sleep_ms(300);
    npClear();
    npWrite();

    if (button == sequence[player_index]) {
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

int main() {
    int bt = 0;
    stdio_init_all();
    sleep_ms(2000);
    npInit(LED_PIN);
    npClear();
    npWrite();
    initButtons();
    srand(time_us_64());

    resetGame();

    while (true)
        checkInput();
}
