/******************************************************************************
 * Author: Ryan Freas
 * Project: Interfacing your ESP32 and CC1101
 *
 * Purpose:
 * A program to confirm your ESP32 and CC1101 device are communicating properly
 * 
 ******************************************************************************/

#include "esp_log.h"
#include <cstdint>
#include <string>

extern "C" {
#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}
// Register values from the CC1101 datasheet
constexpr uint8_t CC1101_STROBE_SRES = 0x30;
constexpr uint8_t CC1101_STROBE_SIDLE = 0x36;
constexpr uint8_t CC1101_STROBE_SFTX = 0x3B;

constexpr uint8_t CC1101_STATUS_PARTNUM = 0xF0;
constexpr uint8_t CC1101_STATUS_VERSION = 0xF1;

constexpr uint8_t CC1101_DUMMY_BYTE = 0x00;

void transmit_data(spi_device_handle_t cc1101, const uint8_t* data, size_t len,  const std::string& operation) {
    spi_transaction_t t = {};
    uint8_t rx[len];
    t.tx_buffer = data;
    t.rx_buffer = rx; // rx[0] will always be the Chip Status Byte (section 10.1 of the datasheet)
    t.length = len * 8;
    ESP_ERROR_CHECK(spi_device_polling_transmit(cc1101, &t));
    ESP_LOGI("CC1101", "Operation: %s", operation.c_str());
    for (size_t i = 0; i < len; ++i) {
        ESP_LOGI("CC1101_RX", "rx[%zu] = 0x%02X", i, rx[i]);
    }
};

void strobe_reset(spi_device_handle_t cc1101) {
    // Reset chip
    transmit_data(
        cc1101,
        (uint8_t[]){CC1101_STROBE_SRES},
        1,
        "SRES"
    );
    // Put CC1101 in idle mode
    transmit_data(
        cc1101,
        (uint8_t[]){CC1101_STROBE_SIDLE},
        1,
        "SIDLE"
    );
    // Flush transmit buffer: in order to send the SFTX strobe, CC1101 must be in certain states (like idle mode)
    transmit_data(
        cc1101,
        (uint8_t[]){CC1101_STROBE_SFTX},
        1,
        "SFTX"
    );
};
// To write c++ in the ESP-IDF, we have to include extern "C"
extern "C" void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI("MAIN", "Hello World...?");

    // ===================== CONFIGURE BUS SECTION ========================= //
    spi_bus_config_t busConfig = {};
    busConfig.mosi_io_num = GPIO_NUM_23;
    busConfig.miso_io_num = GPIO_NUM_19;
    busConfig.sclk_io_num = GPIO_NUM_18;
    busConfig.quadwp_io_num = -1; 
    busConfig.quadhd_io_num = -1;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &busConfig, SPI_DMA_DISABLED));
    // ===================== END CONFIGURE BUS SECTION ===================== //

    // ===================== CONFIGURE DEVICE SECTION ====================== //
    spi_device_interface_config_t deviceConfig = {};
    spi_device_handle_t cc1101; 
    deviceConfig.command_bits = 0; 
    deviceConfig.address_bits = 0;
    deviceConfig.dummy_bits = 0;
    deviceConfig.mode = 0;
    deviceConfig.clock_speed_hz = 1000000; // 1 MHz
    deviceConfig.spics_io_num = GPIO_NUM_5; // Chip Select Pin
    deviceConfig.queue_size = 1; // Program doesn't queue anything, only synchronous methods used
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &deviceConfig, &cc1101));
    // =================== END CONFIGURE DEVICE SECTION ===================== //

    // ======================= TRANSACTION SECTION ========================== //
    // We must reset the system when the power supply is turned on (see datasheet: 19.1 Power-On Start-Up Sequence)
    strobe_reset(cc1101);
    // Retrieve the PARTNUM register value
    transmit_data(
        cc1101,
        (uint8_t[]){CC1101_STATUS_PARTNUM, CC1101_DUMMY_BYTE},
        2, // How many bytes we are sending
        "PARTNUM" 
    );
    // Retrieve the VERSION register value
    transmit_data(
        cc1101,
        (uint8_t[]){CC1101_STATUS_VERSION, CC1101_DUMMY_BYTE},
        2,
        "VERSION"
    );
    // ====================== END TRANSACTION SECTION ====================== //
}