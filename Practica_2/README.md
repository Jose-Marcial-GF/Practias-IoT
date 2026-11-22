# Práctica 2 del guion (carpeta `Practica_3` del repo) — Supervisor y sensor por Serial1

```
pio run -e supervisor -t upload --upload-port /dev/ttyACM1
pio run -e sensor     -t upload --upload-port /dev/ttyACM0
```
```
pio device monitor -p /dev/ttyACM0 -b 9600
pio device monitor -p /dev/ttyACM1 -b 115200
```



(pantalla)[https://learn.adafruit.com/monochrome-oled-breakouts/wiring-128x64-oleds]
(sensor)[https://www.robot-electronics.co.uk/htm/srf02techI2C.htm]