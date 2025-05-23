/*
 *  Copyright (C) 2021 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_board_open_close.h"
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include <drivers/adc.h>
#include <drivers/gpio.h>
#include <kernel/dpl/AddrTranslateP.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/DebugP.h>
#include <math.h>
#include <stdio.h>

uint16_t readADC(uint8_t adc_number);
float adc_to_voltage(uint16_t raw_adc);
float adc_to_current(uint16_t raw_adc);
void read_values(float &ain0, float &ain0, bool &gpio44_value);

void test_ideal_diodes();
bool check_test_values(float ain0, float ain1, bool gpio44_value,
                       float ain0_expected_value, float ain1_expected_value,
                       bool gpio44_expected_value);

const float VREF = 3.3f;            // Reference ADC Voltage
const int ADC_RESOLUTION = 4095;    // 12 bit resolution
const float CURRENT_SCALE = 0.432f; // Voltage to Current Conversion factor
const float DISCREPANCY = 0.1;

void empty_main(void *args) {

  Drivers_open();
  Board_driversOpen();

  ADC_enableConverter(ADC1_BASE_ADDR);

  DebugP_log("Hello World!\r\n");

  GPIO_setDirMode(GPIO43_BASE_ADDR, GPIO43_PIN, GPIO43_DIR);
  DebugP_log("GPIO43 configured as output\r\n");

  GPIO_setDirMode(GPIO44_BASE_ADDR, GPIO44_PIN, GPIO44_DIR);
  DebugP_log("GPIO44 configured as input\r\n");

  while (true) {
    DebugP_log("ADC Result register value : %d\r\n", readADC(0));
    ClockP_sleep(1);
  }

  ADC_disableInterrupt(ADC1_BASE_ADDR, ADC_INT_NUMBER1);
  ADC_clearInterruptStatus(ADC1_BASE_ADDR, ADC_INT_NUMBER1);
  /* Power down the ADC */
  ADC_disableConverter(ADC1_BASE_ADDR);

  Board_driversClose();
  Drivers_close();
}

uint16_t readADC(uint8_t adc_number) {
  ADC_clearInterruptStatus(ADC1_BASE_ADDR, ADC_INT_NUMBER1);
  ADC_clearInterruptStatus(ADC1_BASE_ADDR, ADC_INT_NUMBER2);

  uint16_t conversion_result;

  switch (adc_number) {
  case 0:
    ADC_forceSOC(ADC1_BASE_ADDR, ADC_SOC_NUMBER0);
    while (ADC_getInterruptStatus(ADC1_BASE_ADDR, ADC_INT_NUMBER1) == false) {
      /* Wait for the SOC conversion to complete */
    }
    conversion_result = ADC_readResult(ADC1_RESULT_BASE_ADDR, ADC_SOC_NUMBER0);
    break;

  case 1:
    ADC_forceSOC(ADC1_BASE_ADDR, ADC_SOC_NUMBER1);
    while (ADC_getInterruptStatus(ADC1_BASE_ADDR, ADC_INT_NUMBER2) == false) {
      /* Wait for the SOC conversion to complete */
    }
    conversion_result = ADC_readResult(ADC1_RESULT_BASE_ADDR, ADC_SOC_NUMBER1);
    break;
  }

  ADC_clearInterruptStatus(ADC1_BASE_ADDR, ADC_INT_NUMBER1);
  ADC_clearInterruptStatus(ADC1_BASE_ADDR, ADC_INT_NUMBER2);

  return conversion_result;
}

void test_ideal_diodes() {
  float ain0, ain1;
  bool gpio44_value;

  DebugP_log("Starting ideal diode test");

  // Step 1
  DebugP_log("Starting test Step 1");
  GPIO_pinWriteLow(GPIO43_BASE_ADDR, GPIO43_PIN);

  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 1: %d",
             check_test_values(ain0, ain1, gpio44_value, 0, 0.45, 1));

  // Step 2
  DebugP_log("Starting test Step 2");
  GPIO_pinWriteHigh(GPIO43_BASE_ADDR, GPIO43_PIN);

  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 2: %d",
             check_test_values(ain0, ain1, gpio44_value, 2, 0.45, 1));

  // Step 3
  DebugP_log("Starting test Step 3");

  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 3.1: %d",
             check_test_values(ain0, ain1, gpio44_value, 1, 0.45, 1));
  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 3.2: %d",
             check_test_values(ain0, ain1, gpio44_value, 1.5, 0.45, 1));
  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 3.3: %d",
             check_test_values(ain0, ain1, gpio44_value, 3, 0.45, 1));
  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 3.4: %d",
             check_test_values(ain0, ain1, gpio44_value, 5.5, 0.45, 1));
  read_values(ain0, ain1, gpio44_value);

  // Step 4
  DebugP_log("Starting test Step 4");
  GPIO_pinWriteHigh(GPIO43_BASE_ADDR, GPIO43_PIN);

  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 4: %d",
             check_test_values(ain0, ain1, gpio44_value, 2, 0.45, 1));

  // Step 5
  DebugP_log("Starting test Step 5");
  // Set PSU to 12V

  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 5: %d",
             check_test_values(ain0, ain1, gpio44_value, 2, 0.22, 1));

  // Step 6
  DebugP_log("Starting test Step 6");
  // Set PSU to 12V

  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 6: %d",
             check_test_values(ain0, ain1, gpio44_value, 0, 0.11, 1));

  // Step 7
  DebugP_log("Starting test Step 7");
  GPIO_pinWriteHigh(GPIO43_BASE_ADDR, GPIO43_PIN);

  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 7: %d",
             check_test_values(ain0, ain1, gpio44_value, 2, 0.45, 1));

  // Step 8
  DebugP_log("Starting test Step 8");
  // Set PSU to 36V

  read_values(ain0, ain1, gpio44_value);

  // AIN0 could be oscillating
  DebugP_log("Step 8: %d",
             check_test_values(ain0, ain1, gpio44_value, 0, 0.68, 0));

  // Step 9
  DebugP_log("Starting test Step 9");
  GPIO_pinWriteHigh(GPIO43_BASE_ADDR, GPIO43_PIN);

  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 9: %d",
             check_test_values(ain0, ain1, gpio44_value, 2, 0.45, 1));

  // Step 10
  DebugP_log("Starting test Step 10");
  // Set Rload to short circuit

  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 10: %d",
             check_test_values(ain0, ain1, gpio44_value, 0, 0.45, 0));

  // Step 11
  DebugP_log("Starting test Step 11");
  GPIO_pinWriteHigh(GPIO43_BASE_ADDR, GPIO43_PIN);

  read_values(ain0, ain1, gpio44_value);

  DebugP_log("Step 11: %d",
             check_test_values(ain0, ain1, gpio44_value, 2, 0.45, 1));

}

float adc_to_voltage(uint16_t raw_adc) {
  return (raw_adc / ADC_RESOLUTION) * VREF;
}

float adc_to_current(uint16_t raw_adc) {
  return adc_to_voltage(raw_adc) / CURRENT_SCALE;
}

void read_values(float &ain0, float &ain0, bool &gpio44_value) {
  ain1 = readADC(1);
  ain0 = readADC(0);
  gpio44_value = GPIO_pinRead(GPIO44_BASE_ADDR, GPIO44_PIN);
}

bool check_test_values(float ain0, float ain1, bool gpio44_value,
                       float ain0_expected_value, float ain1_expected_value,
                       bool gpio44_expected_value) {
  if (fabs(adc_to_voltage(ain1) - ain1_expected_value) > DISCREPANCY) {
    DebugP_log("Unexpected value in ADC1_AIN1");
    return false;
  }

  if (fabs(adc_to_current(ain1) - ain0_expected_value) > DISCREPANCY) {
    DebugP_log("Unexpected value in ADC1_AIN0");
    return false;
  }

  if (gpio44_value != gpio44_expected_value) {
    DebugP_log("Unexpected value in GPIO44");
    return false;
  }
  return true;
}