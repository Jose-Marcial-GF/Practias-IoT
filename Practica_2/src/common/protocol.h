#pragma once
#include <stdint.h>
#include <Arduino.h>

#define PROTO_BAUD            115200    // Serial1, 8N1
static const uint8_t HEAD_SIZE = 1;
static const uint8_t PROTO_MAX_BODY =        8;        // cuerpo máximo de una petición

static const uint8_t COMMAND_POSITION =    7;
static const uint8_t SENSOR_POSITION =     5;
static const uint8_t MESSAGE_POSITION =    4;

// -----X----
static const uint8_t OP =    0b0 << COMMAND_POSITION;
static const uint8_t ERROR = 0b0 << COMMAND_POSITION;
static const uint8_t RESP =  0b1 << COMMAND_POSITION;

// -----ZXX----
static const uint8_t SENSORES = 0b11 << SENSOR_POSITION; //2 sensores;
static const uint8_t SENSOR1 =  0b01 << SENSOR_POSITION; //1 sensores;
static const uint8_t SENSOR2 =  0b10 << SENSOR_POSITION; //2 sensores;
static const uint8_t PANTALLA = 0b00 << SENSOR_POSITION; // pantalla;
//--ZZZX----
static const uint8_t MESSAGE =    0b1 << MESSAGE_POSITION;
static const uint8_t NO_MESSAGE = 0b0 << MESSAGE_POSITION;
//---ZZZZ-XXXX--
static const uint8_t US_LIST =            0x0;
static const uint8_t US_ONESHOT =         0x1;
static const uint8_t OP_US_PERIODIC_OFF = 0x2;
static const uint8_t OP_STATUS =          0x4;
static const uint8_t OP_OLED_OFF =        0x8;
static const uint8_t OP_OLED_ON =         0x9;

static const uint8_t PROTO_UNIT_INC =   0xC;
static const uint8_t PROTO_UNIT_CM =   0xD;
static const uint8_t PROTO_UNIT_MS =   0xE;

// Necesitan mensajes
static const uint8_t OP_US_PERIODIC_ON =  0x3;
static const uint8_t OP_US_SET_DELAY =    0x6;



/* ----- Códigos de error controlados sensor que envía al supervisor---------------------------------------------------- */
static const uint8_t PROTO_ERR_I2C_SENSOR1 =         1;
static const uint8_t PROTO_ERR_I2C_SENSOR2 =         2;
static const uint8_t PROTO_ERR_I2C_SENSORES =        3;
static const uint8_t PROTO_ERR_I2C_PANTALLA =        4;
static const uint8_t PROTO_ERROR_BUSY =              5;


static const uint16_t MIN_DELAY_SFR = 70;
static const uint16_t MAX_DELAY_SFR = 60000;   // 1 minuto

static uint8_t writeMessage(uint8_t * body, uint8_t len)
{
  Serial1.write(body, len);
  return 0;
}

// Nombre corto de una unidad del protocolo. PROTO_UNIT_MS es el tiempo de
// vuelo, que el SRF02 devuelve en microsegundos
static inline const char *unitName(uint8_t unit)
{
    switch (unit) {
        case PROTO_UNIT_INC: return "in";
        case PROTO_UNIT_CM:  return "cm";
        case PROTO_UNIT_MS:  return "us";
    }
}

static inline uint8_t generateHead(uint8_t command, uint8_t device,
                                   uint8_t contain_message, uint8_t operation)
{
    return command | device | contain_message | operation;
}

static inline uint8_t sendHead(uint8_t command, uint8_t device,
                               uint8_t contain_message, uint8_t operation)
{
    uint8_t head = generateHead(command, device, contain_message, operation);
    return writeMessage(&head, HEAD_SIZE);
}


static inline uint8_t sendMessage(uint8_t command,uint8_t device,uint8_t contain_message, uint8_t operation, uint8_t* message, uint8_t len){
    uint8_t head = command | device | contain_message | operation;
    uint8_t body[len + HEAD_SIZE];
    body[0] = head;
    for (size_t index = HEAD_SIZE; index < len + HEAD_SIZE; index++)
    {
        body[index] = message[index - HEAD_SIZE];
    }
    return writeMessage(body, len+ HEAD_SIZE);
}



static inline uint8_t generateError(uint8_t command,uint8_t device,uint8_t contain_message, uint8_t operation, uint8_t message){
    return command | device | contain_message | operation;
}


















/* ----- Códigos de error controlados solo por el supervisor y no se lo envia al sensor---------------------------------------------------- */
#define PROTO_ERR_UNKNOWN_OP  1         // código de operación desconocido
#define PROTO_ERR_NO_EXIST_SENSOR   2         // el sensor no existe
static const uint8_t PROTO_ERR_BAD_PARAM =   3;         // parámetro inválido
#define PROTO_ERR_MALFORMED   5         // mensaje mal formado



// Esto no hace falta para  creo porque se envía solo 8 bits
/* ----- Leer un valor de 16 bits (byte bajo primero) ------------------------ */
// p es un puntero al primer byte
#define PROTO_GET16(p)        ((p)[0] | ((p)[1] << 8))

/* ----- Escribir un valor de 16 bits (byte bajo primero) -------------------- */
// p es un puntero al primer byte, v el valor



#define PROTO_PUT16(p, v)     ((p)[0] = (v) & 0xFF, (p)[1] = ((v) >> 8) & 0xFF)

