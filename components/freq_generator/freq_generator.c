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

#include <math.h>
#include <stdio.h>

// -----------------------------------
// Expressif SDK-IDF standard includes
// -----------------------------------

#include <esp_system.h>
#include <esp_log.h>

// --------------
// Local includes
// --------------

#include "freq_generator.h"

/* ************************************************************************* */
/*                      DEFINES AND ENUMERATIONS SECTION                     */
/* ************************************************************************* */

#define NO_RX_BUFFER        0
#define DEFAULT_ALLOC_FLAGS 0

#define FGEN_APB 80000000.0
#define FGEN_TAG "FGen"

#define FGEN_CHECK(a, str, ret_val) \
    if (!(a)) { \
        ESP_LOGE(FGEN_TAG,"%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val); \
    }


#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


/* ************************************************************************* */
/*                               DATATYPES SECTION                           */
/* ************************************************************************* */

typedef enum {
    FGEN_CHANNEL_FREE,
    FGEN_CHANNEL_USED,
    FGEN_CHANNEL_UNAVAILABLE,   // because other channl is using its memory block
} fgen_state_t;

typedef struct {
    gpio_num_t gpio_num;    // GPIO number
    bool       allocated;   // GIPO resource status (allocated to frequency geneartor or free)
} fgen_gpio_t;

typedef struct {
    size_t       mem_blocks;   // number of 64-item memory blocks allocated to this channel
    fgen_state_t state;         // RMT channel state
} fgen_channel_t;


/* ************************************************************************* */
/*                          GLOBAL VARIABLES SECTION                         */
/* ************************************************************************* */

#define FREQ_GPIO_NUM 4

fgen_gpio_t FREQ_GPIO[FREQ_GPIO_NUM] = {
    { GPIO_NUM_5,  false },
    { GPIO_NUM_18, false },
    { GPIO_NUM_19, false },
    { GPIO_NUM_21, false }
}; 


fgen_channel_t FREQ_CHANNEL[RMT_CHANNEL_MAX] = {
    { 1, FGEN_CHANNEL_FREE }, // RMT_CHANNEL_0
    { 1, FGEN_CHANNEL_FREE }, // RMT_CHANNEL_1
    { 1, FGEN_CHANNEL_FREE }, // RMT_CHANNEL_2
    { 1, FGEN_CHANNEL_FREE }, // RMT_CHANNEL_3
    { 1, FGEN_CHANNEL_FREE }, // RMT_CHANNEL_4
    { 1, FGEN_CHANNEL_FREE }, // RMT_CHANNEL_5
    { 1, FGEN_CHANNEL_FREE }, // RMT_CHANNEL_6
    { 1, FGEN_CHANNEL_FREE }  // RMT_CHANNEL_7
}; 



/* ************************************************************************* */
/*                        AUXILIAR FUNCTIONS SECTION                         */
/* ************************************************************************* */


static
gpio_num_t fgen_gpio_alloc(gpio_num_t gpio_num)
{
    if (gpio_num != GPIO_NUM_NC) {
        ESP_LOGD(FGEN_TAG,"returning same GPIO %d as given", gpio_num);
        return gpio_num;
    }

    for (int i=0; i<FREQ_GPIO_NUM; i++) {
        if (FREQ_GPIO[i].allocated == false) {
            FREQ_GPIO[i].allocated = true;
            ESP_LOGD(FGEN_TAG,"Allocating new GPIO %d", FREQ_GPIO[i].gpio_num);
            return FREQ_GPIO[i].gpio_num;
        }
    }
    return GPIO_NUM_NC;
}

/* ------------------------------------------------------------------------- */

static
void fgen_gpio_free(gpio_num_t gpio_num)
{
    
    for (int i=0; i<FREQ_GPIO_NUM; i++) {
        if (FREQ_GPIO[i].gpio_num == gpio_num) {
            FREQ_GPIO[i].allocated = false;
            break;
        }
    }
    return;
}


/* ------------------------------------------------------------------------- */

// A given channel has its block and all the blocks of the
// other upper channels if they are not already allocated 
// RMT Channel 7 -> only 1 block
// RMT Channel 6 -> up to 2 blocks
// RMT Channel 5 -> up to 3 blocks
// ...
// RMT Channel 0 -> up to 8 blocks
static
size_t fgen_max_mem_blocks(rmt_channel_t channel)
{
    size_t sum = 0;
    for (rmt_channel_t i=channel; i<RMT_CHANNEL_MAX; i++) {
        if  (FREQ_CHANNEL[i].state == FGEN_CHANNEL_FREE)
            sum += 1;
    }
    return sum;
}

/* ------------------------------------------------------------------------- */


static
rmt_channel_t fgen_channel_alloc(size_t mem_blocks)
{
    size_t N;
    for (rmt_channel_t i = 0; i < RMT_CHANNEL_MAX; i++) {
        rmt_channel_t ch = RMT_CHANNEL_MAX-1-i;
        N = fgen_max_mem_blocks(ch);
        if (FREQ_CHANNEL[ch].state == FGEN_CHANNEL_FREE && mem_blocks <= N) {
            ESP_LOGD(FGEN_TAG,"Allocating new RMT channel %d with %d blocks", ch, mem_blocks);
            FREQ_CHANNEL[ch].state = FGEN_CHANNEL_USED;
            FREQ_CHANNEL[ch].mem_blocks = mem_blocks;
            if (mem_blocks > 1) {
                mem_blocks -= 1;
                // Marking channels > ch as unavailable because we use their memory blocks
                for(rmt_channel_t j = ch+1; j<ch+1+mem_blocks; j++) {
                    ESP_LOGD(FGEN_TAG,"Marking RMT channel %d as unavailable", j);
                    FREQ_CHANNEL[j].state = FGEN_CHANNEL_UNAVAILABLE;
                    FREQ_CHANNEL[j].mem_blocks = 0;
                }
            }
            return ch;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------------- */

static
void fgen_channel_free(rmt_channel_t channel)
{
    size_t mem_blocks;

    if (FREQ_CHANNEL[channel].state == FGEN_CHANNEL_FREE || FREQ_CHANNEL[channel].state == FGEN_CHANNEL_UNAVAILABLE)
        return;
    mem_blocks = FREQ_CHANNEL[channel].mem_blocks;
    ESP_LOGD(FGEN_TAG,"Freeing RMT channel %d and its %d blocks", channel, mem_blocks);
    for (rmt_channel_t ch = channel; ch < channel+mem_blocks; ch++) {
        ESP_LOGD(FGEN_TAG,"Also freeing adjacent RMT channel %d", ch);
        FREQ_CHANNEL[ch].state = FGEN_CHANNEL_FREE;
        FREQ_CHANNEL[ch].mem_blocks = 1;
    }
}


// Find two fgen N and Prescaler so that
// FGEN_APB = Fout * (Prescaler * N)
// being Prescaler and N both integers

static 
esp_err_t fgen_find_freq(double fout, double duty_cycle, fgen_info_t* fgen)
{

    double whole; 
    double new_N;
    double err, new_err;
    double dNhigh, dNlow;

    fgen->prescaler = 255;   // Assume highest prescaler

    whole   = round(FGEN_APB/fout);
    fgen->N = whole / fgen->prescaler;
    err     = fmod(whole,  fgen->prescaler);

    while (fgen->prescaler > 1) {
        new_N     = whole / fgen->prescaler;
        new_err   = fmod(whole, fgen->prescaler);
        if (new_err == 0.0 && new_N > 1.0) {
            err     = new_err;
            fgen->N = new_N;
            break;
        } else if (new_err < err) {
            err     = new_err;
            fgen->N = new_N;
        }
        fgen->prescaler -= 1; 
    }

    if (fgen->prescaler == 2 && err != 0.0) {
        fgen->prescaler = 1;
        fgen->N         = whole;
    }

    // Now that N has been fixed, we find its High and low part
    // taking into account the duty cycle
    // If N is odd, there will be a roundoff error
    // and we will increment N by one to preserve 50% duty cycle if requested

    dNhigh = fgen->N * duty_cycle; // floating point
    dNlow  = fgen->N - dNhigh;     // still floating point

    FGEN_CHECK(dNhigh >=1.0, "High state count NH < 1", ESP_ERR_INVALID_SIZE);
    FGEN_CHECK(dNlow >= 1.0, "Low  state count NL < 1", ESP_ERR_INVALID_SIZE);

    // If Ntot is odd, we'd better round to even parts
    // and increment Ntot by one to preserve 50% duty cycle if requested
    fgen->NH = (uint32_t)(round(dNhigh));
    fgen->NL = (uint32_t)(round(dNlow));
    fgen->N  = fgen->NH + fgen->NL;   // May be changed by one unit by rounding

    return ESP_OK;
}



/* -------------------------------------------------------------------------- */

static 
uint16_t fgen_count_items(uint32_t NH, uint32_t NL)
{
    uint16_t count = 0;

    // Quick way out for only 1 item
    if (NH < 32768 && NL < 32768) {
        count += 1;  // item
        return count;
    }

    // Long high period
    while (NH > 32767*2) {
        NH   -= 32767*2;
        count += 1;
    }

    // Ending high part
    if ((32767 < NH) && (NH <= 32767*2)) {
        NH -= 32767;
        count += 1;
    } else {
        uint32_t padding = min(NL, 32767);
        NL -= padding;
        count += 1;
    }
        
    // Long low period
    while (NL > 32767*2) {
        NL   -= 32767*2;
        count += 1;
    }

    // Ending low part
    if ((32767 < NL) && (NL <= 32767*2)) {
        NL -= 32767;
        count += 1;
    } else if (NL > 0) {
        count += 1;
    }

    // The count does not include a final EoTx item
    return count;
}

/* -------------------------------------------------------------------------- */

// Fills an array of items
// Give an updated pointer so that we can mark the end of transmission
// or concatenate more items
// NO EoTX value is written, must be written outside
static 
rmt_item32_t* fgen_fill_items(rmt_item32_t* item, uint32_t NH, uint32_t NL)
{

    // Quick way out for only 1 item
    if (NH < 32768 && NL < 32768) {
        item->duration0 = NH; item->level0 = 1;
        item->duration1 = NL; item->level1 = 0;
        item++;
        return item;
    }

    // Long high period
    while (NH > 32767*2) {
        NH   -= 32767*2;
        item->duration0 = 32767; item->level0 = 1;
        item->duration1 = 32767; item->level1 = 1;
        item++;
    }

    // Ending high part
    if ((32767 < NH) && (NH <= 32767*2)) {
        NH -= 32767;
        item->duration0 = 32767; item->level0 = 1;
        item->duration1 = NH;    item->level1 = 1;
        item++;
        
    } else {
        uint32_t padding = min(NL, 32767);
        item->duration0 = NH;      item->level0 = 1;
        item->duration1 = padding; item->level1 = 0;
        item++;
        NL -= padding;
    }
        
    // Long low period
    while (NL > 32767*2) {
        NL   -= 32767*2;
        item->duration0 = 32767; item->level0 = 0;
        item->duration1 = 32767; item->level1 = 0;
        item++;
    }

    // Ending low part
    if ((32767 < NL) && (NL <= 32767*2)) {
        NL -= 32767;
        item->duration0 = 32767; item->level0 = 0;
        item->duration1 = NL;    item->level1 = 0;
        item++;
    } else if (NL > 0) {
        item->duration0 = NL; item->level0 = 0;
        item->duration1 = 0;  item->level1 = 0;
        item++;
    }
    return item;
}

/* -------------------------------------------------------------------------- */

static 
void fgen_print_items(const rmt_item32_t* p, uint32_t N)
{
    ESP_LOGD(FGEN_TAG,"Displaying %d items + EoTx", N-1);

#if CONFIG_LOG_DEFAULT_LEVEL == ESP_LOG_VERBOSE
    const uint32_t NN = 8;
    uint32_t rows = N / NN;
    uint32_t rem  = N % NN;
    uint32_t row, offset, i, j;  
    ESP_LOGV(FGEN_TAG,"%d complete rows with %d items each and %d more items in the last row", rows, NN, rem);
    printf("-------------------------------------------------------------------\n");
    for (row = 0; row < rows; row++) {
        for (i=0; i<NN; i++) {
            offset = NN*row+i;
            printf("{{{%d,%d,%d,%d}}},\t", p[offset].duration0, p[offset].level0, p[offset].duration1, p[offset].level1);
        }
        printf("\n");
    }
    for (j=0; j<rem; j++) {
        offset = NN*rows+j;
        printf("{{{%d,%d,%d,%d}}},\t", p[offset].duration0, p[offset].level0, p[offset].duration1, p[offset].level1);
    }
    if (rem) printf("\n");
    printf("-------------------------------------------------------------------\n");
#endif
}

/* -------------------------------------------------------------------------- */

static 
void fgen_log_params(double Fout, double duty_cycle, fgen_info_t* fgen)
{
    double Tclk, ErrFreq, Errduty_cycle;

    // Recompute the Fout frequency with all that rounding taking place
    // and check the relative  error
    fgen->freq       = FGEN_APB / (fgen->prescaler * (double)(fgen->N));
    fgen->duty_cycle = fgen->NH / (double)(fgen->N);
    ErrFreq          = (fgen->freq - Fout)/Fout;
    Errduty_cycle    = (fgen->duty_cycle - duty_cycle)/duty_cycle; 
    Tclk             = (double) fgen->prescaler / FGEN_APB;

    ESP_LOGD(FGEN_TAG,"Ref Clock = %.0f Hz, Prescaler = %d, RMT Clock = %.2f Hz", FGEN_APB, fgen->prescaler, 1/Tclk);    
    ESP_LOGD(FGEN_TAG,"Ntot = %d, Nhigh = %d, Nlow = %d", fgen->N, fgen->NH, fgen->NL);
    ESP_LOGD(FGEN_TAG,"Fout = %.3f Hz => %.3f Hz (%.2f%%), Duty Cycle = %.2f%% => %.2f%% (%.2f%%)", Fout, fgen->freq, ErrFreq*100, duty_cycle*100, fgen->duty_cycle*100, Errduty_cycle*100);
}

/* -------------------------------------------------------------------------- */


static
void fgen_waveform(fgen_resources_t* res)
{
    // Generate the pattern and repeat it as much as we can within a 64 -item block
    rmt_item32_t* p = res->items;
    for(int i = 0 ; i<res->info.nrep; i++) {
        p = fgen_fill_items(p, res->info.NH, res->info.NL);
    }
    p->val = 0; // mark end of sequence
    fgen_print_items(res->items, res->info.nitems);
}
/* -------------------------------------------------------------------------- */

static
esp_err_t fgen_allocate(const fgen_info_t* info, gpio_num_t gpio_num, fgen_resources_t* res)
{
    esp_err_t ret;

    res->info = *info;      // copy structure
   
    // Allocate a free GPIO pin
    res->gpio_num   = fgen_gpio_alloc(gpio_num);
    FGEN_CHECK(res->gpio_num != GPIO_NUM_NC, "No Free GPIO",  ESP_ERR_NO_MEM);

    res->items      = (rmt_item32_t*) calloc(res->info.nitems, sizeof(rmt_item32_t));
    FGEN_CHECK(res->items != NULL, "Out of memory allocating RMT items",  ESP_ERR_NO_MEM);
   
    // Generate the pattern and repeat it as much as we can within a 64 -item block
    fgen_waveform(res);

    // Allocate a free RMT channel
    res->channel = fgen_channel_alloc(res->info.mem_blocks);
    FGEN_CHECK(res->channel != -1, "No Free RMT channel",  ESP_ERR_NO_MEM);

    // Configure and load the RMT driver
    rmt_config_t config = {
        // Common config
        .channel              = res->channel,
        .rmt_mode             = RMT_MODE_TX,
        .gpio_num             = res->gpio_num,
        .mem_block_num        = res->info.mem_blocks,
        .clk_div              = res->info.prescaler,
        // Tx only config
        .tx_config.loop_en    = true,
        .tx_config.carrier_en = false
    };

    ret = rmt_config(&config);
    FGEN_CHECK(ret == ESP_OK, "Error configure RMT module",  ret);

    ret = rmt_driver_install(res->channel, NO_RX_BUFFER, DEFAULT_ALLOC_FLAGS);
    FGEN_CHECK(ret == ESP_OK, "Error installing RMT driver",  ret);
    ESP_LOGD(FGEN_TAG, "%s: rmt_driver_install() returned ok.", __FUNCTION__ );

    ret = rmt_tx_stop(res->channel);
    FGEN_CHECK(ret == ESP_OK, "Error stopping RMT Tx",  ret);
   
    // This is a needed hack for Tx looping since the rmt_config does not do it.
    ret = rmt_set_tx_intr_en(res->channel, false);
    FGEN_CHECK(ret == ESP_OK, "Error disabling RMT Tx interrupt",  ret);

    return ESP_OK;
    
}

/* ************************************************************************* */
/*                               API FUNCTIONS                               */
/* ************************************************************************* */


esp_err_t fgen_info(double freq, double duty_cycle, fgen_info_t* info)
{
    // Decompose Frequency into the product of 2 factors: prescaler and N
    // Decompose N into NH and NL taking into account dyty cycle
    fgen_find_freq(freq,  duty_cycle, info);
    fgen_log_params(freq, duty_cycle, info);
 
    // See how many RMT 32-bit items needs this frequency generation
    // How many channes does it take and how many repetitions withon a channel
    // to minimize wraparround jitter (1 Tclk delay is introduced by wraparound)

    info->jitter     = info->prescaler / FGEN_APB; 
    info->onitems    = fgen_count_items(info->NH, info->NL);  // without EoTx
    info->mem_blocks = (info->onitems > 0 ) ? 1 + (info->onitems / 64) : 0;
    info->nrep       = (info->mem_blocks * 63) / info->onitems;
    // This is a hack due to a firmware's bug
    info->nrep       = (info->nrep == 63) ? 62 : info->nrep;

    FGEN_CHECK(info->mem_blocks <= 8, "Fout needs more than 8 RMT channels",  ESP_ERR_INVALID_SIZE);
    ESP_LOGD(FGEN_TAG,"Nitems = %d, Mem Blocks = %d", info->onitems, info->mem_blocks);
    ESP_LOGD(FGEN_TAG,"This sequence can be duplicated %d times + final EoTx (0,0,0,0)",info->nrep);
    ESP_LOGD(FGEN_TAG,"Loop jitter %.02f (ms)", info->jitter);

    info->nitems     = info->onitems * info->nrep + 1; // global array size including final EoTx
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */

fgen_resources_t* fgen_alloc(const fgen_info_t* info, gpio_num_t gpio_num)
{
    fgen_resources_t* resources;
    esp_err_t ret;

    resources = (fgen_resources_t*) calloc(1, sizeof(fgen_resources_t));
    FGEN_CHECK(resources != NULL, "Out of memory allocating Resources RAM",  NULL); 

    ret = fgen_allocate(info, gpio_num, resources);
    if (ret != ESP_OK) {
        free(resources);
        return NULL;
    }
    return resources;
}

/* -------------------------------------------------------------------------- */

void fgen_free(fgen_resources_t* res)
{
    fgen_channel_free(res->channel);
    fgen_gpio_free(res->gpio_num);
    ESP_ERROR_CHECK( rmt_driver_uninstall(res->channel) );
    free(res->items);
    free(res);
}

/* -------------------------------------------------------------------------- */

esp_err_t fgen_start(fgen_resources_t* res)
{
    esp_err_t ret;

    ESP_LOGD(FGEN_TAG, "Starting RMT channel %d on GPIO %d => %0.2f Hz",res->channel, res->gpio_num, res->info.freq);

    // Copy the generated pattern we've just generated to the internal RMT buffers
    // The rmt_tx_stop places an EoTx in the beginning of the RMT memory buffer
    ret = rmt_fill_tx_items(res->channel, res->items, res->info.nitems, 0);
    FGEN_CHECK(ret == ESP_OK, "Error copying RMT items to shared mem",  ret);

    // and start
    return rmt_tx_start(res->channel, true);
}

/* -------------------------------------------------------------------------- */

esp_err_t fgen_stop(fgen_resources_t* res)
{
    ESP_LOGD(FGEN_TAG, "Stopping RMT channel %d on GPIO %d => %0.2f Hz",res->channel, res->gpio_num, res->info.freq);
    return rmt_tx_stop(res->channel);
}

/* -------------------------------------------------------------------------- */

#if 0
// I have fopund that in continuos mode, the Tx bit resets itself when the first loop is done
// so it is no longer a reliable indiocator of idle / busy
static esp_err_t rmt_tx_get_state(rmt_channel_t channel, uint32_t* state)
{
    extern rmt_dev_t RMT;

    FGEN_CHECK(channel < RMT_CHANNEL_MAX, "RMT CHANNEL ERR", ESP_ERR_INVALID_ARG);
    *state = RMT.conf_ch[channel].conf1.tx_start;
   return ESP_OK;
}
#else
// I have found that the rmt_tx_stop writes an EoTx marker at the beginning of
// the RMT memory when stopping, so it is a reliable indicator ...
static esp_err_t rmt_tx_get_state(rmt_channel_t channel, uint32_t* state)
{
    extern rmt_mem_t RMTMEM;

    FGEN_CHECK(channel < RMT_CHANNEL_MAX, "RMT CHANNEL ERR", ESP_ERR_INVALID_ARG);
    *state = RMTMEM.chan[channel].data32[0].val;
   return ESP_OK;
}
#endif

rmt_channel_status_t fgen_get_state(const fgen_resources_t* res)
{
    uint32_t state;

    rmt_tx_get_state(res->channel, &state);

    ESP_LOGD(FGEN_TAG, "Getting state of RMT channel %d returned %d", res->channel, state);
    return state ? RMT_CHANNEL_BUSY : RMT_CHANNEL_IDLE;
}

/* -------------------------------------------------------------------------- */
