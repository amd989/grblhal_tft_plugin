/*
 * arduino_spi.cpp - Arduino SPIClass for TFT_eSPI, using the ESP-IDF spi_master driver
 *
 * Part of grblHAL TFT Plugin
 *
 * TFT_eSPI falls back to its Generic processor on ESP-IDF and sends every byte
 * through SPI.transfer()/transfer16(), so this is what actually drives the display
 * and the XPT2046 touch controller. Chip selects are toggled by TFT_eSPI itself.
 */

#include "User_Setup.h"
#include "arduino_compat.h"
#include "tft_config.h"

#if TFT_ENABLE

// The MKS DLC32 LCD header is wired to the VSPI IO_MUX pins (18/19/23).
#define TFT_SPI_HOST SPI3_HOST  // VSPI

// Largest single DMA transfer; writeBytes() splits longer blocks.
#define TFT_SPI_MAX_TRANSFER 8192

SPIClass SPI;

void SPIClass::begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss)
{
    (void)ss;

    if(dev)
        return;

    spi_bus_config_t bus = {};
    bus.sclk_io_num = sck >= 0 ? sck : TFT_SCLK;
    bus.miso_io_num = miso >= 0 ? miso : TFT_MISO;
    bus.mosi_io_num = mosi >= 0 ? mosi : TFT_MOSI;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = TFT_SPI_MAX_TRANSFER;

    // DMA for writeBytes(); single bytes and words still go via tx_data.
    // Channel 2: grblHAL asks for channel 1 for the SD card (spi.c).
    if(spi_bus_initialize(TFT_SPI_HOST, &bus, SPI_DMA_CH2) != ESP_OK)
        return;

    if(freq_hz == 0)
        freq_hz = SPI_FREQUENCY;

    add_device();
}

void SPIClass::end(void)
{
    if(dev) {
        spi_device_release_bus(dev);
        spi_bus_remove_device(dev);
        dev = NULL;
    }
    spi_bus_free(TFT_SPI_HOST);
}

bool SPIClass::add_device(void)
{
    spi_device_interface_config_t cfg = {};
    cfg.mode = 0;
    cfg.clock_speed_hz = (int)freq_hz;
    cfg.spics_io_num = -1;              // TFT_eSPI drives TFT_CS and TOUCH_CS itself
    cfg.queue_size = 1;
    cfg.flags = SPI_DEVICE_NO_DUMMY;    // allow 40 MHz full duplex writes, reads run slower

    if(spi_bus_add_device(TFT_SPI_HOST, &cfg, &dev) != ESP_OK) {
        dev = NULL;
        return false;
    }

    // Only user of this bus, so keep it acquired: polling transfers skip the bus lock.
    spi_device_acquire_bus(dev, portMAX_DELAY);

    return true;
}

void SPIClass::setFrequency(uint32_t freq)
{
    if(freq == freq_hz)
        return;

    freq_hz = freq;

    if(dev) {
        spi_device_release_bus(dev);
        spi_bus_remove_device(dev);
        dev = NULL;
        add_device();
    }
}

// Write a block (write only). data must be DMA capable, i.e. in internal RAM.
void SPIClass::writeBytes(const uint8_t *data, uint32_t len)
{
    if(!dev)
        return;

    while(len) {
        uint32_t chunk = len > TFT_SPI_MAX_TRANSFER ? TFT_SPI_MAX_TRANSFER : len;
        spi_transaction_t t = {};
        t.length = chunk * 8;
        t.tx_buffer = data;
        spi_device_polling_transmit(dev, &t);
        data += chunk;
        len -= chunk;
    }
}

uint8_t SPIClass::transfer(uint8_t data)
{
    if(!dev)
        return 0;

    spi_transaction_t t = {};
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 8;
    t.tx_data[0] = data;
    spi_device_polling_transmit(dev, &t);

    return t.rx_data[0];
}

uint16_t SPIClass::transfer16(uint16_t data)
{
    if(!dev)
        return 0;

    spi_transaction_t t = {};
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 16;
    t.tx_data[0] = (uint8_t)(data >> 8);    // MSB first
    t.tx_data[1] = (uint8_t)data;
    spi_device_polling_transmit(dev, &t);

    return ((uint16_t)t.rx_data[0] << 8) | t.rx_data[1];
}

#endif // TFT_ENABLE
