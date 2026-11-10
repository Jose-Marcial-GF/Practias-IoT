/* ----------------------------------------------------------------------------
 *  protocol.h - Mensajes entre supervisor y sensor (por Serial1)
 *
 *  Este fichero lo comparten los dos firmwares: si se cambia algo aquí, se
 *  cambia para los dos.
 *
 *  Todos los mensajes, en los dos sentidos, tienen este formato:
 *
 *      +--------+----------+------------------+
 *      | código | longitud | cuerpo           |
 *      | 1 byte |  1 byte  | "longitud" bytes |
 *      +--------+----------+------------------+
 *
 *  - El primer byte es el código de operación.
 *  - La longitud es el número de bytes del cuerpo (0 si no hay). Hace falta
 *    porque la UART entrega bytes sueltos y no marca dónde acaba un mensaje.
 *  - Los valores de 16 bits se envían con el byte bajo primero.
 *  - "sensor" es el número de orden del SRF02 (1, 2, ...), no su dirección I2C.
 *
 *  El sensor contesta a cada petición con un único mensaje (ACK, ERROR o la
 *  respuesta con datos). Con el modo periódico activo envía además mensajes
 *  MEASURE sin que nadie los pida. El supervisor no valida los valores: si
 *  algo es incorrecto, el sensor contesta con ERROR.
 * ----------------------------------------------------------------------------
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PROTO_BAUD            115200    // Serial1, 8N1
#define PROTO_MAX_BODY        48        // cuerpo máximo de una petición

/* ----- Peticiones (supervisor -> sensor) ----------------------------------- */
/* cuerpo: vacío.                       respuesta: LIST                        */
#define OP_US_LIST            0x01
/* cuerpo: sensor(1).                   respuesta: MEASURE                     */
#define OP_US_ONESHOT         0x02
/* cuerpo: sensor(1), periodo_ms(2).    respuesta: ACK y después MEASURE
 *                                      cada periodo_ms                        */
#define OP_US_PERIODIC_ON     0x03
/* cuerpo: sensor(1).                   respuesta: ACK                         */
#define OP_US_PERIODIC_OFF    0x04
/* cuerpo: sensor(1), unidad(1).        respuesta: ACK                         */
#define OP_US_SET_UNIT        0x05
/* cuerpo: sensor(1), retardo_ms(2).    respuesta: ACK                         */
#define OP_US_SET_DELAY       0x06
/* cuerpo: sensor(1).                   respuesta: STATUS                      */
#define OP_US_STATUS          0x07

/* Pantalla OLED */
/* cuerpo: línea(1), texto(1 a 32 caracteres, sin terminador).
 * La línea 0 es la primera.            respuesta: ACK                         */
#define OP_OLED_TEXT          0x10
/* cuerpo: vacío.                       respuesta: ACK                         */
#define OP_OLED_CLEAR         0x11
/* cuerpo: modo(1).                     respuesta: ACK                         */
#define OP_OLED_MODE          0x12

#define PROTO_OLED_TEXT_MAX   32

/* ----- Respuestas (sensor -> supervisor) ----------------------------------- */
/* cuerpo: código de la petición(1)                                            */
#define OP_RSP_ACK            0x80
/* cuerpo: código de la petición(1), código de error(1)                        */
#define OP_RSP_ERROR          0x81
/* cuerpo: cantidad(1) y, por cada sensor: número(1), dirección I2C(1)         */
#define OP_RSP_LIST           0x82
/* cuerpo: sensor(1), dirección I2C(1), unidad(1), retardo_ms(2),
 *         periódico(1: 0 no, 1 sí), periodo_ms(2)                             */
#define OP_RSP_STATUS         0x83
/* cuerpo: sensor(1), unidad(1), valor(2)                                      */
#define OP_RSP_MEASURE        0x84

/* ----- Unidades ------------------------------------------------------------ */
#define PROTO_UNIT_INC        0         // pulgadas
#define PROTO_UNIT_CM         1         // centímetros
#define PROTO_UNIT_MS         2         // comando "ms": tiempo del eco en
                                        // microsegundos, como lo da el SRF02

/* ----- Modos de la pantalla ------------------------------------------------ */
#define PROTO_OLED_MANUAL     0         // solo muestra lo que se escribe con text
#define PROTO_OLED_AUTO       1         // muestra el estado de los sensores

/* ----- Códigos de error ---------------------------------------------------- */
#define PROTO_ERR_UNKNOWN_OP  1         // código de operación desconocido
#define PROTO_ERR_NO_SENSOR   2         // el sensor no existe
#define PROTO_ERR_BAD_PARAM   3         // parámetro inválido
#define PROTO_ERR_I2C         4         // fallo de I2C con el SRF02 o la pantalla
#define PROTO_ERR_MALFORMED   5         // mensaje mal formado

/* ----- Leer un valor de 16 bits (byte bajo primero) ------------------------ */
// p es un puntero al primer byte
#define PROTO_GET16(p)        ((p)[0] | ((p)[1] << 8))

#endif
