#include <stdio.h>
#include <string.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"

// nossas bibliotecas
#include "../include/display.h"
#include "../include/button.h"
#include "fft_analyzer.h"

// enum para controlar o modo de exibicao atual
typedef enum {
    MODE_PEAK_FREQUENCY,
    MODE_SPECTRUM_ANALYZER,
    MODE_CHROMATIC_TUNER
} display_mode_t;

void draw_peak_mode(display* disp, float peak_freq) {
    char buffer[32];
    display_draw_string(2, 2, "Frequencia", true, disp);
    display_draw_line(0, 12, 127, 12, true, disp);
    
    snprintf(buffer, sizeof(buffer), "%.2f Hz", peak_freq);
    display_draw_string(10, 30, buffer, true, disp);
}

// analisador de espectro com escala dinamica
void draw_spectrum_mode(display* disp) {
    const float* magnitudes = fft_analyzer_get_magnitudes_ptr();
    uint32_t count = fft_analyzer_get_magnitudes_count();

    // titulo
    display_draw_string(2, 2, "Espectro", true, disp);
    display_draw_line(0, 12, 127, 12, true, disp);

    // logica de escala dinamica
    // encontra a magnitude maxima no frame atual
    float max_magnitude = 0.0f;
    for (int i = 1; i < count; i++) {
        if (magnitudes[i] > max_magnitude) {
            max_magnitude = magnitudes[i];
        }
    }

    // se estiver em silencio, nao desenha para evitar ruido
    if (max_magnitude < 10.0) { // limiar de silencio, ajuste se precisar
        return;
    }
    
    // altura maxima para as barras
    const int max_bar_height = DISPLAY_HEIGHT - 14; 

    // desenha as barras com escala dinamica
    for (int i = 0; i < DISPLAY_WIDTH; i++) {
        int start_bin = (i * count) / DISPLAY_WIDTH;
        int end_bin = ((i + 1) * count) / DISPLAY_WIDTH;
        if (start_bin == 0) start_bin = 1;
        if (end_bin <= start_bin) end_bin = start_bin + 1;

        float avg_mag = 0;
        for (int j = start_bin; j < end_bin && j < count; j++) {
            avg_mag += magnitudes[j];
        }
        avg_mag /= (end_bin - start_bin);

        // a altura da barra e proporcional a sua magnitude
        // o logaritmo (escala db) melhora a visualizacao
        float log_mag = log10f(avg_mag + 1.0f);
        float log_max_mag = log10f(max_magnitude + 1.0f);
        
        int bar_height = (int)((log_mag / log_max_mag) * max_bar_height);

        if (bar_height > max_bar_height) bar_height = max_bar_height;
        if (bar_height < 0) bar_height = 0;

        display_draw_line(i, DISPLAY_HEIGHT - 1, i, DISPLAY_HEIGHT - 1 - bar_height, true, disp);
    }
}

const char* NOTE_NAMES[] = {"A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#"};
void draw_tuner_mode(display* disp, float peak_freq) {
    char buffer[32];
    display_draw_string(2, 2, "Afinador", true, disp);
    display_draw_line(0, 12, 127, 12, true, disp);

    // silencio ou ruido de baixa frequencia
    if (peak_freq < 20.0) { 
        display_draw_string(40, 30, "--.--", true, disp);
        return;
    }

    // converte frequencia para nota musical
    int note_index = (int)round(12 * log2(peak_freq / 440.0));
    float ideal_freq = 440.0 * pow(2.0, note_index / 12.0);
    int cents_diff = (int)round(1200 * log2(peak_freq / ideal_freq));
    
    // pega o nome da nota
    const char* note_name = NOTE_NAMES[(note_index % 12 + 12) % 12];
    
    snprintf(buffer, sizeof(buffer), "Nota: %s", note_name);
    display_draw_string(10, 25, buffer, true, disp);
    
    // barra de afinacao
    int center_x = DISPLAY_WIDTH / 2;
    int indicator_pos = center_x + (cents_diff / 2); // 1 pixel = 2 cents
    if(indicator_pos < 5) indicator_pos = 5;
    if(indicator_pos > DISPLAY_WIDTH - 5) indicator_pos = DISPLAY_WIDTH - 5;

    display_draw_line(center_x, 45, center_x, 55, true, disp); // marca central
    display_draw_rectangle(indicator_pos - 2, 48, indicator_pos + 2, 52, true, true, disp); // indicador
}

display disp;

int main() {
    stdio_init_all();
    
    // inicializa as bibliotecas
    display_init(&disp);
    button_init();
    fft_analyzer_init();

    display_mode_t current_mode = MODE_PEAK_FREQUENCY;
    bool hold_mode = false;

    // mensagem de boas-vindas
    display_clear(&disp);
    display_draw_string(30, 20, "Analisador", true, &disp);
    display_draw_string(40, 35, "de Audio", true, &disp);
    display_update(&disp);
    sleep_ms(1000);

    while (true) {
        
        button_event event = button_get_event();
        if (event != BUTTON_NONE) {
            switch (event) {
                case BUTTON_A: // muda de modo
                    current_mode = (current_mode + 1) % 3;
                    break;
                case BUTTON_B: // congela/descongela
                    hold_mode = !hold_mode;
                    break;
                // logica de bootsel com desligamento seguro
                case BUTTON_JOYSTICK:
                    display_clear(&disp);
                    display_draw_string(10, 30, "Reiniciando...", true, &disp);
                    display_update(&disp);
                    sleep_ms(250);

                    display_shutdown(&disp);
                    
                    sleep_ms(250); // pausa para garantir que tudo desligou

                    reset_usb_boot(0, 0);
                    break;
                default:
                    break;
            }
            button_clear_event();
        }

        if (!hold_mode) {
            fft_analyzer_run_analysis();
        }

        // obtem o pico de frequencia (usado em mais de um modo)
        float peak_freq = fft_analyzer_get_peak_frequency();

        display_clear(&disp); // limpa o buffer
        
        // desenha a tela do modo atual
        switch (current_mode) {
            case MODE_PEAK_FREQUENCY:
                draw_peak_mode(&disp, peak_freq);
                break;
            case MODE_SPECTRUM_ANALYZER:
                draw_spectrum_mode(&disp);
                break;
            case MODE_CHROMATIC_TUNER:
                draw_tuner_mode(&disp, peak_freq);
                break;
        }

        // se estiver em modo hold, mostra um indicador
        if (hold_mode) {
            display_draw_string(DISPLAY_WIDTH - 24, 2, "[H]", true, &disp);
        }

        display_update(&disp); // envia o buffer para o display fisico
        sleep_ms(50); // controla a taxa de atualizacao
    }

    return 0;
}
