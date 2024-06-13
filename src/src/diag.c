/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <openthread-core-config.h>
#include <openthread/config.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform-nrf5.h"

#include <hal/nrf_gpio.h>

#include <openthread/cli.h>
#include <openthread/platform/alarm-milli.h>
#include <openthread/platform/diag.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/toolchain.h>

#include <common/logging.hpp>
#include <nrf_802154.h>
#include <utils/code_utils.h>

typedef enum
{
    kDiagTransmitModeIdle,
    kDiagTransmitModePackets,
    kDiagTransmitModeCarrier
} DiagTrasmitMode;

struct PlatformDiagCommand
{
    const char *mName;
    otError (*mCommand)(otInstance *aInstance, uint8_t aArgsLength, char *aArgs[]);
};

struct PlatformDiagMessage
{
    const char mMessageDescriptor[11];
    uint8_t    mChannel;
    int16_t    mID;
    uint32_t   mCnt;
};

/**
 * Diagnostics mode variables.
 *
 */
static bool                       sDiagMode         = false;
static bool                       sListen           = false;
static DiagTrasmitMode            sTransmitMode     = kDiagTransmitModeIdle;
static uint8_t                    sChannel          = 20;
static int8_t                     sTxPower          = 0;
static uint32_t                   sTxPeriod         = 1;
static int32_t                    sTxCount          = 0;
static int32_t                    sTxRequestedCount = 1;
static int16_t                    sID               = -1;
static struct PlatformDiagMessage sDiagMessage      = {.mMessageDescriptor = "DiagMessage",
                                                       .mChannel           = 0,
                                                       .mID                = 0,
                                                       .mCnt               = 0};

static otPlatDiagOutputCallback sDiagOutputCallback  = NULL;
static void                    *sDiagCallbackContext = NULL;

static otError parseLong(char *aArgs, long *aValue)
{
    char *endptr;
    *aValue = strtol(aArgs, &endptr, 0);
    return (*endptr == '\0') ? OT_ERROR_NONE : OT_ERROR_PARSE;
}

static void diagOutput(const char *aFormat, ...)
{
    va_list args;

    va_start(args, aFormat);

    if (sDiagOutputCallback != NULL)
    {
        sDiagOutputCallback(aFormat, args, sDiagCallbackContext);
    }

    va_end(args);
}

static void appendErrorResult(otError aError)
{
    if (aError != OT_ERROR_NONE)
    {
        diagOutput("failed\r\nstatus %#x\r\n", aError);
    }
}

static bool startCarrierTransmision(void)
{
    nrf_802154_channel_set(sChannel);
    nrf_802154_tx_power_set(sTxPower);

    return nrf_802154_continuous_carrier();
}

static otError processListen(otInstance *aInstance, uint8_t aArgsLength, char *aArgs[])
{
    OT_UNUSED_VARIABLE(aInstance);

    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(otPlatDiagModeGet(), error = OT_ERROR_INVALID_STATE);

    if (aArgsLength == 0)
    {
        diagOutput("listen: %s\r\n", sListen == true ? "yes" : "no");
    }
    else
    {
        long value;

        error = parseLong(aArgs[0], &value);
        otEXPECT(error == OT_ERROR_NONE);
        sListen = (bool)(value);
        diagOutput("set listen to %s\r\nstatus 0x%02x\r\n", sListen == true ? "yes" : "no", error);
    }

exit:
    appendErrorResult(error);
    return error;
}

static otError processID(otInstance *aInstance, uint8_t aArgsLength, char *aArgs[])
{
    OT_UNUSED_VARIABLE(aInstance);

    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(otPlatDiagModeGet(), error = OT_ERROR_INVALID_STATE);

    if (aArgsLength == 0)
    {
        diagOutput("ID: %" PRId16 "\r\n", sID);
    }
    else
    {
        long value;

        error = parseLong(aArgs[0], &value);
        otEXPECT(error == OT_ERROR_NONE);
        otEXPECT_ACTION(value >= 0, error = OT_ERROR_INVALID_ARGS);
        sID = (int16_t)(value);
        diagOutput("set ID to %" PRId16 "\r\nstatus 0x%02x\r\n", sID, error);
    }

exit:
    appendErrorResult(error);
    return error;
}

static otError processTransmit(otInstance *aInstance, uint8_t aArgsLength, char *aArgs[])
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(otPlatDiagModeGet(), error = OT_ERROR_INVALID_STATE);

    if (aArgsLength == 0)
    {
        diagOutput("transmit will send %" PRId32 " diagnostic messages with %" PRIu32
                   " ms interval\r\nstatus 0x%02x\r\n",
                   sTxRequestedCount, sTxPeriod, error);
    }
    else if (strcmp(aArgs[0], "stop") == 0)
    {
        otEXPECT_ACTION(sTransmitMode != kDiagTransmitModeIdle, error = OT_ERROR_INVALID_STATE);

        otPlatAlarmMilliStop(aInstance);
        diagOutput("diagnostic message transmission is stopped\r\nstatus 0x%02x\r\n", error);
        sTransmitMode = kDiagTransmitModeIdle;
        otPlatRadioReceive(aInstance, sChannel);
    }
    else if (strcmp(aArgs[0], "start") == 0)
    {
        otEXPECT_ACTION(sTransmitMode == kDiagTransmitModeIdle, error = OT_ERROR_INVALID_STATE);

        otPlatAlarmMilliStop(aInstance);
        sTransmitMode = kDiagTransmitModePackets;
        sTxCount      = sTxRequestedCount;
        uint32_t now  = otPlatAlarmMilliGetNow();
        otPlatAlarmMilliStartAt(aInstance, now, sTxPeriod);
        diagOutput("sending %" PRId32 " diagnostic messages with %" PRIu32 " ms interval\r\nstatus 0x%02x\r\n",
                   sTxRequestedCount, sTxPeriod, error);
    }
    else if (strcmp(aArgs[0], "carrier") == 0)
    {
        otEXPECT_ACTION(sTransmitMode == kDiagTransmitModeIdle, error = OT_ERROR_INVALID_STATE);

        otEXPECT_ACTION(startCarrierTransmision(), error = OT_ERROR_FAILED);

        sTransmitMode = kDiagTransmitModeCarrier;

        diagOutput("sending carrier on channel %d with tx power %d\r\nstatus 0x%02x\r\n", sChannel, sTxPower, error);
    }
    else if (strcmp(aArgs[0], "interval") == 0)
    {
        long value;

        otEXPECT_ACTION(aArgsLength == 2, error = OT_ERROR_INVALID_ARGS);

        error = parseLong(aArgs[1], &value);
        otEXPECT(error == OT_ERROR_NONE);
        otEXPECT_ACTION(value > 0, error = OT_ERROR_INVALID_ARGS);
        sTxPeriod = (uint32_t)(value);
        diagOutput("set diagnostic messages interval to %" PRIu32 " ms\r\nstatus 0x%02x\r\n", sTxPeriod, error);
    }
    else if (strcmp(aArgs[0], "count") == 0)
    {
        long value;

        otEXPECT_ACTION(aArgsLength == 2, error = OT_ERROR_INVALID_ARGS);

        error = parseLong(aArgs[1], &value);
        otEXPECT(error == OT_ERROR_NONE);
        otEXPECT_ACTION((value > 0) || (value == -1), error = OT_ERROR_INVALID_ARGS);
        sTxRequestedCount = (uint32_t)(value);
        diagOutput("set diagnostic messages count to %" PRId32 "\r\nstatus 0x%02x\r\n", sTxRequestedCount, error);
    }
    else
    {
        error = OT_ERROR_INVALID_ARGS;
    }

exit:
    appendErrorResult(error);
    return error;
}

static otError processTemp(otInstance *aInstance, uint8_t aArgsLength, char *aArgs[])
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aArgs);

    otError error = OT_ERROR_NONE;
    int32_t temperature;

    otEXPECT_ACTION(otPlatDiagModeGet(), error = OT_ERROR_INVALID_STATE);
    otEXPECT_ACTION(aArgsLength == 0, error = OT_ERROR_INVALID_ARGS);

    temperature = nrf5TempGet();

    // Measurement resolution is 0.25 degrees Celsius
    // Convert the temperature measurement to a decimal value, in degrees Celsius
    diagOutput("%" PRId32 ".%02" PRId32 "\r\n", temperature / 4, 25 * (temperature % 4));

exit:
    appendErrorResult(error);
    return error;
}

static otError processCcaThreshold(otInstance *aInstance, uint8_t aArgsLength, char *aArgs[])
{
    OT_UNUSED_VARIABLE(aInstance);

    otError              error = OT_ERROR_NONE;
    nrf_802154_cca_cfg_t ccaConfig;

    otEXPECT_ACTION(otPlatDiagModeGet(), error = OT_ERROR_INVALID_STATE);

    if (aArgsLength == 0)
    {
        nrf_802154_cca_cfg_get(&ccaConfig);

        diagOutput("cca threshold: %u\r\n", ccaConfig.ed_threshold);
    }
    else
    {
        long value;
        error = parseLong(aArgs[0], &value);
        otEXPECT(error == OT_ERROR_NONE);
        otEXPECT_ACTION(value >= 0 && value <= 0xFF, error = OT_ERROR_INVALID_ARGS);

        memset(&ccaConfig, 0, sizeof(ccaConfig));
        ccaConfig.mode         = NRF_RADIO_CCA_MODE_ED;
        ccaConfig.ed_threshold = (uint8_t)value;

        nrf_802154_cca_cfg_set(&ccaConfig);
        diagOutput("set cca threshold to %u\r\nstatus 0x%02x\r\n", ccaConfig.ed_threshold, error);
    }

exit:
    appendErrorResult(error);
    return error;
}

const struct PlatformDiagCommand sCommands[] = {{"ccathreshold", &processCcaThreshold},
                                                {"id", &processID},
                                                {"listen", &processListen},
                                                {"temp", &processTemp},
                                                {"transmit", &processTransmit}};

void otPlatDiagSetOutputCallback(otInstance *aInstance, otPlatDiagOutputCallback aCallback, void *aContext)
{
    OT_UNUSED_VARIABLE(aInstance);

    sDiagOutputCallback  = aCallback;
    sDiagCallbackContext = aContext;
}

otError otPlatDiagProcess(otInstance *aInstance, uint8_t aArgsLength, char *aArgs[])
{
    otError error = OT_ERROR_INVALID_COMMAND;
    size_t  i;

    for (i = 0; i < otARRAY_LENGTH(sCommands); i++)
    {
        if (strcmp(aArgs[0], sCommands[i].mName) == 0)
        {
            error = sCommands[i].mCommand(aInstance, aArgsLength - 1, aArgsLength > 1 ? &aArgs[1] : NULL);
            break;
        }
    }

    return error;
}

void otPlatDiagModeSet(bool aMode)
{
    sDiagMode = aMode;

    if (!sDiagMode)
    {
        otPlatRadioReceive(NULL, sChannel);
        otPlatRadioSleep(NULL);

        // Clear all remaining events before switching to MAC callbacks.
        nrf5RadioClearPendingEvents();
    }
    else
    {
        // Reinit
        sTransmitMode = kDiagTransmitModeIdle;
    }
}

bool otPlatDiagModeGet()
{
    return sDiagMode;
}

void otPlatDiagChannelSet(uint8_t aChannel)
{
    sChannel = aChannel;
}

void otPlatDiagTxPowerSet(int8_t aTxPower)
{
    sTxPower = aTxPower;
}

void otPlatDiagRadioReceived(otInstance *aInstance, otRadioFrame *aFrame, otError aError)
{
    OT_UNUSED_VARIABLE(aInstance);

    if (sListen && (aError == OT_ERROR_NONE))
    {
        if (aFrame->mLength == sizeof(struct PlatformDiagMessage))
        {
            struct PlatformDiagMessage *message = (struct PlatformDiagMessage *)aFrame->mPsdu;

            if (strncmp(message->mMessageDescriptor, "DiagMessage", 11) == 0)
            {
                otPlatLog(OT_LOG_LEVEL_DEBG, OT_LOG_REGION_PLATFORM,
                          "{\"Frame\":{"
                          "\"LocalChannel\":%u ,"
                          "\"RemoteChannel\":%u,"
                          "\"CNT\":%" PRIu32 ","
                          "\"LocalID\":%" PRId16 ","
                          "\"RemoteID\":%" PRId16 ","
                          "\"RSSI\":%d"
                          "}}\r\n",
                          aFrame->mChannel, message->mChannel, message->mCnt, sID, message->mID,
                          aFrame->mInfo.mRxInfo.mRssi);
            }
        }
    }
}

void otPlatDiagAlarmCallback(otInstance *aInstance)
{
    if (sTransmitMode == kDiagTransmitModePackets)
    {
        if ((sTxCount > 0) || (sTxCount == -1))
        {
            otRadioFrame *sTxPacket = otPlatRadioGetTransmitBuffer(aInstance);

            sTxPacket->mLength  = sizeof(struct PlatformDiagMessage);
            sTxPacket->mChannel = sChannel;

            sDiagMessage.mChannel = sTxPacket->mChannel;
            sDiagMessage.mID      = sID;

            memcpy(sTxPacket->mPsdu, &sDiagMessage, sizeof(struct PlatformDiagMessage));
            otPlatRadioTransmit(aInstance, sTxPacket);

            sDiagMessage.mCnt++;

            if (sTxCount != -1)
            {
                sTxCount--;
            }

            uint32_t now = otPlatAlarmMilliGetNow();
            otPlatAlarmMilliStartAt(aInstance, now, sTxPeriod);
        }
        else
        {
            sTransmitMode = kDiagTransmitModeIdle;
            otPlatAlarmMilliStop(aInstance);
            otPlatLog(OT_LOG_LEVEL_DEBG, OT_LOG_REGION_PLATFORM, "Transmit done");
        }
    }
}

otError otPlatDiagGpioSet(uint32_t aGpio, bool aValue)
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(otPlatDiagModeGet(), error = OT_ERROR_INVALID_STATE);
    otEXPECT_ACTION(nrf_gpio_pin_present_check(aGpio), error = OT_ERROR_INVALID_ARGS);
    otEXPECT_ACTION(nrf_gpio_pin_dir_get(aGpio) == NRF_GPIO_PIN_DIR_OUTPUT, error = OT_ERROR_INVALID_STATE);

    nrf_gpio_pin_write(aGpio, (uint32_t)aValue);

exit:
    return error;
}

otError otPlatDiagGpioGet(uint32_t aGpio, bool *aValue)
{
    otError            error = OT_ERROR_NONE;
    nrf_gpio_pin_dir_t pinDir;

    otEXPECT_ACTION(otPlatDiagModeGet(), error = OT_ERROR_INVALID_STATE);
    otEXPECT_ACTION((aValue != NULL) && (nrf_gpio_pin_present_check(aGpio)), error = OT_ERROR_INVALID_ARGS);

    pinDir = nrf_gpio_pin_dir_get(aGpio);
    if (pinDir == NRF_GPIO_PIN_DIR_INPUT)
    {
        *aValue = (bool)nrf_gpio_pin_read(aGpio);
    }
    else
    {
        *aValue = (bool)nrf_gpio_pin_out_read(aGpio);
    }

exit:
    return error;
}

otError otPlatDiagGpioSetMode(uint32_t aGpio, otGpioMode aMode)
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(otPlatDiagModeGet(), error = OT_ERROR_INVALID_STATE);
    otEXPECT_ACTION(nrf_gpio_pin_present_check(aGpio), error = OT_ERROR_INVALID_ARGS);

    switch (aMode)
    {
    case OT_GPIO_MODE_INPUT:
        nrf_gpio_cfg_input(aGpio, NRF_GPIO_PIN_NOPULL);
        break;

    case OT_GPIO_MODE_OUTPUT:
        nrf_gpio_cfg_output(aGpio);
        break;

    default:
        error = OT_ERROR_INVALID_ARGS;
    }

exit:
    return error;
}

otError otPlatDiagGpioGetMode(uint32_t aGpio, otGpioMode *aMode)
{
    otError            error = OT_ERROR_NONE;
    nrf_gpio_pin_dir_t pinDir;

    otEXPECT_ACTION(otPlatDiagModeGet(), error = OT_ERROR_INVALID_STATE);
    otEXPECT_ACTION((aMode != NULL) && (nrf_gpio_pin_present_check(aGpio)), error = OT_ERROR_INVALID_ARGS);

    pinDir = nrf_gpio_pin_dir_get(aGpio);
    switch (pinDir)
    {
    case NRF_GPIO_PIN_DIR_INPUT:
        *aMode = OT_GPIO_MODE_INPUT;
        break;

    case NRF_GPIO_PIN_DIR_OUTPUT:
        *aMode = OT_GPIO_MODE_OUTPUT;
        break;

    default:
        error = OT_ERROR_FAILED;
    }

exit:
    return error;
}
