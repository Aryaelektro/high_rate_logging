#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <time.h>
#include "esp_sleep.h"

#define EXAMPLE_MAX_CHAR_SIZE 64
static const char *TAG = "ADC to SD Card";

#define ADC_PIN1 ADC1_CHANNEL_0 // Replace with your ADC input pin
#define ADC_PIN2 ADC1_CHANNEL_4 // Replace with your ADC input pin

#define MOUNT_POINT "/sdcard"

#define PIN_NUM_MISO 10 // CONFIG_EXAMPLE_PIN_MISO
#define PIN_NUM_MOSI 3  // CONFIG_EXAMPLE_PIN_MOSI
#define PIN_NUM_CLK 2   // CONFIG_EXAMPLE_PIN_CLK
#define PIN_NUM_CS 7    // CONFIG_EXAMPLE_PIN_CS

#define LED_PIN 13
#define POWER_PIN 12

void app_main()
{
    gpio_set_direction(POWER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(POWER_PIN, 1);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(LED_PIN, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    // Initialize SD card and mount it--------------------------------------------------------------------------
    ESP_LOGI(TAG, "Initializing SD card...");
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    //-------------------------------------------------------------------------------------------------------
    // Initialize ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_PIN1, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_PIN2, ADC_ATTEN_DB_11);

    ESP_LOGI(TAG, "ADC initialized");

    //Create and open a file on the SD card for logging
    char filename[32];
    snprintf(filename, sizeof(filename), "%s/data.txt", MOUNT_POINT);
    FILE *f = fopen(filename, "w");

    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    
    // Set up timing for 8000 data points per second
    TickType_t delay_ticks = pdMS_TO_TICKS(1000 / 8000); // Delay for 125 microseconds between readings
    

    // Read and log analog data to the SD card
    ESP_LOGI(TAG, "start logging adc4");


    for (int i = 80000; i >= 0; i--)
    {
        // uint16_t adc_value1 = adc1_get_raw(ADC_PIN1); // must in the loop. to update readval
        uint16_t adc_value2 = adc1_get_raw(ADC_PIN2);
        // printf("ADC Value: %ld", adc_value);
        // printf("\n");
        fprintf(f, "%d\n",  adc_value2);
        // fflush(f);                              // Flush the buffer to ensure data is written immediately
        // vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for the desired interval
        vTaskDelay(delay_ticks);
    }

    // Close the file (this won't be reached in this example)
    // fclose(f);
    ESP_LOGI(TAG, "logging finished");
    fclose(f);
    gpio_set_level(POWER_PIN, 0);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    const int wakeup_time_sec = 30;
    printf("Enabling timer wakeup, %ds\n", wakeup_time_sec);
    esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);

    for (int i = 2; i >= 0; i--)
    {
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }

    printf("entering Deepsleep");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_deep_sleep_start();
}
