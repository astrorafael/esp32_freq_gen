/* 
   (c) Rafael González (astrorafael@gmail.com), LICA, Ftad. CC. Fisicas, UCM

   See project's LICENSE file.
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif


/* ************************************************************************* */
/*                         INCLUDE HEADER SECTION                            */
/* ************************************************************************* */

// -----------------------------------
// Expressif SDK-IDF standard includes
// -----------------------------------

#include <driver/gpio.h>
#include <nvs_flash.h>
#include <nvs.h>

/* ************************************************************************* */
/*                               DATATYPES SECTION                           */
/* ************************************************************************* */

typedef struct {
    double        freq;       // frequency (Hz)
    double        duty_cycle; // duty cycle (0 < x < 1)
    gpio_num_t    gpio_num;	  // GPIO number
} freq_nvs_info_t;

/* ************************************************************************* */
/*                               API FUNCTIONS                               */
/* ************************************************************************* */

esp_err_t freq_nvs_autoboot_load(uint32_t* flag);

esp_err_t freq_nvs_autoboot_save(uint32_t  flag);

esp_err_t freq_nvs_info_erase(uint32_t channel);

esp_err_t freq_nvs_begin_transaction(nvs_open_mode_t open_mode, nvs_handle_t* handle);

esp_err_t freq_nvs_info_load(nvs_handle_t handle, uint32_t channel, freq_nvs_info_t* info);

esp_err_t freq_nvs_info_save(nvs_handle_t handle, uint32_t channel, const freq_nvs_info_t* info);

esp_err_t freq_nvs_end_transaction(nvs_handle_t handle, bool commit);


#ifdef __cplusplus
}
#endif


