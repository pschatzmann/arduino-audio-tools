/*
    This example code is in the Public Domain (or CC0 licensed, at your option.)

    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * initialize S/PDIF driver
 *   rate: sampling rate, 44100Hz, 48000Hz etc.
 */ 
void spdif_init(int rate);

/*
 * send PCM data to S/PDIF transmitter
 *   src: pointer to 16bit PCM stereo data
 *   size: number of data bytes
 */
void spdif_write(const void *src, size_t size);

/*
 * change sampling rate
 *   rate: sampling rate, 44100Hz, 48000Hz etc.
 */ 
void spdif_set_sample_rates(int rate);

#ifdef __cplusplus
}
#endif
