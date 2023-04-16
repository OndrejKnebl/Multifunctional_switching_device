# Multifunctional Switching Device - Multifunkční spínací zařízení

![My Remote Image](http://lora.vsb.cz/wp-content/uploads/2023/03/Switch_complete-scaled.jpg)

## Content
- [Requirements and how to build and run the device](https://github.com/OndrejKnebl/Multifunctional_switching_device#requirements-and-how-to-build-and-run-the-device)
- [Short description](https://github.com/OndrejKnebl/Multifunctional_switching_device#short-description)
- [Splitting of the data traffic using FPort](https://github.com/OndrejKnebl/Multifunctional_switching_device/blob/main/README.md#splitting-of-the-data-traffic-using-fport)
- [Solutions that can be used to set up the device using downlinks](https://github.com/OndrejKnebl/Multifunctional_switching_device#solutions-offered-below-can-be-used-to-set-up-the-device-using-downlinks)
- [Encoding table of data sent in uplinks](https://github.com/OndrejKnebl/Multifunctional_switching_device#encoding-tables-of-data-sent-in-uplinks)
- [Decoding tables of data received in downlinks](https://github.com/OndrejKnebl/Multifunctional_switching_device#decoding-tables-of-data-received-in-downlinks)
- [Small time - custom added type](https://github.com/OndrejKnebl/Multifunctional_switching_device#small-time---custom-added-type)
- [Data storage table in EEPROM memory](https://github.com/OndrejKnebl/Multifunctional_switching_device#data-storage-table-in-eeprom-memory)

## Requirements and how to build and run the device

- All this information is available on the website: https://lora.vsb.cz/index.php/multifunctional-switching-device/


## Short description

A program for a device that can switch, for example, lights in a small parking lot or lights around a house based on light intensity, set switching and opening times, and also according to light intensity at set times or according to sunrise and sunset times in a certain location. The device can also be always closed or always open (contactor).

The device also functions as a measuring device that measures light intensity (with a BH1750 sensor), voltage and frequency of the power line (with a PZEM-004T-100A measuring device). When the device is switched on, it measures the electrical energy consumption, current, active power and power factor of the switched device (also with the PZEM-004T-100A measuring device). Furthermore, the voltage, state and temperature of the battery that powers the device during a power failure (by the LC709203F module) and the temperature of the RTC are measured.

The device works in the LoRaWAN IoT network The Things Stack. Most device settings can be done from TTS using downlinks, in which configuration data encoded in Cayenne LPP format (with custom data types) is sent.

Data measured by the device and information about the currently set configuration of the device are sent to TTS in uplinks encoded in the Cayenne LPP format (however, definitions of custom data types were added).

## Splitting of the data traffic using FPort
- Device configuration is performed on FPort number 2 (this applies to both uplinks and downlinks)
- Measured or calculated data are sent by uplinks on FPort number 1

## Solutions offered below can be used to set up the device using downlinks:

#### Solutions directly for this device:
- https://github.com/OndrejKnebl/Multiswitch_Grafana_panel_plugin
- https://github.com/OndrejKnebl/Multiswitch_Python_program
- https://github.com/OndrejKnebl/Multiswitch_Python_script

#### Universal solutions:
- https://lora.vsb.cz/index.php/grafana-panel-plugin-for-sending-downlinks/

## Encoding tables of data sent in uplinks

Tab.1.: Encoding of sent (measured, computed) data in uplinks using Cayenne LPP
| Entry             | Channel (Cayenne LPP) | Type (Cayenne LPP) |
|-------------------|-----------------------|--------------------|
| relaySwitchState  | 1                     | addSwitch          |
| relayStateChanges | 2                     | addSwitch          |
| lightIntensity    | 3                     | addLuminosity      |
| batteryVoltage    | 4                     | addVoltage         |
| batteryCapacity   | 5                     | addPercentage      |
| batteryTemp       | 6                     | addTemperature     |
| rtcTemp           | 7                     | addTemperature     |
| voltage           | 8                     | addVoltage         |
| frequency         | 9                     | addFrequency       |
| energy            | 10                    | addEnergy          |
| current           | 11                    | addCurrent         |
| power             | 12                    | addPower           |
| pf                | 13                    | addAnalogOutput    |
| Sunset            | 111                   | addSmallTime       |
| Sunrise           | 112                   | addSmallTime       |

Tab.2.: Table of data stored in bit arrays
| Array      | Bit position in array | Meaning of the variable          | Variable for resulting value |
|------------|-----------------------|----------------------------------|------------------------------|
| errors[]   | 7                     | Send data every error            | errorsInOne                  |
|            | 6                     | Number of measured samples error |                              |
|            | 5                     | Working mode error               |                              |
|            | 4                     | Switching times error            |                              |
|            | 3                     | Timezone error                   |                              |
|            | 2                     | Power Grid error                 |                              |
|            | 1                     | Location error                   |                              |
|            | 0                     | Password error                   |                              |
| statuses[] | 7                     | -                                | statusesInOne                |
|            | 6                     | -                                |                              |
|            | 5                     | -                                |                              |
|            | 4                     | -                                |                              |
|            | 3                     | statusAT24C256                   |                              |
|            | 2                     | statusDS3231                     |                              |
|            | 1                     | statusLC709203F                  |                              |
|            | 0                     | statusBH1750                     |                              |

Tab.3.: Encoding of sent (first configuration and configuration) data in uplinks using Cayenne LPP
| Entry                  | Channel (Cayenne LPP) | Type (Cayenne LPP) |
|------------------------|-----------------------|--------------------|
| statusesInOne          | 51                    | addDigitalInput    |
| errorsInOne            | 150                   | addDigitalInput    |
| workingMode            | 101                   | addDigitalInput    |
| myTimeZone             | 102                   | addDigitalInput    |
| myPowerGrid            | 103                   | addDigitalInput    |
| sendDataEvery          | 100                   | addSmallTime       |
| measureSamples         | 100                   | addPresence        |
| lightIThreshold        | 101                   | addLuminosity      |
| lightISafeZone         | 102                   | addLuminosity      |
| latitude, longitude    | 101                   | addGPS             |
| On time1               | 101                   | addSmallTime       |
| On time2               | 103                   | addSmallTime       |
| On time3               | 105                   | addSmallTime       |
| Off time1              | 102                   | addSmallTime       |
| Off time2              | 104                   | addSmallTime       |
| Off time3              | 106                   | addSmallTime       |
| Send only selected 1/2 | 1                     | addDigitalOutput   |
| Send only selected 2/2 | 2                     | addDigitalOutput   |

## Decoding tables of data received in downlinks
Tab.4.: Decoding table of data received in downlinks
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


Tab.5.: Send only selected - Decoding table of data stored in bit arrays received in downlinks 
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


## Data storage table in EEPROM memory
Tab.6.: Data storage table in EEPROM memory at selected addresses
| Address 1 (hexadecimal) | Address 2 (hexadecimal) | Address 1 (decimal) | Address 2 (decimal) | Data                                        | Data types |
|-------------------------|-------------------------|---------------------|---------------------|---------------------------------------------|------------|
| 0x00                    | 0x800                   | 0                   | 2048                | load                                        | char []    |
| 0x10                    | 0x810                   | 16                  | 2064                | Working mode                                | int        |
| 0x20                    | 0x820                   | 32                  | 2080                | Send data every                             | uint32_t   |
| 0x30                    | 0x830                   | 48                  | 2096                | Number of measured samples                  | uint32_t   |
| 0x40                    | 0x840                   | 64                  | 2112                | Light Intensity Threshold                   | float      |
| 0x50                    | 0x850                   | 80                  | 2128                | Light Intensity Safe zone                   | float      |
| 0x60                    | 0x860                   | 96                  | 2144                | Timezone                                    | int        |
| 0x70                    | 0x870                   | 112                 | 2160                | Latitude                                    | float      |
| 0x80                    | 0x880                   | 128                 | 2176                | Longitude                                   | float      |
| 0x90                    | 0x890                   | 144                 | 2192                | Power Grid                                  | int        |
| 0x100                   | 0x900                   | 256                 | 2304                | Send Only Selected -   relaySwitchState     | bool       |
| 0x102                   | 0x902                   | 258                 | 2306                | Send Only Selected -   relaySwitchWaSClosed | bool       |
| 0x104                   | 0x904                   | 260                 | 2308                | Send Only Selected - lightIntensity         | bool       |
| 0x106                   | 0x906                   | 262                 | 2310                | Send Only Selected - batteryVoltage         | bool       |
| 0x108                   | 0x908                   | 264                 | 2312                | Send Only Selected - batteryCapacity        | bool       |
| 0x10A                   | 0x90A                   | 266                 | 2314                | Send Only Selected - batteryTemp            | bool       |
| 0x10C                   | 0x90C                   | 268                 | 2316                | Send Only Selected - rtcTemp                | bool       |
| 0x10E                   | 0x90E                   | 270                 | 2318                | Send Only Selected - voltage                | bool       |
| 0x110                   | 0x910                   | 272                 | 2320                | Send Only Selected - frequency              | bool       |
| 0x112                   | 0x912                   | 274                 | 2322                | Send Only Selected - energy                 | bool       |
| 0x114                   | 0x914                   | 276                 | 2324                | Send Only Selected - current                | bool       |
| 0x116                   | 0x916                   | 278                 | 2326                | Send Only Selected - power                  | bool       |
| 0x118                   | 0x918                   | 280                 | 2328                | Send Only Selected - pf                     | bool       |
| 0x11A                   | 0x91A                   | 282                 | 2330                | Send Only Selected - Sunrise                | bool       |
| 0x11C                   | 0x91C                   | 284                 | 2332                | Send Only Selected - Sunset                 | bool       |
| 0x11E                   | 0x91E                   | 286                 | 2334                | Send Only Selected - reserved               | bool       |
| 0x130                   | 0x930                   | 304                 | 2352                | onOffTimes - small_time_101                 | uint32_t   |
| 0x138                   | 0x938                   | 312                 | 2360                | onOffTimes - small_time_102                 | uint32_t   |
| 0x140                   | 0x940                   | 320                 | 2368                | onOffTimes - small_time_103                 | uint32_t   |
| 0x148                   | 0x948                   | 328                 | 2376                | onOffTimes - small_time_104                 | uint32_t   |
| 0x150                   | 0x950                   | 336                 | 2384                | onOffTimes - small_time_105                 | uint32_t   |
| 0x158                   | 0x958                   | 344                 | 2392                | onOffTimes - small_time_106                 | uint32_t   |
