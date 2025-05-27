#include "ti_board_open_close.h"
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include <drivers/adc.h>
#include <drivers/gpio.h>
#include <drivers/hw_include/tistdtypes.h>
#include <drivers/mcspi.h>
#include <kernel/dpl/AddrTranslateP.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/DebugP.h>
#include <math.h>
#include <stdio.h>

#define SLEEP_TIME 1000 * 200

#define ADS7953_CMD(channel)                                                   \
  (0x1000 | ((channel & 0x0F)                                                  \
             << 7)) // MACRO for generating a command for ADC in Manual Mode

static uint16_t readADC(uint8_t adc_number);
static float adc_to_voltage(uint16_t raw_adc);
static float adc_to_current_ch0(uint16_t raw_adc);
static float adc_to_current_ch1(uint16_t raw_adc);
static void read_values(float *ch0, float *ch1, bool *gpio44_value, bool *gpio46_value);
uint16_t SPI_ReadWrite(uint16_t data);

static void test_high_side_switches();
static bool check_test_values(float ch0, float ch1, bool gpio44_value,
                              bool gpio46_value, float ch0_expected_value,
                              float ch1_expected_value,
                              bool gpio44_expected_value,
                              bool gpio46_expected_value);

static const float VREF = 2.5f;           // Reference ADC Voltage
static const float ADC_RESOLUTION = 4095.0f; // 12 bit resolution
static const float CURRENT_SCALE_CH0 =
    0.108f; // Voltage to Current Conversion factor CH0
static const float CURRENT_SCALE_CH1 =
    0.383f; // Voltage to Current Conversion factor CH1
static const float DISCREPANCY = 0.1;

MCSPI_Handle gSpiHandle = NULL;

void high_side_switches_test(void *args) {

  Drivers_open();
  Board_driversOpen();

  gSpiHandle = gMcspiHandle[SPI0];

  GPIO_setDirMode(SPI0_CS_BASE_ADDR, SPI0_CS_PIN, SPI0_CS_DIR);
  DebugP_log("SPI_CS0 configured as output\r\n");

  GPIO_setDirMode(GPIO43_BASE_ADDR, GPIO43_PIN, GPIO43_DIR);
  DebugP_log("GPIO43 configured as output\r\n");

  GPIO_setDirMode(GPIO44_BASE_ADDR, GPIO44_PIN, GPIO44_DIR);
  DebugP_log("GPIO44 configured as input\r\n");

  GPIO_setDirMode(GPIO45_BASE_ADDR, GPIO45_PIN, GPIO45_DIR);
  DebugP_log("GPIO45 configured as output\r\n");

  GPIO_setDirMode(GPIO46_BASE_ADDR, GPIO46_PIN, GPIO46_DIR);
  DebugP_log("GPIO46 configured as input\r\n");

  // GPIO_pinWriteHigh(GPIO43_BASE_ADDR, GPIO43_PIN);

  // while(true)
  // {
  //   DebugP_log("CH0: %d\r\n", readADC(0));
  //   DebugP_log("CH1: %f\r\n", adc_to_voltage(readADC(1)));

  //   DebugP_log("CH1: %f\r\n", adc_to_current_ch1(readADC(1)));
  // }
  

  test_high_side_switches();

  Board_driversClose();
  Drivers_close();
}

void test_high_side_switches() {
  float ch0, ch1;
  bool gpio44_value, gpio46_value;

  DebugP_log("Starting high side switches test\r\n");

  // Step 1
  DebugP_log("Starting test Step 1\r\n");
  GPIO_pinWriteLow(GPIO43_BASE_ADDR, GPIO43_PIN);
  GPIO_pinWriteLow(GPIO45_BASE_ADDR, GPIO45_PIN);

  ClockP_usleep(SLEEP_TIME);
  read_values(&ch0, &ch1, &gpio44_value, &gpio46_value);

  DebugP_log("CH0: %fA, CH1: %fA, GPIO44: %d, GPIO46: %d\r\n", ch0, ch1,
             gpio44_value, gpio46_value);
  DebugP_log("Step 1: %d\r\n", check_test_values(ch0, ch1, gpio44_value,
                                                 gpio46_value, 0, 0, 1, 1));

  // Step 2
  DebugP_log("Starting test Step 2\r\n");
  GPIO_pinWriteHigh(GPIO43_BASE_ADDR, GPIO43_PIN);

  ClockP_usleep(SLEEP_TIME);

  read_values(&ch0, &ch1, &gpio44_value, &gpio46_value);

  DebugP_log("CH0: %fA, CH1: %fA, GPIO44: %d, GPIO46: %d\r\n", ch0, ch1,
             gpio44_value, gpio46_value);
  DebugP_log("Step 2: %d\r\n", check_test_values(ch0, ch1, gpio44_value,
                                                 gpio46_value, 0, 1, 1, 1));

  // Step 3
  DebugP_log("Starting test Step 3\r\n");
  GPIO_pinWriteHigh(GPIO43_BASE_ADDR, GPIO43_PIN);
  GPIO_pinWriteHigh(GPIO45_BASE_ADDR, GPIO45_PIN);

  ClockP_usleep(SLEEP_TIME);
  read_values(&ch0, &ch1, &gpio44_value, &gpio46_value);

  DebugP_log("CH0: %fA, CH1: %fA, GPIO44: %d, GPIO46: %d\r\n", ch0, ch1,
             gpio44_value, gpio46_value);
  DebugP_log("Step 3: %d\r\n", check_test_values(ch0, ch1, gpio44_value,
                                                 gpio46_value, 1, 1, 1, 1));

  // Step 4
  for (uint8_t i = 1; i <= 5; i++)
  {
    DebugP_log("Starting test Step 4.%d\r\n", i);
    GPIO_pinWriteHigh(GPIO43_BASE_ADDR, GPIO43_PIN);
    GPIO_pinWriteHigh(GPIO45_BASE_ADDR, GPIO45_PIN);

    ClockP_usleep(SLEEP_TIME);
    read_values(&ch0, &ch1, &gpio44_value, &gpio46_value);

    DebugP_log("CH0: %fA, CH1: %fA, GPIO44: %d, GPIO46: %d\r\n", ch0, ch1,
                gpio44_value, gpio46_value);
    DebugP_log("Step 4.%d: %d\r\n", i, check_test_values(ch0, ch1, gpio44_value,
                                                    gpio46_value, 1, 1, 1, 1)); // Read the corresponding values from ADC
  }

  // Step 5
  DebugP_log("Starting test Step 5\r\n");
  GPIO_pinWriteHigh(GPIO43_BASE_ADDR, GPIO43_PIN);
  GPIO_pinWriteHigh(GPIO45_BASE_ADDR, GPIO45_PIN);

  ClockP_usleep(SLEEP_TIME);
  read_values(&ch0, &ch1, &gpio44_value, &gpio46_value);

  DebugP_log("CH0: %fA, CH1: %fA, GPIO44: %d, GPIO46: %d\r\n", ch0, ch1,
             gpio44_value, gpio46_value);
  DebugP_log("Step 5: %d\r\n", check_test_values(ch0, ch1 * CURRENT_SCALE_CH1, gpio44_value,
                                                 gpio46_value, 6.5, 3.3, 0, 1)); //3.3V from ch1

  // Step 6
  DebugP_log("Starting test Step 6\r\n");
  GPIO_pinWriteHigh(GPIO43_BASE_ADDR, GPIO43_PIN);
  GPIO_pinWriteHigh(GPIO45_BASE_ADDR, GPIO45_PIN);

  ClockP_usleep(SLEEP_TIME);
  read_values(&ch0, &ch1, &gpio44_value, &gpio46_value);

  DebugP_log("CH0: %fV, CH1: %fA, GPIO44: %d, GPIO46: %d\r\n", ch0, ch1,
             gpio44_value, gpio46_value);
  DebugP_log("Step 6: %d\r\n", check_test_values(ch0, ch1, gpio44_value,
                                                 gpio46_value, 1, 1, 1, 1)); 

  // Step 7
  DebugP_log("Starting test Step 7\r\n");

  ClockP_usleep(SLEEP_TIME);
  read_values(&ch0, &ch1, &gpio44_value, &gpio46_value);

  DebugP_log("CH0: %fA, CH1: %fA, GPIO44: %d, GPIO46: %d\r\n", ch0, ch1,
             gpio44_value, gpio46_value);
  DebugP_log("Step 5: %d\r\n", check_test_values(ch0 * CURRENT_SCALE_CH0, ch1, gpio44_value,
                                                 gpio46_value, 3.3, 1, 1, 0)); //3.3V from ch0
  
  // Step 8
  DebugP_log("Starting test Step 8\r\n");
  GPIO_pinWriteHigh(GPIO43_BASE_ADDR, GPIO43_PIN);
  GPIO_pinWriteHigh(GPIO45_BASE_ADDR, GPIO45_PIN);

  ClockP_usleep(SLEEP_TIME);
  read_values(&ch0, &ch1, &gpio44_value, &gpio46_value);

  DebugP_log("CH0: %fA, CH1: %fA, GPIO44: %d, GPIO46: %d\r\n", ch0, ch1,
             gpio44_value, gpio46_value);
  DebugP_log("Step 8: %d\r\n", check_test_values(ch0, ch1, gpio44_value,
                                                 gpio46_value, 1, 1, 1, 1)); 
}

uint16_t readADC(uint8_t channel) {
  uint16_t command = ADS7953_CMD(channel);

  SPI_ReadWrite(command); // Since response comes only in the third package,
                          // doing 2 empty reads

  SPI_ReadWrite(command);

  return SPI_ReadWrite(command); // here reading actual data
}

float adc_to_voltage(uint16_t raw_adc) {
  return ((float)raw_adc / ADC_RESOLUTION) * VREF;
}

float adc_to_current_ch0(uint16_t voltage) {
  return adc_to_voltage((float)voltage) / CURRENT_SCALE_CH0;
}

float adc_to_current_ch1(uint16_t voltage) {
  return adc_to_voltage((float)voltage) / CURRENT_SCALE_CH1;
}

void read_values(float *ch0, float *ch1, bool *gpio44_value,
                 bool *gpio46_value) {
  *ch0 = adc_to_current_ch0(readADC(0));
  *ch1 = adc_to_current_ch1(readADC(1));
  *gpio44_value = GPIO_pinRead(GPIO44_BASE_ADDR, GPIO44_PIN);
  *gpio46_value = GPIO_pinRead(GPIO46_BASE_ADDR, GPIO46_PIN);
}

// Pass [amps] as ch0 and ch1 here
bool check_test_values(float ch0, float ch1, bool gpio44_value,
                       bool gpio46_value, float ch0_expected_value,
                       float ch1_expected_value, bool gpio44_expected_value,
                       bool gpio46_expected_value) {

  if (fabs(ch1 - ch1_expected_value) > DISCREPANCY) {
    DebugP_log("Unexpected value in ADC_CH1\r\n");
    return false;
  }

  if (fabs(ch0 - ch0_expected_value) > DISCREPANCY) {
    DebugP_log("Unexpected value in ADC_CH0\r\n");
    return false;
  }

  if (gpio44_value != gpio44_expected_value) {
    DebugP_log("Unexpected value in GPIO44\r\n");
    return false;
  }

  if (gpio46_value != gpio46_expected_value) {
    DebugP_log("Unexpected value in GPIO46\r\n");
    return false;
  }

  return true;
}

uint16_t SPI_ReadWrite(uint16_t data) {
  MCSPI_Transaction spiTransaction;
  uint16_t txData = data;
  uint16_t rxData = 0;

  MCSPI_Transaction_init(&spiTransaction);
  spiTransaction.channel = gSpi0ChCfg[0].chNum;
  spiTransaction.dataSize = 16;    // 16-bit for ADS7953
  spiTransaction.csDisable = true; // Manual Control CS
  spiTransaction.count = 1;
  spiTransaction.txBuf = (void *)&txData;
  spiTransaction.rxBuf = (void *)&rxData;
  spiTransaction.args = NULL;

  GPIO_pinWriteLow(SPI0_CS_BASE_ADDR, SPI0_CS_PIN);

  if (MCSPI_transfer(gSpiHandle, &spiTransaction) != SystemP_SUCCESS) {
    DebugP_log("SPI transfer failed!\r\n");
    return 0xFFFF; // Error value
  }

  GPIO_pinWriteHigh(SPI0_CS_BASE_ADDR, SPI0_CS_PIN);

  return rxData & 0x0FFF; // Getting the value from the package
}