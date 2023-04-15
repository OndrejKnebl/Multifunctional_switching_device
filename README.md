# Multifunctional Switching Device - Multifunkční spínací zařízení

A program for a device that can switch, for example, lights in a small parking lot or lights around a house based on light intensity, set switching and opening times, and also according to light intensity at set times or according to sunrise and sunset times in a certain location. The device can also be always closed or always open (contactor).

The device also functions as a measuring device that measures light intensity (with a BH1750 sensor), voltage and frequency of the power line (with a PZEM-004T-100A measuring device). When the device is switched on, it measures the electrical energy consumption, current, active power and power factor of the switched device (also with the PZEM-004T-100A measuring device). Furthermore, the voltage, state and temperature of the battery that powers the device during a power failure (by the LC709203F module) and the temperature of the RTC are measured.

The device works in the LoRaWAN IoT network in The Things Stack. Most device settings can be performed from TTS using downlinks, in which configuration data encoded in Cayenne LPP format is sent.

One of the solutions offered below can be used to set up the device.

Data measured by the device and information about the currently set configuration of the device are sent to TTS in uplinks encoded in the Cayenne LPP format (however, definitions of custom data types were added).

-----



Program pro zařízení, které dokáže spínat například světla na malém parkovišti nebo světla kolem domu na základě intenzity světla, nastavených spínacích a rozpínacích času, dále podle intenzity světla v nastavených časech nebo podle časů východu a západu slunce v určitém místě. Zařízení může být také stále sepnuté nebo stále rozepnuté (stykač).

Zařízení také funguje jako měřící přístroj, který měří intenzitu světla (čidlem BH1750), napětí a frekvenci napájecí sítě (měřícím přístrojem PZEM-004T-100A). Když je zařízení sepnuté, tak měří spotřebu elektrické energie, proud, činný výkon a účiník spínaného zařízení (také měřícím přístrojem PZEM-004T-100A). Dále je měřeno napětí, stav a teplota baterie, která zařízení napájí při výpadku napájení (modulem LC709203F) a teplota RTC.

Zařízení pracuje v IoT síti LoRaWAN v The Things Stack. Většinu nastavení zařízení je možné provádět z TTS pomocí downlinků, ve kterých jsou zasílána konfigurační data zakódována ve formátu Cayenne LPP.

Pro nastavování zařízení může být použito některého z níže nabízených řešení.

Data naměřená zařízením a informace o aktuální nastavené konfiguraci zařízení jsou posíláná do TTS v uplincích zakódované ve formátu Cayenne LPP (byly ovšem přidány definice vlastních typů dat).


# Requirements


