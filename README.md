# Ceiling Fan Control - (No Humming)

The purpose of this project was to develop a hum-free electronic ceiling fan device that can be controlled remotely (IOT) or locally with a rotary encoder.

The selected processor was the [ESP32-C3](https://www.espressif.com/sites/default/files/documentation/esp32-c3-mini-1_datasheet_en.pdf) in its mini development kit format.


## Explode view 

As the purpose was to replace an old controller in my house, to fit it into a 100mm wall box I had to build a PCB to reduce footprint and use a 3D printer for the front cover that would fit a standard frame for electricity.

As in linux there is no support for fusion 360 use onshape to model the [assembly](https://cad.onshape.com/documents/71acf9008c56a0b1a5695f37/w/967222a47d34025a139e3862/e/0c178ba322c00475acc6add3) and cads of the [front cover](https://cad.onshape.com/documents/136d4c7a6a8e92c2be45bf03/w/4c451ab5564fe96ef80b367c/e/ab9c2ed234f4e64b43c2f324) which only requires a browser, and the result is amazing. And to design and manufacture the [PCB](https://easyeda.com/juanschiavoni/ceiling-iot-esp32-c3) use easyeda, the manufacturing service is good, but I could not export the pcba in step format, I had to use kicad.

![](images/explode_view_v18.gif)

## Electronic for No Humming

To prevent humming, capacitive reactances are used to limit the current in the motor stator, and since the friction (blades etc) is moderately high, the rotation speed decreases. This version has four speeds to keep the form factor as small as possible, (capacitors and relays are large).

## IOT Control

To control the device from a cell phone, [Espressif Rainmaker](https://rainmaker.espressif.com/docs/get-started.html) is used, which has an application for android and iphone, which allows you to configure the Wi-Fi network (provisioning), and control the device through the AWS IOT.

![alt text](images/app_devices.png)

![alt text](images/app_fan.png)

### Manual Control

For local control, a pushbutton rotary encoder is used to set the speed and turn the fan light on/off. To improve the speed resolution, the user has to rotate the shaft three or more steps to increase/decrease.

### Visual indication

Taking advantage of the fact that the ESP32-C3 mini board has a neopixel, the combinations of functions are indicated with colors and brightness levels. For example, when the light is off, the fan speed is indicated with 4 levels of brightness in red, but when it is on in green, and when the fan is off and the light on it turns blue to the maximum, and in idle the the led is white.

### Thermostat Device

Controls the (ON / OFF) of the fan with temperature. As a sensor it uses a thermistor of 100K at 25 degrees Celsius with a beta of 4250 from Murata model NXRT15WF104FA1B040. The firmware uses the simplified [Steinhart equation](https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation) to linearize the response. The control implements a 2 degree hysterisis to improve system stability.

![alt text](images/app_thermostat.png)

The [MF52A104F3950](https://datasheet.lcsc.com/lcsc/1810171512_Nanjing-Shiheng-Elec-MF52A104F3950-A1_C13424.pdf) thermistor that I end up using in the final product is different than the one I used in the prototype, but the results are similar to what I got with the Murata, below is a video with the measurements. Click to play in Youtube

[![](http://img.youtube.com/vi/cc-UPIDVQn8/0.jpg)](https://www.youtube.com/watch?v=cc-UPIDVQn8 "Click to play in Youtube")


### Power Supply

To power the electronic equipment, approximately 350 mA at 3.3V is required for the CPU when Wi-Fi is in active mode, and 90 mA at 5V for each relay. (350mA * 3.3V) = 1,155 W + (90 mA * 5 * 3 relays) = 1,350 W, total = 2,505 W. To increase the safety factor for the proof of concept select the Hi-Link source [HLK-5M05](https://datasheet.lcsc.com/szlcsc/1912111437_HI-LINK-HLK-5M05_C209907.pdf) that delivers 5V at 1A which is twice the estimated power.

![alt text](images/power_supply_5v_1a.png)

On page 7 of the datasheet, there is a table of suggestions for additional components such as a fuse to protect the circuit from damage when the module malfunctions, a capacitor for filtering and safety protection (EMC certified), and a filter / inductor in common. mode, for EMI filtering. And the last thing was adding a filter capacitor that can reduce the output ripple from the original 50mV to 30mV.

The next two images show the DC output and ripple output of the power supply without load.

![alt text](images/TEK_hlk_5v_dc_no_charge.png)

![alt text](images/TEK_hlk_ripple_no_charged.png)

The next two pictures show the DC output and ripple output of the power supply with the CPU running.

![alt text](images/TEK_hlk_5v_dc_charged.png)

![alt text](images/TEK_hlk_ripple_charged.png)

### Reset to Factory

Press and hold the encoder button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.

### Schematic

The schematic of the ESP32-C3 version is simple, it consists of an output stage made up of 4 relays, an encoder for manual control, and a thermistor to control the thermostat. As a power supply it uses a 5V 600 mA HLK.

![alt text](images/Schematic_Ceiling_IOT_ESP32-C3_2021-07-03.png)

### Prototype Board

The first version of the prototype is a mix of Arduino boards with conventional electronics. The next version will add a power supply to test the concept on a fan.

![alt text](images/first_version.png)

The second version has the thermistor to control the fan thermostat.

![alt text](images/second_version.png)

The third prototype is already functional and, although the form factor is not suitable for a wall mount, it allows to control the fan with all the specified functionalities. As you can see, it uses the ESP32-C3 version of the CPU.

![alt text](images/protoype_esp32-c3_1.png)

![alt text](images/protoype_esp32-c3_3.png)

The fourth prototype was to test a smaller relay that is essential for the equipment to fit in a 100mm box. Therefore, a test had to be performed with the new [932-5VDC-SL-AH 10A/0.45W](https://datasheet.lcsc.com/lcsc/1912112237_NHLC-932-5VDC-SL-AH-10A-0-45W_C396979.pdf) relays to verify that they withstand current pulses when fan speeds change. NOTE: Sorry, but since the photo was taken after the final PCBA was assembled, it doesn't have the ESP32-C3 processor.

![alt text](images/protoype_esp32-c3_4.png)

Below is a video of the test performed to evaluate the new relays. Click to play in Youtube.

[![](http://img.youtube.com/vi/B1B9G1hrwu8/0.jpg)](https://www.youtube.com/watch?v=B1B9G1hrwu8 "Click to play in Youtube")


### Final PCBA

To design and manufacture the PCB I used the easyeda online service, which allows you to buy at least 5 units, in my case it works because I plan to replace 4 old fan controllers and I also wanted to experience what the import process is like in my country.

![alt text](images/pcba_easyeda/pcba_front.png)

![alt text](images/pcba_easyeda/pcba_back.png)

In this [link](https://easyeda.com/juanschiavoni/ceiling-iot-esp32-c3) you can access the project that is free under the MIT license, to reform it to your space needs, or if it works for you, you can order it directly from easyeda.

Images of the PCB mounted on the frame for the 100 mm deep-drawn box, typical of Argentina.

![alt text](images/pcba_easyeda/3d_print_mount_front.png)
![alt text](images/pcba_easyeda/3d_print_mount_left.png)
![alt text](images/pcba_easyeda/3d_print_mount_back.png)


### PCBA order proccess

The following video shows the steps to be taken to order the boards with the assembled components except for the esp32-c3 processor, because for now they only mount one side of the PCB. Click to play in Youtube.

[![](http://img.youtube.com/vi/jPR6EYx1I08/0.jpg)](https://www.youtube.com/watch?v=jPR6EYx1I08 "Click to play in Youtube")

### 3D print parts

To mount the pcba in the commercial rack, design with onshape a [cabinet front](https://cad.onshape.com/documents/136d4c7a6a8e92c2be45bf03/w/4c451ab5564fe96ef80b367c/e/ab9c2ed234f4e64b43c2f324) that with three screws allows mounting the board with a protector to avoid short circuits in metal boxes.
In total, 3 pieces are required to be printed in petg: the front, the knob and the protector.

![alt text](images/3d_print_cover.png)
![alt text](images/3d_print_cover.png)

### Development environment

The application is written in C, based on Espressif IDF and as a development tool the [Visual Code](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/vscode-setup.html) of MS was used with the Espressif plugin.

1 - Download the [IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/).

2 - Download the [Rainmaker](https://github.com/espressif/esp-rainmaker).

3 - Download the [celing-fan-iot](https://github.com/jjsch-dev/ceiling-fan-iot) application code.

4 - Select the device target CPU for ESP32-C3.
![](images/visual_code_set_target.gif)

5 - Build (compile and link).
![](images/visual_code_build.gif)

5 - Flash the code.
![](images/visual_code_flash.gif)

6 - Run the serial monitor.
![](images/visual_code_monitor.gif)

7 - Download the Rain-Maker application for Android or iPhone.

8 - Provisioning (Configure and connect to the Wi-Fi network) Click to play in Youtube.

[![](http://img.youtube.com/vi/cgpIMO7QH-g/0.jpg)](https://youtu.be/cgpIMO7QH-g "Click to play in Youtube")

[![](http://img.youtube.com/vi/ymNhdFpC66I/0.jpg)](https://youtu.be/ymNhdFpC66I "Click to play in Youtube")

9 - Enjoy. Click to play in Youtube

[![](http://img.youtube.com/vi/VVv3FSHKODo/0.jpg)](https://www.youtube.com/watch?v=VVv3FSHKODo "Click to play in Youtube")


### Testing the final controller

It shows how the ceiling fan can be controlled from the rainmaker cell phone APP that is connected to the AWS network. Click to play in Youtube.

[![](http://img.youtube.com/vi/gEUzPvxm420/0.jpg)](https://www.youtube.com/watch?v=gEUzPvxm420 "Click to play in Youtube")

It shows how the ceiling fan can be controlled manually. Click to play in Youtube.

[![](http://img.youtube.com/vi/aZ0LPAEYysA/0.jpg)](https://www.youtube.com/watch?v=aZ0LPAEYysA "Click to play in Youtube")

### Old ceiling controller

For reference, this was the bulky externally mounted ceiling fan light speed controller.

![alt text](images/old_ceiling_controller.png)