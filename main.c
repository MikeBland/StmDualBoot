/* *****************************************************************************
 * The MIT License
 *
 * Copyright (c) 2010 LeafLabs LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * ****************************************************************************/

/**
 *  @file main.c
 *
 *  @brief main loop and calling any hardware init stuff. timing hacks for EEPROM
 *  writes not to block usb interrupts. logic to handle 2 second timeout then
 *  jump to user code.
 *
 */

#include "common.h"
#include "dfu.h"
#include "SerialLoader.h"
extern volatile dfuUploadTypes_t userUploadType;

extern bool readButtonState() ;

//Possible resets
// Power on - check for protocol 0 and button pressed - serial loader
// Power on otherwise - check for USB and serial
// Soft reset:
// BKP10 == RTC_BOOTLOADER_SELF_RESTART - nothing in flash to execute - check for USB and serial
// BKP10 == RTC_BOOTLOADER_JUST_UPLOADED - exeute loaded firmware if possible
// BKP10 == 0 - serial loader

int main()
{
    bool no_user_jump = FALSE;
    bool dont_wait = FALSE;

    systemReset(); // peripherals but not PC
    setupCLK();
    setupUSB();
    setupFLASH();
	serialSetup() ;

    // Configure the LED pin
    SET_REG(GPIO_CR(LED_BANK,LED_PIN),(GET_REG(GPIO_CR(LED_BANK,LED_PIN)) & crMask(LED_PIN)) | CR_OUTPUT_PP << CR_SHITF(LED_PIN));

    // Check the reset reason and clear the flags
    switch(checkAndClearBootloaderFlag())
    {
        case 0x01:
            // Persistent bootloader mode - don't jump to the app code
            no_user_jump = TRUE;
            strobePin(LED_BANK, LED_PIN, STARTUP_BLINKS, BLINK_FAST,LED_ON_STATE);
            break;
        case 0x02:
            // Firmware just uploaded - don't wait for another upload, go straight to the app code
            dont_wait = TRUE;
            break;
        case 0x03:
            // Bootloader app is running - don't jump to the app code
            no_user_jump = TRUE;
            break;
        default:
            // Any other condition (e.g. power-on reset)

            // Strobe the LED quickly
            strobePin(LED_BANK, LED_PIN, STARTUP_BLINKS, BLINK_FAST, LED_ON_STATE);

            // Check if there is a valid application in flash
            if (!checkUserCode(USER_CODE_FLASH0X8002000))
            {
                // No valid application - don't jump to the app code
                no_user_jump = TRUE;
            }
            break;
    }

    // Read the state of the USB D- pin (PA11) (indicates if USB is plugged in)
    if (!(GET_REG(GPIO_IDR(GPIOA)) & (0x01 << 11))) {
        // Stay in the bootloader if D- is low (USB is plugged in)
        no_user_jump = TRUE;
    }

    // Wait for an upload attempt, unless we shouldn't
    if (!dont_wait)
    {
        int delay_count = 0;

        // Wait here until the startup delay expires, or indefinitely if we shouldn't jump to the app code for some reason
        while ((delay_count++ < BOOTLOADER_WAIT) || no_user_jump)
        {
            // Strobe the LED less quickly
            strobePin(LED_BANK, LED_PIN, 1, BLINK_SLOW, LED_ON_STATE);

            if (dfuUploadStarted())
            {
                dfuFinishUpload(); // systemHardReset from DFU once done
            }
			testLoader() ;
        }
    }
	
    // Jump to the main app code
    jumpToUser(USER_CODE_FLASH0X8002000);
}
