# Diagnostic module

nRF528xx ports extend [OpenThread Diagnostics Module][diag].

## diag gpio

Note: `<gpio>` in `diag gpio` commands is an integer that combines port and pin into a single, contiguous number space as follows:

```c
   gpio = (port * 32) + pin
```

See also the [`NRF_GPIO_PIN_MAP`](../../../third_party/NordicSemiconductor/hal/nrf_gpio.h) macro.

## New commands

New commands allow for more accurate low level radio testing.

- [diag ccathreshold](#diag-ccathreshold)
- [diag id](#diag-id)
- [diag listen](#diag-listen)
- [diag temp](#diag-temp)
- [diag transmit](#diag-transmit)

### Diagnostic radio packet

[diag listen](#diag-listen) and [diag transmit](#diag-transmit) use radio frame payload specified below.

```c
struct PlatformDiagMessage
{
    const char mMessageDescriptor[11];
    uint8_t mChannel;
    int16_t mID;
    uint32_t mCnt;
};
```

`mMessageDescriptor` is a constant string `"DiagMessage"`.<br /> `mChannel` contains the channel number on which the packet was transmitted.<br /> `mID` contains the board ID set with the [diag id](#diag-id) command.<br /> `mCnt` is a counter incremented every time the board transmits diagnostic radio packet.

If the [listen mode](#diag-listen) is enabled and OpenThread was built with the`DEFAULT_LOGGING` flag, JSON string is printed every time a diagnostic radio packet is received.

```JSON
 {"Frame":{
   "LocalChannel":"<listening board channel>",
   "RemoteChannel":"<mChannel>",
   "CNT":"<mCnt>",
   "LocalID":"<listening board ID>",
   "RemoteID":"<mID>",
   "RSSI":"<packet RSSI>"
 }}
```

### diag ccathreshold

Get the current CCA threshold.

### diag ccathreshold \<threshold\>

Set the CCA threshold.

Value range: 0 to 255.

Default: `45`.

### diag id

Get board ID.

### diag id \<id\>

Set board ID.

Value range: 0 to 32767.

Default: `-1`.

### diag listen

Get the listen state.

### diag listen \<listen\>

Set the listen state.

`0` disables the listen state.<br /> `1` enables the listen state.

By default, the listen state is disabled.

### diag temp

Get the temperature from the internal temperature sensor (in degrees Celsius).

### diag transmit

Get the message count and the interval between the messages that will be transmitted after `diag transmit start`.

### diag transmit interval \<interval\>

Set the interval in ms between the transmitted messages.

Value range: 1 to 4294967295.

Default: `1`.

### diag transmit count \<count\>

Set the number of messages to be transmitted.

Value range: 1 to 2147483647<br /> or<br /> For continuous transmission: `-1`

Default: `1`

### diag transmit stop

Stop the ongoing transmission regardless of the remaining number of messages to be sent.

### diag transmit start

Start transmiting messages with specified interval.

### diag transmit carrier

Start transmitting continuous carrier wave.

[diag]: https://github.com/openthread/openthread/tree/main/src/core/diags/README.md
