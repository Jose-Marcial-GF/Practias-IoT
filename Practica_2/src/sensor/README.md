# Firmware del sensor (pendiente)

Aquí va el código del dispositivo sensor (entorno `sensor` de `platformio.ini`).
Todo lo que está en esta carpeta se compila con `pio run -e sensor`.

Debe incluir `../common/protocol.h` y respetar las reglas que figuran allí.
El supervisor ya está terminado en `../supervisor/`.

Sugerencia: separar los módulos igual que en el supervisor (config, hebras,
atención de mensajes, sensores, pantalla, idle).
