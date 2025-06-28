// fft_analyzer.c

#include "fft_analyzer.h" // Inclui nossa própria interface

// Includes do Pico SDK e da biblioteca padrão C
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/timer.h"

// =================================================================================
// DEFINIÇÕES E VARIÁVEIS INTERNAS DA BIBLIOTECA
// =================================================================================

// Definição do PI para os cálculos
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Intervalo de amostragem calculado a partir da frequência
#define SAMPLING_INTERVAL_US (1000000 / SAMPLING_FREQUENCY_HZ)

// --- Pinos e Canal do ADC ---
#define ADC_CHANNEL 2
#define ADC_PIN     (26 + ADC_CHANNEL)

// Estrutura interna para um número complexo
typedef struct {
    float real;
    float imag;
} complex_t;

// --- Variáveis Globais (estáticas) para a Amostragem e FFT ---
static volatile uint16_t adc_samples[N_SAMPLES];
static volatile int sample_count = 0;
static volatile bool sampling_complete = false;
static struct repeating_timer timer;

static complex_t fft_buffer[N_SAMPLES];      // Buffer para os números complexos
static float fft_magnitudes[N_SAMPLES / 2];  // Buffer para as magnitudes

// =================================================================================
// IMPLEMENTAÇÃO INTERNA DA FFT (FUNÇÕES ESTÁTICAS)
// =================================================================================

// Função para trocar dois números complexos
static void swap_complex(complex_t* a, complex_t* b) {
    complex_t temp = *a;
    *a = *b;
    *b = temp;
}

// O algoritmo FFT Radix-2 de Cooley-Tukey iterativo (in-place)
static void fft_in_place(complex_t data[], uint32_t n) {
    if (n <= 1) return;

    // Etapa 1: Permutação Bit-Reversal
    for (uint32_t i = 1, j = 0; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            swap_complex(&data[i], &data[j]);
        }
    }

    // Etapa 2: Computação das "Borboletas" (Butterflies)
    for (uint32_t len = 2; len <= n; len <<= 1) {
        double ang = 2 * M_PI / len;
        complex_t wlen = {cos(ang), -sin(ang)};
        for (uint32_t i = 0; i < n; i += len) {
            complex_t w = {1.0, 0.0};
            for (uint32_t j = 0; j < len / 2; j++) {
                complex_t u = data[i + j];
                complex_t v;
                v.real = data[i + j + len / 2].real * w.real - data[i + j + len / 2].imag * w.imag;
                v.imag = data[i + j + len / 2].real * w.imag + data[i + j + len / 2].imag * w.real;
                data[i + j].real = u.real + v.real;
                data[i + j].imag = u.imag + v.imag;
                data[i + j + len / 2].real = u.real - v.real;
                data[i + j + len / 2].imag = u.imag - v.imag;

                float w_real_temp = w.real * wlen.real - w.imag * wlen.imag;
                w.imag = w.real * wlen.imag + w.imag * wlen.real;
                w.real = w_real_temp;
            }
        }
    }
}

// Função para calcular as magnitudes do resultado complexo da FFT
static void calculate_magnitudes(const complex_t fft_result[], float magnitudes[], uint32_t n) {
    for (uint32_t i = 0; i < n / 2; i++) {
        magnitudes[i] = sqrtf(fft_result[i].real * fft_result[i].real + fft_result[i].imag * fft_result[i].imag);
    }
}

// =================================================================================
// LÓGICA DE AMOSTRAGEM (FUNÇÕES ESTÁTICAS)
// =================================================================================

// Callback do timer que lê o ADC
static bool timer_callback(struct repeating_timer *t) {
    if (sample_count < N_SAMPLES) {
        adc_samples[sample_count] = adc_read();
        sample_count++;
    } else {
        cancel_repeating_timer(&timer);
        sampling_complete = true;
    }
    return true;
}

// Inicia o processo de amostragem
static void start_sampling() {
    sample_count = 0;
    sampling_complete = false;
    // Inicia um timer que chama o callback em intervalos regulares
    add_repeating_timer_us(-SAMPLING_INTERVAL_US, timer_callback, NULL, &timer);
}


void fft_analyzer_init(void) {
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_CHANNEL);
}

void fft_analyzer_run_analysis(void) {
    start_sampling();
    while (!sampling_complete) {
        tight_loop_contents();
    }

    float dc_offset = 0.0f;
    for (int i = 0; i < N_SAMPLES; i++) dc_offset += (float)adc_samples[i];
    dc_offset /= N_SAMPLES;

    for (int i = 0; i < N_SAMPLES; i++) {
        fft_buffer[i].real = (float)adc_samples[i] - dc_offset;
        fft_buffer[i].imag = 0.0f;
    }

    fft_in_place(fft_buffer, N_SAMPLES);

    calculate_magnitudes(fft_buffer, fft_magnitudes, N_SAMPLES);
}

float fft_analyzer_get_peak_frequency(void) {
    float max_magnitude = 0.0f;
    uint32_t max_index = 0;
    
    for (int i = 1; i < N_SAMPLES / 2; i++) { // Ignora DC
        if (fft_magnitudes[i] > max_magnitude) {
            max_magnitude = fft_magnitudes[i];
            max_index = i;
        }
    }
    
    return (float)max_index * SAMPLING_FREQUENCY_HZ / N_SAMPLES;
}

const float* fft_analyzer_get_magnitudes_ptr(void) {
    return fft_magnitudes;
}

uint32_t fft_analyzer_get_magnitudes_count(void) {
    return N_SAMPLES / 2;
}