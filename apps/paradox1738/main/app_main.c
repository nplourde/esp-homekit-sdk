#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include <hap_fw_upgrade.h>

#include <iot_button.h>

#include <app_wifi.h>
#include <app_hap_setup_payload.h>

#include "paradox1738.h"

/* Comment out the below line to disable Firmware Upgrades */
#define CONFIG_FIRMWARE_SERVICE

static const char *TAG = "HAP motionsensor";

#define MOTIONSENSOR_TASK_PRIORITY  1
#define MOTIONSENSOR_TASK_STACKSIZE 4 * 1024
#define MOTIONSENSOR_TASK_NAME      "hap_motionsensor"

/* Reset network credentials if button is pressed for more than 3 seconds and then released */
#define RESET_NETWORK_BUTTON_TIMEOUT        3

/* Reset to factory if button is pressed and held for more than 10 seconds */
#define RESET_TO_FACTORY_BUTTON_TIMEOUT     10

/* The button "Boot" will be used as the Reset button for the example */
#define RESET_GPIO  GPIO_NUM_0
/**
 * @brief The network reset button callback handler.
 * Useful for testing the Wi-Fi re-configuration feature of WAC2
 */
static void reset_network_handler(void* arg)
{
    hap_reset_network();
}
/**
 * @brief The factory reset button callback handler.
 */
static void reset_to_factory_handler(void* arg)
{
    hap_reset_to_factory();
}

/**
 * The Reset button  GPIO initialisation function.
 * Same button will be used for resetting Wi-Fi network as well as for reset to factory based on
 * the time for which the button is pressed.
 */
static void reset_key_init(uint32_t key_gpio_pin)
{
    button_handle_t handle = iot_button_create(key_gpio_pin, BUTTON_ACTIVE_LOW);
    iot_button_add_on_release_cb(handle, RESET_NETWORK_BUTTON_TIMEOUT, reset_network_handler, NULL);
    iot_button_add_on_press_cb(handle, RESET_TO_FACTORY_BUTTON_TIMEOUT, reset_to_factory_handler, NULL);
}

/* Mandatory identify routine for the accessory.
 * In a real accessory, something like LED blink should be implemented
 * got visual identification
 */
static int sensor_identify(hap_acc_t *ha)
{
    ESP_LOGI(TAG, "Accessory identified");
    return HAP_SUCCESS;
}

/*The main thread for handling the Accessory */
static void motionsensor_thread_entry(void *arg)
{
    hap_acc_t *accessory;
    hap_serv_t *service;

    /* Initialize the HAP core */
    hap_init(HAP_TRANSPORT_WIFI);

    /* Initialise the mandatory parameters for Accessory which will be added as
     * the mandatory services internally
     */
    hap_acc_cfg_t cfg = {
        .name = "Esp-MotionSensor",
        .manufacturer = "Espressif",
        .model = "EspMotionSensor01",
        .serial_num = "abcdefg",
        .fw_rev = "0.9.0",
        .hw_rev = "1.0",
        .pv = "1.1.0",
        .identify_routine = sensor_identify,
        .cid = HAP_CID_SENSOR,
    };

    /* Create accessory object */
    accessory = hap_acc_create(&cfg);
    if (!accessory)
    {
        ESP_LOGE(TAG, "Failed to create accessory");
        goto sensor_err;
    }

    /* Add a dummy Product Data */
    uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};
    hap_acc_add_product_data(accessory, product_data, sizeof(product_data));

    /* Add Wi-Fi Transport service required for HAP Spec R16 */
    hap_acc_add_wifi_transport_service(accessory, 0);

    /* Create the Motion Sensor Service. Include the "name" since this is a user visible service  */
    service = hap_serv_motion_sensor_create(false);
    if (!service)
    {
        ESP_LOGE(TAG, "Failed to create Motion Sensor Service");
        goto sensor_err;
    }

    /* Add the optional characteristic to the Sensor Service */
    int ret = hap_serv_add_char(service, hap_char_name_create("ESP32 Motion Sensor"));
    
    if (ret != HAP_SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to add optional characteristics to Motion Sensor");
        goto sensor_err;
    }
    
    /* Add the Service to the Accessory Object */
    hap_acc_add_serv(accessory, service);

#ifdef CONFIG_FIRMWARE_SERVICE
    hap_serv_t *firmware_service;
    /*  Required for server verification during OTA, PEM format as string  */
    static char server_cert[] = {};
    hap_fw_upgrade_config_t ota_config = {
        .server_cert_pem = server_cert,
    };
    /* Create and add the Firmware Upgrade Service, if enabled.
     * Please refer the FW Upgrade documentation under components/homekit/extras/include/hap_fw_upgrade.h
     * and the top level README for more information.
     */
    firmware_service = hap_serv_fw_upgrade_create(&ota_config);
    if (!firmware_service) {
        ESP_LOGE(TAG, "Failed to create Firmware Upgrade Service");
        goto sensor_err;
    }
    hap_acc_add_serv(accessory, firmware_service);
#endif

    /* Add the Accessory to the HomeKit Database */
    hap_add_accessory(accessory);

    /* Initialize the Paradox1738 Hardware */
    paradox1738_init();

    /* Register a common button for reset Wi-Fi network and reset to factory.
     */
    reset_key_init(RESET_GPIO);

    /* TODO: Do the actual hardware initialization here */

    /* For production accessories, the setup code shouldn't be programmed on to
     * the device. Instead, the setup info, derived from the setup code must
     * be used. Use the factory_nvs_gen utility to generate this data and then
     * flash it into the factory NVS partition.
     *
     * By default, the setup ID and setup info will be read from the factory_nvs
     * Flash partition and so, is not required to set here explicitly.
     *
     * However, for testing purpose, this can be overridden by using hap_set_setup_code()
     * and hap_set_setup_id() APIs, as has been done here.
     */
#ifdef CONFIG_EXAMPLE_USE_HARDCODED_SETUP_CODE
    /* Unique Setup code of the format xxx-xx-xxx. Default: 111-22-333 */
    hap_set_setup_code(CONFIG_EXAMPLE_SETUP_CODE);
    /* Unique four character Setup Id. Default: ES32 */
    hap_set_setup_id(CONFIG_EXAMPLE_SETUP_ID);
#ifdef CONFIG_APP_WIFI_USE_WAC_PROVISIONING
    app_hap_setup_payload(CONFIG_EXAMPLE_SETUP_CODE, CONFIG_EXAMPLE_SETUP_ID, true, cfg.cid);
#else
    app_hap_setup_payload(CONFIG_EXAMPLE_SETUP_CODE, CONFIG_EXAMPLE_SETUP_ID, false, cfg.cid);
#endif
#endif

    /* Enable Hardware MFi authentication (applicable only for MFi variant of SDK) */
    hap_enable_mfi_auth(HAP_MFI_AUTH_HW);

    /* Initialize Wi-Fi */
    app_wifi_init();

    /* After all the initializations are done, start the HAP core */
    hap_start();
    /* Start Wi-Fi */
    app_wifi_start(portMAX_DELAY);

    /* Get motion characteristic iid */
    uint32_t motion_sensor_iid = 0;
    hap_char_t *hc;
    for (hc = hap_serv_get_first_char(service); hc; hc = hap_char_get_next(hc))
    {
        if (!strcmp(hap_char_get_type_uuid(hc), HAP_CHAR_UUID_MOTION_DETECTED))
        {
            motion_sensor_iid = hap_char_get_iid(hc);
            continue;
        }
    }

    if(motion_sensor_iid != 0)
    {
        hap_char_t *hc_motion_sensor = hap_acc_get_char_by_iid(accessory, motion_sensor_iid);

        paradox1738_event_t event;

        while (1)
        {
            hap_val_t new_val;
            
            if(paradox1738_event_read(&event))
            {
                ESP_LOGI(TAG, "Recv event: %d %d", event.EventId, event.CategoryId);
            
                switch(event.EventId)
                {        
                    case 0:
                    new_val.i = 0;
                    break;
                    
                    case 1:
                    new_val.i = 1;
                    break;

                    default:
                    break;
                }            

                hap_char_update_val(hc_motion_sensor, &new_val);
            }
        }      
    }
    else
    {
        goto sensor_err;
    }

    /* The task ends here. The read/write callbacks will be invoked by the HAP Framework */
    vTaskDelete(NULL);

sensor_err:
    hap_acc_delete(accessory);
    vTaskDelete(NULL);
}

void app_main()
{
    xTaskCreate(motionsensor_thread_entry, MOTIONSENSOR_TASK_NAME, MOTIONSENSOR_TASK_STACKSIZE, NULL, MOTIONSENSOR_TASK_PRIORITY, NULL);
}
