Solarstorm_X2
=============

alternative Firmware for the Solarstorm X2 flashlight with Attiny44,84 & Arduino
it uses the Arduino Tiny core (https://github.com/SpenceKonde/ATTinyCore)


- 4 light modes
- Light intensity of each mode can be set and stored in internal eeprom by pressing the button longer than 3secs
- NTC temp control
- under voltage control

Note: you has to replace the original SOIC14 uC to an Attiny44,84 and solder from the internal 5V a 10k 
      pull-up resistor to the Attiny Reset pin. that's all 
      
have fun
