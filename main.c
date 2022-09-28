/******************************************************************************
 * Copyright (C) 2022 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *
 ******************************************************************************/

/*
 * @file    main.c
 * @brief   Demonstrates the various low power modes.
 *
 * @details Iterates through the various low power modes, using either the RTC
 *          alarm or a GPIO to wake from each.  #defines determine which wakeup
 *          source to use.  Once the code is running, you can measure the
 *          current used on the VCORE rail.
 *
 *          The power states shown are:
 *            1. Active mode power with all clocks on
 *            2. Active mode power with peripheral clocks disabled
 *            3. Active mode power with unused RAMs shut down
 *            4. SLEEP mode
 *            5. LPM mode
 *            6. UPM mode
 *            7. BACKUP mode
 *            8. STANDBY mode
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "mxc_device.h"
#include "mxc_errors.h"
#include "board.h"
#include "gpio.h"
#include "pb.h"
#include "led.h"
#include "lp.h"
#include "icc.h"
#include <mxc.h>
#include "adc.h"
#include "rtc.h"
#include "uart.h"
#include "nvic_table.h"
#include "mxc_delay.h"

/*================== ADC ==================*/
unsigned int adc_val = 0;
uint8_t overflow;
/*=========================================*/

/*=============================== GPIO ==================================*/
#define DELAY_IN_SEC 2
mxc_gpio_cfg_t gpio_interrupt;
#define Interrupt_PORT_input MXC_GPIO0      // GPIO-0
#define Interrupt_PIN_input MXC_GPIO_PIN_18 // GPIO-Pin-18

mxc_gpio_cfg_t gpio_in;
#define Normal_PORT_input MXC_GPIO0
#define Normal_PIN_input MXC_GPIO_PIN_19

mxc_gpio_cfg_t gpio_out;
#define Normal_PORT_output MXC_GPIO0
#define Normal_PIN_output MXC_GPIO_PIN_25
// *****************************************************************************

volatile int buttonPressed;

void buttonHandler(void *pb)
{
    buttonPressed = 1;
    printf("Interrupt Occured.\n");
    printf("Waking up from SLEEP mode.\n");
    printf(" \n");
}

void setTrigger(int waitForTrigger)
{
    volatile int tmp;

    buttonPressed = 0;

    if (waitForTrigger)
    {
        while (!buttonPressed)
            ;
    }

    // Debounce the button press.
    for (tmp = 0; tmp < 0x100000; tmp++)
    {
        __NOP();
    }

    // Wait for serial transactions to complete.
    while (MXC_UART_ReadyForSleep(MXC_UART_GET_UART(CONSOLE_UART)) != E_NO_ERROR)
        ;
}

int main(void)
{
    /*========================== GPIO Interrupts ==========================*/
    /*
     *   Set up interrupt on MXC_GPIO_PORT_INTERRUPT_IN.
     *   Switch on EV kit is open when non-pressed, and grounded when pressed.  Use an internal pull-up so pin
     *     reads high when button is not pressed.
     */

    gpio_interrupt.port = Interrupt_PORT_input;
    gpio_interrupt.mask = Interrupt_PIN_input;
    gpio_interrupt.pad = MXC_GPIO_PAD_PULL_UP;
    gpio_interrupt.func = MXC_GPIO_FUNC_IN;
    gpio_interrupt.vssel = MXC_GPIO_VSSEL_VDDIO;

    MXC_GPIO_RegisterCallback(&gpio_interrupt, buttonHandler, (void *)&gpio_interrupt);
    MXC_GPIO_IntConfig(&gpio_interrupt, MXC_GPIO_INT_FALLING);
    MXC_GPIO_EnableInt(gpio_interrupt.port, gpio_interrupt.mask);
    NVIC_EnableIRQ(MXC_GPIO_GET_IRQ(MXC_GPIO_GET_IDX(gpio_interrupt.port)));
    /*========================= END GPIO Interrupts =========================*/

    /*========================== Normal GPIO ==========================*/
    gpio_in.port = Normal_PORT_input;
    gpio_in.mask = Normal_PIN_input;
    gpio_in.pad = MXC_GPIO_PAD_PULL_UP;
    gpio_in.func = MXC_GPIO_FUNC_IN;
    MXC_GPIO_Config(&gpio_in);

    gpio_out.port = Normal_PORT_output;
    gpio_out.mask = Normal_PIN_output;
    gpio_out.pad = MXC_GPIO_PAD_NONE;
    gpio_out.func = MXC_GPIO_FUNC_OUT;
    MXC_GPIO_Config(&gpio_out);
    /*======================== END Normal GPIO ========================*/

    /*====================== Initialize ADC =======================*/
    if (MXC_ADC_Init() != E_NO_ERROR)
    {
        printf("Error Bad Parameter\n");

        while (1)
            ;
    }

    /* Set up LIMIT0 to monitor high and low trip points */
    MXC_ADC_SetMonitorChannel(MXC_ADC_MONITOR_0, MXC_ADC_CH_0);
    MXC_ADC_SetMonitorHighThreshold(MXC_ADC_MONITOR_0, 0x300);
    MXC_ADC_SetMonitorLowThreshold(MXC_ADC_MONITOR_0, 0x25);
    MXC_ADC_EnableMonitor(MXC_ADC_MONITOR_0);

    float inputVolt = 0;
    /*============================= END ADC =================================*/

    printf("****Low Power Mode Example****\n\n");

    printf("This code cycles through the MAX32655 power modes, using a push button (PB1) to exit "
           "from each mode and enter the next.\n\n");

    printf("Running in ACTIVE mode.\n");

    setTrigger(1);

    MXC_LP_EnableGPIOWakeup((mxc_gpio_cfg_t *)&gpio_interrupt);

    while (1)
    {

        /*=============== ADC ================*/
        adc_val = MXC_ADC_StartConversion(MXC_ADC_CH_0);
        printf("%d\n", adc_val);
        printf("ADC Value \n\n");
        inputVolt = (adc_val * 1.2) / 1023;
        printf("%f\n", inputVolt);
        printf("Imput Voltage \n\n");
        /*====================================*/

        if (!(MXC_GPIO_InGet(gpio_in.port, gpio_in.mask)))
        {
            // Go to SOFT RESET, when GPIO P0.19 is pressed
            printf("Soft Reset.\n");
            // Wait for serial transactions to complete.
            while (MXC_UART_ReadyForSleep(MXC_UART_GET_UART(CONSOLE_UART)) != E_NO_ERROR)
                ;
            NVIC_SystemReset();
        }

        if (inputVolt <= 0.5)
        {
            // Go to SLEEP MODE, when battery level is below the threshold value
            MXC_GPIO_OutClr(gpio_out.port, gpio_out.mask); // Turn ON the LED
            printf("Low Battery\n\n");
            printf("Device is in SLEEP MODE\n\n");
            // Wait for serial transactions to complete.
            while (MXC_UART_ReadyForSleep(MXC_UART_GET_UART(CONSOLE_UART)) != E_NO_ERROR)
                ;
            MXC_LP_EnterLowPowerMode();
        }
        else
            MXC_GPIO_OutSet(gpio_out.port, gpio_out.mask); // Turn OFF the LED

        /* Delay for 1/4 second before next reading */
        MXC_TMR_Delay(MXC_TMR0, MSEC(250)); // Fixed Delay in Microseconds
    }
}
