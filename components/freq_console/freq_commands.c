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

#include <stdio.h>

// -----------------------------------
// Expressif SDK-IDF standard includes
// -----------------------------------

#include <esp_log.h>
#include <esp_console.h>
#include <argtable3/argtable3.h>

// --------------
// Local includes
// --------------

#include "freq_generator.h"
#include "freq_nvs.h"


/* ************************************************************************* */
/*                      DEFINES AND ENUMERATIONS SECTION                     */
/* ************************************************************************* */

#define CMD_TAG "CMDS"  // logging tag

/* ************************************************************************* */
/*                               DATATYPES SECTION                           */
/* ************************************************************************* */



/* ************************************************************************* */
/*                          GLOBAL VARIABLES SECTION                         */
/* ************************************************************************* */


fgen_resources_t* FGEN[RMT_CHANNEL_MAX] = { 0, 0, 0, 0, 0, 0, 0, 0 };

// 'params' command arguments variable
static struct params_args_s {
    struct arg_dbl *frequency;
    struct arg_dbl *duty_cycle;
    struct arg_end *end;
} params_args;

// 'create' command arguments variable
static struct create_args_s {
    struct arg_dbl *frequency;
    struct arg_dbl *duty_cycle;
    struct arg_int *gpio_num;
    struct arg_end *end;
} create_args;

// 'delete' command arguments variable
static struct delete_args_s {
    struct arg_int *channel;
    struct arg_lit *nvs;
    struct arg_end *end;
} delete_args;

// 'list' command arguments variable
static struct list_args_s {
    struct arg_lit *extended;
    struct arg_lit *nvs;
    struct arg_end *end;
} list_args;

// 'start' command arguments variable
static struct start_args_s {
    struct arg_int *channel;
    struct arg_end *end;
} start_args;

// 'stop' command arguments variable
static struct stop_args_s {
    struct arg_int *channel;
    struct arg_end *end;
} stop_args;

// 'autoload' command arguments variable
static struct autoload_args_s {
    struct arg_lit *yes;
    struct arg_lit *no;
    struct arg_end *end;
} autoload_args;

// 'load' command arguments variable
static struct load_args_s {
    struct arg_int *channel;
    struct arg_end *end;
} load_args;

// 'save' command arguments variable
static struct save_args_s {
    struct arg_int *channel;
    struct arg_end *end;
} save_args;

/* ************************************************************************* */
/*                        AUXILIAR FUNCTIONS SECTION                         */
/* ************************************************************************* */

static void register_fgen(fgen_resources_t* fgen)
{
    extern fgen_resources_t* FGEN[];

    FGEN[fgen->channel] = fgen; 
}

static void unregister_fgen(fgen_resources_t* fgen)
{
    extern fgen_resources_t* FGEN[];

    FGEN[fgen->channel] = 0; 
}

static fgen_resources_t* search_fgen(rmt_channel_t channel)
{
    extern fgen_resources_t* FGEN[];

    for (rmt_channel_t i = 0; i<RMT_CHANNEL_MAX; i++) {
        if ( (FGEN[i] != NULL) && (FGEN[i]->channel == channel) ) {
            return FGEN[i];
        }
    }
    return NULL;
}

static const char* state_msg(fgen_resources_t* fgen)
{
    static const char* msg[] = {"uninit", "stopped", "started"};
    rmt_channel_status_t state = fgen_get_state(fgen);
    return msg[state];
}

static void print_fgen_summary(fgen_resources_t* fgen)
{
    printf("Channel: %02d [%s]\tGPIO: %02d\tFreq.: %0.2f Hz\tDC.: %0.0f%%\tBlocks: %d\n", 
                fgen->channel, state_msg(fgen), fgen->gpio_num, fgen->info.freq, 100*fgen->info.duty_cycle, fgen->info.mem_blocks);
}

static void print_config_summary(rmt_channel_t channel, freq_nvs_info_t* info)
{
    printf("Channel: %02d [%s]\tGPIO: %02d\tFreq.: %0.2f Hz\tDC.: %0.0f%%\tBlocks: %d\n", 
                channel, "nvs", info->gpio_num, info->freq, 100*info->duty_cycle, 0);
}


/* ************************************************************************* */
/*                     COMMAND IMPLEMENTATION SECTION                        */
/* ************************************************************************* */

// ============================================================================

// forward declaration
static int exec_params(int argc, char **argv);

// 'params' command registration
static void register_params()
{
    extern struct params_args_s params_args;

    params_args.frequency =
        arg_dbl1("f", "freq", "<Hz>", "Frequency");
    params_args.duty_cycle =
        arg_dbl0("d", "duty", "<duty cycle>",
                 "Defaults to 0.5 (50%) if not given");
    params_args.duty_cycle->dval[0] = 0.5; // Give it a default value
  
    params_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command  = "params",
        .help     = "Computes the frequency generator parameters as well as the needed resources. "
                    "Does not create a frequency generator. ",
        .hint     = NULL,
        .func     = exec_params,
        .argtable = &params_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

// 'params' command implementation
static int exec_params(int argc, char **argv)
{
    extern struct params_args_s params_args;
    fgen_info_t    info;

    int nerrors = arg_parse(argc, argv, (void **) &params_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, params_args.end, argv[0]);
        return 1;
    }

    fgen_info( params_args.frequency->dval[0], 
               params_args.duty_cycle->dval[0], 
               &info);

    printf("------------------------------------------------------------------\n");
    printf("                 FREQUENCY GENERATOR PARAMETERS                   \n");
    printf("Final Frequency:\t%0.4f Hz\n", info.freq);
    printf("Final Duty Cycle:\t%0.2f%%\n", info.duty_cycle*100);
    printf("Prescaler:\t\t%d\n", info.prescaler);
    printf("N:\t\t\t%d (%d high + %d low)\n", info.N, info.NH, info.NL);
    printf("Nitems:\t\t\t%d, x%d times + EoTx\n", info.onitems, info.nrep);
    printf("Blocks:\t\t\t%d (64 items each)\n", info.mem_blocks);
    printf("Jitter:\t\t\t%0.3f us every %d times\n", info.jitter*1000000, info.nrep);
    printf("------------------------------------------------------------------\n");
    return 0;
}


// ============================================================================


// forward declaration
static int exec_create(int argc, char **argv);

// 'create' command registration
static void register_create()
{
    extern struct create_args_s create_args;

    create_args.frequency =
        arg_dbl1("f", "freq", "<Hz>", "Frequency");
    create_args.duty_cycle =
        arg_dbl0("d", "duty", "<duty cycle>",
                 "Defaults to 0.5 (50%) if not given");
    create_args.duty_cycle->dval[0] = 0.5; // Give it a default value
    create_args.gpio_num =
        arg_int0("g", "gpio", "<GPIO num>",
                 "Defaults to -1 if not given");
    create_args.gpio_num->ival[0] = GPIO_NUM_NC; // Give it a default value
    create_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command  = "create",
        .help     = "Creates a frequency generator and binds it to a GPIO pin. "
                    "Does not start it.",
        .hint     = NULL,
        .func     = exec_create,
        .argtable = &create_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

// 'create' command implementation
static int exec_create(int argc, char **argv)
{
    extern struct create_args_s create_args;
    fgen_info_t       info;
    fgen_resources_t* fgen;

    int nerrors = arg_parse(argc, argv, (void **) &create_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, create_args.end, argv[0]);
        return 1;
    }

    fgen_info( create_args.frequency->dval[0], 
               create_args.duty_cycle->dval[0], 
               &info);

    fgen = fgen_alloc(&info, create_args.gpio_num->ival[0] );
    if (fgen != NULL) {
        register_fgen(fgen); 
        printf("Channel: %02d [%s]\tGPIO: %02d\tFreq.: %0.2f Hz\tBlocks: %d\n", 
                fgen->channel, state_msg(fgen), fgen->gpio_num, fgen->info.freq, fgen->info.mem_blocks);
    } else {
        printf("NO RESOURCES AVAILABLE TO CREATE A NEW FREQUENCY GENERATOR\n");
    }
    return 0;
}

// ============================================================================


// forward declaration
static int exec_delete(int argc, char **argv);

// 'delete' command registration
static void register_delete()
{
    extern struct delete_args_s delete_args;

    delete_args.channel =
        arg_int0("c", "channel", "<0-7>", "RMT channel number.");
    delete_args.nvs =
        arg_lit0("n", "nvs", "Delete NVS configuration as well.");
    delete_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command  = "delete",
        .help     = "Deletes frequency generator and frees its GPIO pin. "
                    "Deletes all if no channel is given.",
        .hint     = NULL,
        .func     = exec_delete,
        .argtable = &delete_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}


static void exec_delete_single(rmt_channel_t channel)
{
    fgen_resources_t* fgen;

    fgen = search_fgen(channel);
    if (fgen != NULL) {
        if (fgen_get_state(fgen) == RMT_CHANNEL_BUSY) {
            fgen_stop(fgen);
        }
        unregister_fgen(fgen);
        fgen_free(fgen);
    }  
}

// 'delete' command implementation
static int exec_delete(int argc, char **argv)
{
    extern struct delete_args_s delete_args;
    rmt_channel_t channel;

    int nerrors = arg_parse(argc, argv, (void **) &delete_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, delete_args.end, argv[0]);
        return 1;
    }

    if(delete_args.channel->count) {
        channel = delete_args.channel->ival[0];
        exec_delete_single(channel);
        if(delete_args.nvs->count) {
            ESP_ERROR_CHECK( freq_nvs_info_erase(channel) );
        }
    } else {
        for (rmt_channel_t channel= 0; channel<RMT_CHANNEL_MAX; channel++) {
            exec_delete_single(channel);
            if(delete_args.nvs->count) {
                ESP_ERROR_CHECK( freq_nvs_info_erase( channel) );
            }
        }
    }

    return 0;
}

// ============================================================================


// forward declaration
static int exec_list(int argc, char **argv);

// 'list' command registration
static void register_list()
{
    extern struct list_args_s list_args;

    list_args.extended =
        arg_lit0("x", "extended", "Extended listing.");
    list_args.nvs =
        arg_lit0("n", "nvs", "List saved configuration in NVS.");
    list_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command  = "list",
        .help     = "List all created frequency generators or NVS configuration.",
        .hint     = NULL,
        .func     = exec_list,
        .argtable = &list_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

// 'list' command implementation
static int exec_list(int argc, char **argv)
{
    extern struct list_args_s list_args;

    fgen_resources_t* fgen;

    int nerrors = arg_parse(argc, argv, (void **) &list_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, list_args.end, argv[0]);
        return 1;
    }

    // List saved conbfiguration instead

    if (list_args.nvs->count) {

        freq_nvs_info_t info;
        nvs_handle_t    handle;

        ESP_ERROR_CHECK( freq_nvs_begin_transaction(NVS_READONLY, &handle) );
        printf("------------------------------------------------------------------\n");
        for (rmt_channel_t channel= 0; channel<RMT_CHANNEL_MAX; channel++) {
            ESP_ERROR_CHECK( freq_nvs_info_load(handle, channel, &info) );
            if (info.gpio_num != GPIO_NUM_NC) {
                print_config_summary(channel, &info);
            }
        }
        printf("------------------------------------------------------------------\n");
        ESP_ERROR_CHECK( freq_nvs_end_transaction(handle, false) );
        return 0;
    }


    printf("------------------------------------------------------------------\n");
    for (rmt_channel_t channel = 0; channel<RMT_CHANNEL_MAX ; channel++) {
         fgen = search_fgen(channel);
        if (fgen != NULL) {
            print_fgen_summary(fgen);
            if (list_args.extended->count) {
                printf("\tPrescaler: %03d, N: %d (%d + %d)\n", 
                fgen->info.prescaler, fgen->info.N, fgen->info.NH, fgen->info.NL);
            }
        }
    }
    printf("------------------------------------------------------------------\n");   
    return 0;
}

// ============================================================================


// forward declaration
static int exec_start(int argc, char **argv);

// 'start' command registration
static void register_start()
{
    extern struct start_args_s start_args;

    start_args.channel =
        arg_int0("c", "channel", "<0-7>", "RMT channel number.");
    start_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command  = "start",
        .help     = "Starts frequency generator given by channel id. "
                    "Starts all if no channel is given.",
        .hint     = NULL,
        .func     = exec_start,
        .argtable = &start_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void exec_start_single(rmt_channel_t channel)
{
    fgen_resources_t* fgen;
    fgen = search_fgen(channel);
    if (fgen != NULL) {
       fgen_start(fgen);
       print_fgen_summary(fgen);
    }     
}


// 'start' command implementation
static int exec_start(int argc, char **argv)
{
    extern struct start_args_s start_args;

    int nerrors = arg_parse(argc, argv, (void **) &start_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, start_args.end, argv[0]);
        return 1;
    }

    if (start_args.channel->count) {
        exec_start_single(start_args.channel->ival[0]);
    }  else {
        for (rmt_channel_t channel= 0; channel<RMT_CHANNEL_MAX; channel++) {
            exec_start_single(channel);
        }
    }

    return 0;
}

// ============================================================================


// forward declaration
static int exec_stop(int argc, char **argv);

// 'stop' command registration
static void register_stop()
{
    extern struct stop_args_s stop_args;

    stop_args.channel =
        arg_int0("c", "channel", "<0-7>", "RMT channel number.");
    stop_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command  = "stop",
        .help     = "Stops frequency generator given by channel id. "
                    "Stops all if no channel is given.",
        .hint     = NULL,
        .func     = exec_stop,
        .argtable = &stop_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}


static void exec_stop_single(rmt_channel_t channel)
{
    fgen_resources_t* fgen;
    fgen = search_fgen(channel);
    if (fgen != NULL) {
       fgen_stop(fgen);
       print_fgen_summary(fgen);
    }      
}

// 'stop' command implementation
static int exec_stop(int argc, char **argv)
{
    extern struct stop_args_s stop_args;

    int nerrors = arg_parse(argc, argv, (void **) &stop_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, stop_args.end, argv[0]);
        return 1;
    }

    if (stop_args.channel->count) {
        exec_stop_single(stop_args.channel->ival[0]);
    }  else {
        for (rmt_channel_t channel= 0; channel<RMT_CHANNEL_MAX; channel++) {
            exec_stop_single(channel);
        }
    }

    return 0;
}


// ============================================================================

// forward declaration
static int exec_save(int argc, char **argv);

// 'save' command registration
static void register_save()
{
    extern struct save_args_s save_args;

    save_args.channel =
        arg_int0("c", "channel", "<0-7>", "RMT channel number.");
    save_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command  = "save",
        .help     = "Saves frequency generator configuration to NVS given by channel id. "
                    "Saves all if no channel is given.",
        .hint     = NULL,
        .func     = exec_save,
        .argtable = &save_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void do_save_single(nvs_handle_t handle, rmt_channel_t channel)
{
    fgen_resources_t* fgen;
    freq_nvs_info_t   nvs_info;

    fgen = search_fgen(channel);
    if (fgen != NULL) {
        nvs_info.gpio_num   = fgen->gpio_num;
        nvs_info.freq       = fgen->info.freq;
        nvs_info.duty_cycle = fgen->info.duty_cycle;
        ESP_ERROR_CHECK( freq_nvs_info_save(handle, channel, &nvs_info) );
    }  
}

// 'save' command implementation
static int exec_save(int argc, char **argv)
{ 
    extern struct save_args_s save_args;
    nvs_handle_t    handle;

    int nerrors = arg_parse(argc, argv, (void **) &save_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, save_args.end, argv[0]);
        return 1;
    }
    
    ESP_ERROR_CHECK( freq_nvs_begin_transaction(NVS_READWRITE, &handle) );
    if (save_args.channel->count == 0) {
        for (rmt_channel_t ch = 0; ch < RMT_CHANNEL_MAX; ch++) {
            do_save_single(handle, ch);
        }
    } else {
        rmt_channel_t channel = save_args.channel->ival[0];
        do_save_single(handle, channel);
    }
    ESP_ERROR_CHECK( freq_nvs_end_transaction(handle, true) );

    return 0;
}

// ============================================================================

// forward declaration
static int exec_load(int argc, char **argv);

// 'save' command registration
static void register_load()
{
    extern struct load_args_s load_args;

    load_args.channel =
        arg_int0("c", "channel", "<0-7>", "RMT channel number.");

    load_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command  = "load",
        .help     = "Loads frequency generator configuration from NVS given by channel id. "
                    "Loads all if no channel is given.",
        .hint     = NULL,
        .func     = exec_load,
        .argtable = &load_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}


static void do_purge_single(fgen_resources_t* fgen)
{
     if (fgen_get_state(fgen) == RMT_CHANNEL_BUSY) {
        fgen_stop(fgen);
    }
    unregister_fgen(fgen);
    fgen_free(fgen);
}


static void exec_load_single(nvs_handle_t handle, rmt_channel_t channel)
{
    extern fgen_resources_t* FGEN[];
    
    freq_nvs_info_t   nvs_info;
    fgen_info_t       info;
    fgen_resources_t* fgen;

    freq_nvs_info_load(handle, channel, &nvs_info);

    // No channel stored in NVS
    if(nvs_info.gpio_num == GPIO_NUM_NC) {
        return;
    }

    fgen_info( nvs_info.freq, nvs_info.duty_cycle, &info);
    fgen = search_fgen(channel);
    if (fgen == NULL) {
        // Channel not created in memory
        fgen = fgen_alloc(&info, nvs_info.gpio_num);
        register_fgen(fgen); 

    } else {
        // Already existing in memory
        do_purge_single(fgen);
        fgen = fgen_alloc(&info, nvs_info.gpio_num);
        register_fgen(fgen); 
    }

}


// 'load' command implementation
static int exec_load(int argc, char **argv)
{ 
    extern struct load_args_s load_args;
    nvs_handle_t    handle;
    rmt_channel_t channel;

    int nerrors = arg_parse(argc, argv, (void **) &load_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, load_args.end, argv[0]);
        return 1;
    }
    
    ESP_ERROR_CHECK( freq_nvs_begin_transaction(NVS_READONLY, &handle) );
    if (load_args.channel->count == 0) {
        for (rmt_channel_t i = 0; i <  RMT_CHANNEL_MAX; i++) {
            channel = RMT_CHANNEL_MAX - 1 - i;
            exec_load_single(handle, channel);
        }
    } else {
        rmt_channel_t channel = load_args.channel->ival[0];
        exec_load_single(handle, channel);
    }
    ESP_ERROR_CHECK( freq_nvs_end_transaction(handle, false) );
    return 0;
}

// ============================================================================

// forward declaration
static int exec_autoload(int argc, char **argv);

// 'autoload' command registration
static void register_autoload()
{
    extern struct autoload_args_s autoload_args;

    autoload_args.yes =
        arg_lit0("y", "yes", "Enable loading configuration at boot time.");
    autoload_args.no =
        arg_lit0("n", "no", "Disable loading configuration at boot time.");
    autoload_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command  = "autoload",
        .help     = "Enables/disables loading configuration at boot time. "
                    "Displays current mode if no flag is given",
        .hint     = NULL,
        .func     = exec_autoload,
        .argtable = &autoload_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void autoload_at_boot()
{
    uint32_t autoload;
    nvs_handle_t    handle;
    esp_err_t res;
    
    res = freq_nvs_autoboot_load(&autoload);
    if (res != ESP_OK) {
        ESP_LOGW(CMD_TAG, "Could not perform autoboot.Trying to fix it");
        freq_nvs_autoboot_save(false);
        return;
    }

    if (autoload) {
        // Autoload from NVS
        ESP_ERROR_CHECK( freq_nvs_begin_transaction(NVS_READONLY, &handle) );
        for (rmt_channel_t ch = 0; ch <  RMT_CHANNEL_MAX; ch++) {
          exec_load_single(handle, RMT_CHANNEL_MAX-1-ch);
          exec_start_single(RMT_CHANNEL_MAX-1-ch);
        }
        ESP_ERROR_CHECK( freq_nvs_end_transaction(handle, false) );
    }
}

// 'autoload' command implementation
static int exec_autoload(int argc, char **argv)
{ 
    extern struct autoload_args_s autoload_args;

    int nerrors = arg_parse(argc, argv, (void **) &autoload_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, autoload_args.end, argv[0]);
        return 1;
    }

    if (autoload_args.yes->count) {
        freq_nvs_autoboot_save(true);
    } else if (autoload_args.no->count) {
        freq_nvs_autoboot_save(false);
    } else {
        uint32_t autoload;
        ESP_ERROR_CHECK( freq_nvs_autoboot_load(&autoload) );
        printf("Autoload at boot time is currently %s.\n", (autoload ? "enabled" : "disabled"));
    }
    
    return 0;
}



/* ************************************************************************* */
/*                               API FUNCTIONS                               */
/* ************************************************************************* */

void freq_cmds_register()
{
    esp_console_register_help_command();
    register_params();
    register_create();
    register_start();
    register_stop();
    register_delete();
    register_list();
    register_save();
    register_load();
    register_autoload();
    autoload_at_boot();
    printf("Try 'help' to check all supported commands\n");
}

