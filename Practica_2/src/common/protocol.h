
#define PROTO_BAUD            115200    // Serial1, 8N1
#define PROTO_MAX_BODY        8        // cuerpo máximo de una petición

#define COMMAND_POSITION    7
#define SENSOR_POSITION     5
#define MESSAGE_POSITION    4

// -----X----
#define OP    0b0 << COMMAND_POSITION
#define ERROR 0b0 << COMMAND_POSITION
#define RESP  0b1 << COMMAND_POSITION

// -----ZXX----
#define sensores 0b11 << SENSOR_POSITION //2 sensores
#define sensor1  0b01 << SENSOR_POSITION //1 sensores
#define sensor2  0b10 << SENSOR_POSITION //2 sensores
#define pantalla 0b00 << SENSOR_POSITION // pantalla
//--ZZZX----
#define mensaje    0b1 << MESSAGE_POSITION
#define no_mensaje 0b0 << MESSAGE_POSITION
//---ZZZZ Esto se va a desplazar <<4 bits para que quepa el mensaje al final--
#define US_LIST            0x0
#define US_ONESHOT         0x1
#define OP_US_PERIODIC_OFF 0x2
#define OP_STATUS          0x4
#define OP_OLED_OFF        0x8
#define OP_OLED_ON         0x9
#define OP_OLED_CLEAR      0xA

#define OP_US_SET_UNIT_1   0xC
#define OP_US_SET_UNIT_2   0xD
#define OP_US_SET_UNIT_3   0xE

// Necesitan mensajes
#define OP_US_PERIODIC_ON  0x3
#define OP_US_SET_DELAY    0x6


/* ----- Códigos de error controlados sensor que envía al supervisor---------------------------------------------------- */
#define PROTO_ERR_I2C_SENSOR1         1
#define PROTO_ERR_I2C_SENSOR2         2
#define PROTO_ERR_I2C_SENSORES        3
#define PROTO_ERR_I2C_PANTALLA        4

//ACK (no creo que haga falta)
#define ACK 0x7E
#define NO_ACK 0x11


static inline uint8_t generateHead(uint8_t command,uint8_t device,uint8_t contain_message, operation, message){
    return command | device | contain_message | operation;
}

static inline uint8_t generateMessage(uint8_t command,uint8_t device,uint8_t contain_message, operation, message){
    return command | device | contain_message | operation;
}

static inline uint8_t generateError(uint8_t command,uint8_t device,uint8_t contain_message, operation, message){
    return command | device | contain_message | operation;
}


















/* ----- Códigos de error controlados solo por el supervisor y no se lo envia al sensor---------------------------------------------------- */
#define PROTO_ERR_UNKNOWN_OP  1         // código de operación desconocido
#define PROTO_ERR_NO_EXIST_SENSOR   2         // el sensor no existe
#define PROTO_ERR_BAD_PARAM   3         // parámetro inválido
#define PROTO_ERR_MALFORMED   5         // mensaje mal formado



// Esto no hace falta para  creo porque se envía solo 8 bits
/* ----- Leer un valor de 16 bits (byte bajo primero) ------------------------ */
// p es un puntero al primer byte
#define PROTO_GET16(p)        ((p)[0] | ((p)[1] << 8))

/* ----- Escribir un valor de 16 bits (byte bajo primero) -------------------- */
// p es un puntero al primer byte, v el valor
#define PROTO_PUT16(p, v)     ((p)[0] = (v) & 0xFF, (p)[1] = ((v) >> 8) & 0xFF)

