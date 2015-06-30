# STM32_Wifi_sprinkler
Use wifi to fetch json data from the web, then design an algorithm to figure out when and how long to turn on the sprinklers.

Hardware used:
STM32F401 but need to change to cheaper STM32 since we only pull every hour, esp8266 to fetch json, SD card for data recording (note to self: Move to 23LC1024), 8 zones SSR + USB for physical connection. Coded an Android app to fetch webpage hosted on esp8266 website interface for data display and manual mode control.

PCB in altium
