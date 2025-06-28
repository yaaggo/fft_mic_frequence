#ifndef FFT_ANALYZER_H
#define FFT_ANALYZER_H

#include <stdint.h>

#define N_SAMPLES 1024
#define SAMPLING_FREQUENCY_HZ 1000

void fft_analyzer_init(void);
void fft_analyzer_run_analysis(void);
float fft_analyzer_get_peak_frequency(void);
const float* fft_analyzer_get_magnitudes_ptr(void);
uint32_t fft_analyzer_get_magnitudes_count(void);


#endif // FFT_ANALYZER_H