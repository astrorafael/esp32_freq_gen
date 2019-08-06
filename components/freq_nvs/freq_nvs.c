/* 
   (c) Rafael González (astrorafael@gmail.com), LICA, Ftad. CC. Fisicas, UCM

   See project's LICENSE file.
*/

/* ************************************************************************* */
/*                         INCLUDE HEADER SECTION                            */
/* ************************************************************************* */

// -------------------
// C standard includes
// -------------------


// -----------------------------------
// Expressif SDK-IDF standard includes
// -----------------------------------

#include <esp_log.h>

// --------------
// Local includes
// --------------

#include "freq_nvs.h"


/* ************************************************************************* */
/*                      DEFINES AND ENUMERATIONS SECTION                     */
/* ************************************************************************* */

#define NVS_TAG "nvs"

// namespace for NVS storage
#define FREQ_NVS_NAMESPACE "freq"

#define NVS_CHECK(a, str, ret_val) \
    if (!(a)) { \
        ESP_LOGE(NVS_TAG,"%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val); \
    }



/* ************************************************************************* */
/*                               DATATYPES SECTION                           */
/* ************************************************************************* */




/* ************************************************************************* */
/*                          GLOBAL VARIABLES SECTION                         */
/* ************************************************************************* */

/* ************************************************************************* */
/*                        AUXILIAR FUNCTIONS SECTION                         */
/* ************************************************************************* */


/* ************************************************************************* */
/*                               API FUNCTIONS                               */
/* ************************************************************************* */

esp_err_t freq_nvs_autoboot_load(uint32_t* flag)
{
	nvs_handle_t handle;
	esp_err_t res;

    res = nvs_open(FREQ_NVS_NAMESPACE, NVS_READONLY, &handle);
    NVS_CHECK(res == ESP_OK, "Error opening NVS handle",res);

    ESP_LOGD(NVS_TAG, "Reading autoboot flag from NVS ... ");
    *flag = 0; // flag will default to 0, if not set yet in NVS

    res = nvs_get_u32(handle, "autoboot", flag);
	if (res == ESP_OK) {
		ESP_LOGD(NVS_TAG, "autoboot flag = %d", *flag);
    } else if (res == ESP_ERR_NVS_NOT_FOUND) {
    	ESP_LOGD(NVS_TAG,"autoboot flag is not initialized yet!");
    	res = ESP_OK;
    }
    nvs_close(handle);
    return res;
}

/* ************************************************************************* */

esp_err_t freq_nvs_autoboot_save(uint32_t flag)
{
	nvs_handle_t handle;
	esp_err_t res;

    res = nvs_open(FREQ_NVS_NAMESPACE, NVS_READWRITE, &handle);
    NVS_CHECK(res == ESP_OK, "Error opening NVS handle", res);

    // Write
    ESP_LOGD(NVS_TAG, "Updating autoboot flag from NVS ... ");
    res = nvs_set_u32(handle, "autoboot", flag);
    //ESP_LOGD(NVS_TAG, (res != ESP_OK) ? "Failed!" : "Done");

    // Commit written value.
    // After setting any values, nvs_commit() must be called to ensure changes are written
    // to flash storage. Implementations may write to storage at other times,
    // but this is not guaranteed.
    ESP_LOGD(NVS_TAG, "Committing updates in NVS ... ");
    res = nvs_commit(handle); 
    //ESP_LOGD(NVS_TAG, (res != ESP_OK) ? "Failed!" : "Done");

    // Close
    nvs_close(handle);
    return res;
}

/* ************************************************************************* */

esp_err_t freq_nvs_info_erase(uint32_t channel)
{	
	nvs_handle_t handle;
	esp_err_t    res;
	char         key[2];

    res = nvs_open(FREQ_NVS_NAMESPACE, NVS_READWRITE, &handle);
    NVS_CHECK(res == ESP_OK, "Error opening NVS handle", res);

    // compose the key string
    key[0] = channel + '0';
    key[1] = 0;

    // Write
    ESP_LOGD(NVS_TAG, "Erasing freq_nvs_info_t info for channel %d in NVS ... ", channel);
    res = nvs_erase_key(handle, key);
    //ESP_LOGD(NVS_TAG, (res != ESP_OK) ? "Failed!" : "Done");

    // Commit written value.
    // After setting any values, nvs_commit() must be called to ensure changes are written
    // to flash storage. Implementations may write to storage at other times,
    // but this is not guaranteed.
    ESP_LOGD(NVS_TAG, "Committing updates for channel %d in NVS ... ", channel);
    res = nvs_commit(handle);
  	//ESP_LOGD(NVS_TAG, (res != ESP_OK) ? "Failed!" : "Done");

    // Close
    nvs_close(handle);
    return res;
}

/* ************************************************************************* */

esp_err_t freq_nvs_begin_transaction(nvs_open_mode_t open_mode, nvs_handle_t* handle)
{
	esp_err_t    res;

 	res = nvs_open(FREQ_NVS_NAMESPACE, NVS_READWRITE, handle);
    NVS_CHECK(res == ESP_OK, "Error opening NVS handle", res);
    return ESP_OK;
}

/* ************************************************************************* */

esp_err_t freq_nvs_end_transaction(nvs_handle_t handle, bool commit)
{
	esp_err_t res = ESP_OK;

	if (commit) {
	 	ESP_LOGD(NVS_TAG, "Committing updates in NVS ... ");
	    res = nvs_commit(handle);
	    //ESP_LOGD(NVS_TAG, (res != ESP_OK) ? "Failed!" : "Done");
	}

    // Close
    nvs_close(handle);

    return res;
}

/* ************************************************************************* */


/* ************************************************************************* */
//esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length)

//esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
//esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key)
//esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key)
/* ************************************************************************* */

esp_err_t freq_nvs_info_load(nvs_handle_t handle, uint32_t channel, freq_nvs_info_t* info)
{
	esp_err_t    res;
	char         key[2];
	size_t       length;

    // Default value if not found
    info->freq       = 0;
    info->duty_cycle = 0;
    info->gpio_num   = GPIO_NUM_NC;

    // compose the key string
    key[0] = channel + '0';
    key[1] = 0;

    res = nvs_get_blob(handle, key, info, &length);
    if (res == ESP_OK) {
		NVS_CHECK(length == sizeof(freq_nvs_info_t), "Read size does not match freq_nvs_info_t size", ESP_FAIL);
    } else if (res == ESP_ERR_NVS_NOT_FOUND) {
    	ESP_LOGD(NVS_TAG, "No freq_nvs_info_t info stored in NVS for channel %u", channel);
    	res = ESP_OK;
    }
    return res;
}

/* ************************************************************************* */

esp_err_t freq_nvs_info_save(nvs_handle_t handle, uint32_t channel, const freq_nvs_info_t* info)
{	
	esp_err_t    res;
	char         key[2];

    // compose the key string
    key[0] = channel + '0';
    key[1] = 0;

    // Write
    ESP_LOGD(NVS_TAG, "Updating freq_nvs_info_t info for channel %d in NVS ... ", channel);
    res = nvs_set_blob(handle, key, info, sizeof(freq_nvs_info_t));
    //ESP_LOGD(NVS_TAG, (res != ESP_OK) ? "Failed!" : "Done");

    return res;
}
