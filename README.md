# Multifunctional Switching Device - Multifunkční spínací zařízení

## Requirements and how to build and run the device

- All this information is available on the website:


## Short description

A program for a device that can switch, for example, lights in a small parking lot or lights around a house based on light intensity, set switching and opening times, and also according to light intensity at set times or according to sunrise and sunset times in a certain location. The device can also be always closed or always open (contactor).

The device also functions as a measuring device that measures light intensity (with a BH1750 sensor), voltage and frequency of the power line (with a PZEM-004T-100A measuring device). When the device is switched on, it measures the electrical energy consumption, current, active power and power factor of the switched device (also with the PZEM-004T-100A measuring device). Furthermore, the voltage, state and temperature of the battery that powers the device during a power failure (by the LC709203F module) and the temperature of the RTC are measured.

The device works in the LoRaWAN IoT network The Things Stack. Most device settings can be performed from TTS using downlinks, in which configuration data encoded in Cayenne LPP format is sent.

Data measured by the device and information about the currently set configuration of the device are sent to TTS in uplinks encoded in the Cayenne LPP format (however, definitions of custom data types were added).


### Solutions offered below can be used to set up the device:

#### Solutions directly for this device:
-
-

#### Universal solutions:
- https://github.com/OndrejKnebl/TTS_Downlink_Sender_plugin
- 

## Encoding table of data sent in uplinks




## Decoding tables of data received in downlinks
Tab.1.: Decoding table of data received in downlinks
| Entry                         | Entry options               | Decoded type and channel (Cayenne LPP) | Expected value          |
|-------------------------------|-----------------------------|----------------------------------------|-------------------------|
| Settings password             | -                           | power_100                              | 0-9999                  |
| Send data every [s]           | -                           | small_time_100                         | 60-3600                 |
| Number of samples             | -                           | presence_100                           | 1-10                    |
| Power grid [V]                | 230                         | digital_in_103                         | 0                       |
|                               | 400                         | digital_in_103                         | 1                       |
| Working mode                  | OFF                         | digital_in_101                         | 0                       |
|                               | ON                          | digital_in_101                         | 1                       |
|                               | Light intensity             | digital_in_101                         | 2                       |
|                               | Time                        | digital_in_101                         | 3                       |
|                               | Light intensity in Time     | digital_in_101                         | 4                       |
|                               | Sunset / sunrise times      | digital_in_101                         | 5                       |
| Timezone                      | Central European Time       | digital_in_102                         | 0                       |
|                               | United Kingdom              | digital_in_102                         | 1                       |
|                               | UTC                         | digital_in_102                         | 2                       |
|                               | US Eastern Time Zone        | digital_in_102                         | 3                       |
|                               | US Central Time Zone        | digital_in_102                         | 4                       |
|                               | US Mountain Time Zone       | digital_in_102                         | 5                       |
|                               | US Arizona                  | digital_in_102                         | 6                       |
|                               | US Pacific Time Zone        | digital_in_102                         | 7                       |
|                               | Australia Eastern Time Zone | digital_in_102                         | 8                       |
| Reset and load                | Saved                       | digital_in_100                         | 1                       |
|                               | Default                     | digital_in_100                         | 2                       |
| Threshold [lux]               | -                           | luminosity_101                         | 0-65535                 |
| Safe zone [lux]               | -                           | luminosity_102                         | 0-65535                 |
| On time 1:                    | Time not set                | small_time_101                         | 100000                  |
|                               | Set time                    | small_time_101                         | 0-86399                 |
| On time 2:                    | Time not set                | small_time_103                         | 100000                  |
|                               | Set time                    | small_time_103                         | 0-86399                 |
| On time 3:                    | Time not set                | small_time_105                         | 100000                  |
|                               | Set time                    | small_time_105                         | 0-86399                 |
| Off time 1:                   | Time not set                | small_time_102                         | 100000                  |
|                               | Set time                    | small_time_102                         | 0-86399                 |
| Off time 2:                   | Time not set                | small_time_104                         | 100000                  |
|                               | Set time                    | small_time_104                         | 0-86399                 |
| Off time 3:                   | Time not set                | small_time_106                         | 100000                  |
|                               | Set time                    | small_time_106                         | 0-86399                 |
| Latitude, Longitude, Altitude | -                           | gps_101                                | -90 - 90, -180 - 180, 0 |


Tab.2.: Send only selected - Decoding table of data stored in bit fields received in downlinks 
| Entry                | Bit position in array | Decoded type and channel (Cayenne LPP) | Expected value |
|----------------------|-----------------------|----------------------------------------|----------------|
| Relay state          | 7                     | digital_out_1                          | 0-255          |
| Number of changes    | 6                     |                                        |                |
| Light intensity      | 5                     |                                        |                |
| Battery voltage      | 4                     |                                        |                |
| Battery percentage   | 3                     |                                        |                |
| Battery temperature  | 2                     |                                        |                |
| RTC temperature      | 1                     |                                        |                |
| Power line voltage   | 0                     |                                        |                |
| Power line frequency | 7                     | digital_out_2                          | 0-255          |
| Active energy        | 6                     |                                        |                |
| Current              | 5                     |                                        |                |
| Active power         | 4                     |                                        |                |
| Power factor         | 3                     |                                        |                |
| Sunrise              | 2                     |                                        |                |
| Sunset               | 1                     |                                        |                |
| -                    | 0                     |                                        |                |



## Small time - custom added type
- Type: C0
- Size: 3 bytes
- Multiplier: 1
- Sign: no (positive values only)
- Minimum value: 0
- Maximum value: 16777215
