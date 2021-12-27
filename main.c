/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include "project.h"

#define and &&
#define or ||

#define fila_1_byte_0 0x00000000
#define fila_2_byte_0 0x00000010
#define fila_3_byte_0 0x00000020

#define variable Variables_1.Variable_1

/****Variables para el control general del Sistema****/
typedef struct Tiempo{                                                          // Estructura con las variables de tiempo
    uint16 ms:10;                                                               // que permiten controlar freuencia de muestreo
    uint16 seg:3;
}Tiempo;
Tiempo tiempo;

typedef union Banderas_1{                                                       // Unión entre una variable de reseteo
    struct Variables1{                                                          // y variables generales para el control del sistema
        uint16 estado:3;  
        uint16 tecla_presionada:1;
        uint16 dato_UART_recibido:1;
        uint16 contador:5;
        uint16 unidades_temperatura:1;
        uint16 unidades_presion:1;
        uint16 signo:1;
        uint16 bienvenida_actualizada:1;
    }Variable_1;
    uint16 resetear;
}banderas_1;
banderas_1 Variables_1;

/****Variables para el control del ADC****/
typedef struct Medidas{                                                         // Estructuras que almacena los valores de las variables sensadas
    uint32 acumulado_temperatura:17;
    uint32 acumulado_presion:17;
    uint32 presion:15;                                                           
    uint32 temperatura:13;                                                    
}medidas;
medidas medida;

/****Variables para el manejo del teclado****/
unsigned char tecla;

/****Variables de Comunicacion Serial****/
unsigned char dato;

/****Variables de la EEPROM****/
char mensaje_bienvenida[] = "                    ";
char fecha[] = "15/07/2004";
uint8 i = 0;
uint8 contador_letras;
    
/****Rutinas del Sistema****/
void menu(uint8 Menu);
void teclado_matricial(void);
void comunicacion_serial(void);
void sensar(void);
void graficar(void);
void actualizar_fecha(const char *letra);
void actualizar_mensaje_bienvenida(const char *letra);

/****Funciones de Interrupcion del Sistema****/
CY_ISR_PROTO(teclado);
CY_ISR_PROTO(UART_Rx);
CY_ISR_PROTO(cronometro);

int main(void)
{
    CyGlobalIntEnable;
    
    /****Inicializacion de Interrupciones****/
    isr_KBI_StartEx(teclado);
    isr_Rx_StartEx(UART_Rx);
    isr_contador_StartEx(cronometro);
    
    /****Inicializacion de Componentes****/
    EEPROM_Start();
    Teclado_Start();
    LCD_Start();
    UART_Start();
    AMux_Start();
    Contador_Start();
    ADC_Start();
    
    /**** Actualizacion de Fecha****/
    if (EEPROM_ReadByte(fila_1_byte_0) != 0){
        for (i=0; i<10; i++) fecha[i] = EEPROM_ReadByte(fila_1_byte_0+i);
    }
    
    /**** Actualizacion de mensaje de bienveida****/
    if (EEPROM_ReadByte(fila_3_byte_0) != 0){
        variable.bienvenida_actualizada = 1;
        contador_letras = EEPROM_ReadByte(fila_3_byte_0);
        
        for (i=0; i<contador_letras; i++) mensaje_bienvenida[i] = EEPROM_ReadByte(fila_2_byte_0+i);
    }
    
    LCD_ClearDisplay();
    variable.estado = 0;
    menu(0); 
    
    
    for(;;)
    {
        /****Interaccion con el teclado matricial****/
        if (variable.tecla_presionada == 1){                                   
            variable.tecla_presionada = 0;                                      
            teclado_matricial();
        }
        
        /****Interaccion con el puerto serial****/
        if (variable.dato_UART_recibido == 1){
            variable.dato_UART_recibido = 0;
            comunicacion_serial();
        }
        
        /****Rutina de Sensado****/
        if (variable.estado == 2){
            if (tiempo.ms%25 == 0) {                                            // En esta rutina se toman 20 muestras en medio segundo
                sensar();                                                       // y se envian al LCD y a la Interfaz cada medio segundo 
            }                                                                   // para su visualizacion
            if (tiempo.ms%500 == 0) {
                menu(3);   
                graficar();
            }
        }
    }
}

void actualizar_mensaje_bienvenida(const char *letra){                          // Un apuntador recorre la cadena de caracteres pasada 
                                                                                // por referenia, almacenando sus caracteres en la fila 2 y 3 de 
    for (i = 0; i < contador_letras; i++){                                      // la memoria EEPROM.
        if (*letra != 0){                                                       // NOTA: Solo recorre la cadena hasta el valor "contador letras"
            EEPROM_WriteByte(*letra, fila_2_byte_0 + i);                        // El cual indica la longitud del mensaje actualizado que se guardó
            letra++;                                                            // previamente en la memoria EEPROM en la fila 3.
        }   
    }
    
    UART_PutString("Mensaje modificado");                                       // Además se Envía a la pantalla de comandos y se enciende
    variable.bienvenida_actualizada = 1;                                        // una bandera que indica que el mensaje se modificó con exito
}

void actualizar_fecha(const char *letra){                                       // La cadena de caracteres pasada por referencia, que contiene 
    i = 0;                                                                      // la fecha, se recorre con un apuntador, que almacena en la 
    for (i = 0; i < 10; i++){                                                   // memoria EEPROM, cada uno de sus caracteres.
        EEPROM_WriteByte(*letra,fila_1_byte_0 + i);
        LCD_Position(2,i+5);LCD_PutChar(*letra);                                // Nota: La fecha se almacena en la fila 1 de la EEPROM
        letra++;
    }
    i = 0; // Reiniciar i, Permite enviar fechas consecutivas sin alterar la funcionalidad
}

void graficar(void){                                                            // Se envían por Serial los digitos de la temperatura y la presion
    if (variable.unidades_temperatura == 0){                                    // si sus unidades se encuentran en Celcius y Kelvin respectivamente
        UART_PutChar('T');
        UART_PutChar(variable.signo + 48);                                      // Formatos:
        UART_PutChar((medida.temperatura/1000)%10 + 48);                        // Temperatura: Bandera + Signo + 4 Digitos
        UART_PutChar((medida.temperatura/100)%10 + 48);                         // Presion: Bandera + 3 Digitos
        UART_PutChar((medida.temperatura/10)%10 + 48);
        UART_PutChar(medida.temperatura%10 + 48);
        UART_PutChar(0);
        
    }
    if (variable.unidades_presion == 0){
        UART_PutChar('P');
        UART_PutChar((medida.presion/100)%10 + 48);
        UART_PutChar((medida.presion/10)%10 + 48);
        UART_PutChar(medida.presion%10 + 48);
        UART_PutChar(0);
        
    }
    
}

void sensar(void){
    variable.contador++;                                                        // Esta variable cuenta las veces que se sensan las variables
                                                                        
    AMux_Select(0);                                                             // Rutina de sensado para la temperatura
    ADC_StartConvert();                                                         
    ADC_IsEndConversion(ADC_WAIT_FOR_RESULT);
    medida.acumulado_temperatura += ADC_GetResult16();                          
    
    AMux_Select(1);                                                             // Rutina de sensado para la presion
    ADC_StartConvert();
    ADC_IsEndConversion(ADC_WAIT_FOR_RESULT);
    medida.acumulado_presion += ADC_GetResult16();                   
   
    if (variable.contador == 20){                                               // Cuando se ha sensado 20 veces
        variable.contador = 0;                                                  // se hace un promedio de los 
        medida.acumulado_temperatura/=20;                                       // registros
        medida.acumulado_presion/=20;
        
        /****Bits leidos en cada trimer para validar el funcionamiento****/
        //LCD_Position(3,0);LCD_PrintString("ADC1:"); LCD_PrintNumber(medida.acumulado_temperatura);
        //LCD_Position(3,11);LCD_PrintString("ADC2:"); LCD_PrintNumber(medida.acumulado_presion);
        
        /****Rutina de Calculo de Temperatura****/
        if (medida.acumulado_temperatura < 491.4){
            if (medida.acumulado_temperatura == 1) medida.acumulado_temperatura = 0;
            medida.temperatura = (uint32)10*(491.4 - medida.acumulado_temperatura)*160/3931.2;
            variable.signo = 1;
        }
        else if (medida.acumulado_temperatura >= 491.4 and medida.acumulado_temperatura <= 3931.2){
            medida.temperatura = (uint32)10*(medida.acumulado_temperatura - 491.4)*160/3931.2;
            variable.signo = 0;
        }
        else{
            medida.temperatura = 10*140;
            variable.signo = 0;
        }
        
        /****Rutina de Calculo de Presion****/
        if (medida.acumulado_presion <= 3685.5){
            medida.presion = (uint32)10*medida.acumulado_presion*30/3685.5;
        }
        else{
            medida.presion = 10*30;
        }
        
        /****RUTINA PARA CAMBIO DE UNIDADES****/                        
        if (variable.unidades_temperatura == 1){                              
            if (variable.signo == 0) medida.temperatura = (medida.temperatura + 2731.5);
            else medida.temperatura = (2731.5 - medida.temperatura);
        }
        if (variable.unidades_presion == 1){
            medida.presion = (uint32)100*(medida.presion*100/133.322387415);
        }
        
        medida.acumulado_temperatura = 0;
        medida.acumulado_presion = 0;
    }
}

void comunicacion_serial(void){
    
    /****Al recibir un dato por comunicacion serial, se analiza el estado del sistema para determinar las rutinas de accion a realizar****/
    switch (variable.estado){
        case 0:
            if (dato == 'I'){                                                   // I: Inicio del Sistema: Se muestra el mensaje de bienvenida,
                menu(1);                                                        // Se responde mediante comunicacion serial, y luego de 4 
                UART_PutString("ok, inicio");                                   // segundos, se presenta el menu principal del sistema
                CyDelay(4000); 
                menu(2);
                variable.estado = 1;                                            // Estado = 1: Menu Principal
            }
            break;
            
        case 2:                                                                 // Estado = 2: Monitoreo Remoto
            if (dato == 0){                                                     // En este estado, cada que se reciba un 0, se cambian las unidades
                variable.unidades_temperatura = ~variable.unidades_temperatura; // de temperatura; y de presion si se recibe un 1.
                if (variable.unidades_temperatura == 0) UART_PutString("Ok,Celcius");
                else UART_PutString("Ok,Kelvin");                               // Ademas, se responde por comunicacion serial, segun corresponda
            }                                                                   // el cambio de unidades
            if (dato == 1){
                variable.unidades_presion = ~variable.unidades_presion;
                if (variable.unidades_presion == 0) UART_PutString("Ok,kPa");
                else UART_PutString("Ok,mmHg");
            }
            break;
            
        case 3:                                                                 // Estado = 3: Actualizar Fecha
            fecha[i++] = dato;                                                  // Se almacena cada caracter en la variable fecha, y cuando
            if (dato == 0){                                                     // se reciba el caracter nulo que indica el fin de la cadena,
                UART_PutString("ok,fecha");                                     // se llama la funcion actualizar fecha
                actualizar_fecha(fecha);
            }
            break;
            
        case 4:                                                                 // Estado = 4: Actualizar Mensaje de Bienvenida
            if (dato == 0) {                                                    // Los datos diferentes de nulo se almacenan en la variable
                                                                                // "mensaje_bienvenida", y al llegar el caracter nulo que indica
                /****Analisis de longitud del mensaje ingresado****/            // el final de la cadena, se analiza la cantidad de datos en ella,    
                if (i > 3 and i < 21){                                          // de tal forma que solo si hay entre 4 y 20 datos se actualiza
                    contador_letras = i;                                        // el mensaje de bienvenida. De lo contrario se responde por 
                    EEPROM_WriteByte(contador_letras, fila_3_byte_0);           // comunicacion serial, "mensaje fuera de rango"
                    actualizar_mensaje_bienvenida(mensaje_bienvenida);
                }
                else { 
                    i = 0;
                    UART_PutString("Msg Fuera de rango");
                    if (EEPROM_ReadByte(fila_3_byte_0) != 0){
                        for (i=0; i<contador_letras; i++) mensaje_bienvenida[i] = EEPROM_ReadByte(fila_2_byte_0+i);
                    }
                }
            }
            /****Almacenamiento en memoria del mensaje modificado****/
            else {
                mensaje_bienvenida[i++] = dato; // Se almacena cada caracter en la variable mensaje_nuevo
                LCD_Position(2,i-1); LCD_PutChar(dato);
            }
            break;
    }
}

void teclado_matricial(void){                                                   // Al Interactuar con el teclado, se analiza el 
                                                                                // estado en el que se encuentra el sistema.
    tecla = Teclado_teclaPresionada();                                          // En funcion del estado se ejecuta alguna de las
                                                                                // tareas, o se regresa en el menú
    if (variable.estado == 1 and tecla == '#'){
        i = 0;
        variable.estado = 0;
        menu(0);
    }
    else if (variable.estado != 1 and tecla == '#'){
        i = 0;
        variable.estado = 1;
        menu(2);
    }
    
    switch (variable.estado){
        
        /****Si el sistema esta en el estado 1 (menu principal) verifica la tarea seleccionada****/
        case 1: 
            if (tecla == '1'){ // Monitoreo remoto
                variable.estado = 2;
            }
            else if (tecla == '2'){ // Fecha
                i = 0;
                variable.estado = 3;
                menu(4);
            }
            else if (tecla == '3'){ // Mensaje Inicial
                i = 0;
                variable.estado = 4;
                menu(5);
            }
            else if (tecla == 'D') {
                UART_PutString("Reset -> EEPROM");
                variable.bienvenida_actualizada = 0;
                EEPROM_EraseSector(0); // Reinicia Sistema
            }
            break;
    }
}

void menu(uint8 Menu){                                                          // Rutinas de Visualizacion en el LCD
    switch(Menu){
        case 0:                                                                 // Menu de Inicio
            LCD_ClearDisplay();
            LCD_Position(1,2); LCD_PrintString("Esperando inicio");
            LCD_Position(2,6); LCD_PrintString("desde PC");
            break;
            
        case 1:                                                                 // Menu de bienvenida que se muestra al recibir una 'I'
            LCD_ClearDisplay();                                                 // mientras el sistema esta en estado 0. 
            if (variable.bienvenida_actualizada == 0){
                LCD_Position(0,5); LCD_PrintString("BIENVENIDO");               // Si hay un mensaje de bienvenida almacenado en la EEPROM
                LCD_Position(1,5); LCD_PrintString("AL SISTEMA");               // Se presenta, en lugar del mensaje original
            }
            else{
                for (i = 0; i < contador_letras; i++){
                    LCD_Position(0,i); LCD_PutChar(mensaje_bienvenida[i]);
                }
            }
            break;
            
        case 2:                                                                 // Menu Principal
            LCD_ClearDisplay();
            LCD_Position(0,6); LCD_PrintString("SISTEMA");
            LCD_Position(1,0); LCD_PrintString("1.Monitoreo remoto");
            LCD_Position(2,0); LCD_PrintString("2.Fecha");
            LCD_Position(3,0); LCD_PrintString("3.Mensaje inicial");
            break;
            
        case 3:                                                                 // Menu de visualizacion de variables
            LCD_ClearDisplay(); 
            LCD_Position(0,0); LCD_PrintString("SISTEMA DE MEDICION");
            
            /**** Rutina de visualizacion de Temperatura****/
            if (variable.unidades_temperatura == 0){ // °C
                if (variable.signo == 0) {
                        LCD_Position(1,0); LCD_PrintString("Temp: ");
                    }
                else {
                    LCD_Position(1,0); LCD_PrintString("Temp:-");
                }
                LCD_Position(1,12); LCD_PutChar(LCD_CUSTOM_0); LCD_PrintString("C");
            }
            else{ // ° K
                LCD_Position(1,0); LCD_PrintString("Temp: ");
                LCD_Position(1,12); LCD_PutChar(LCD_CUSTOM_0); LCD_PrintString("K");
            }
            
            LCD_Position(1,6); LCD_PrintNumber(medida.temperatura/10); LCD_PutChar('.'); LCD_PrintNumber(medida.temperatura%10);
            LCD_Position(2,6); LCD_PrintNumber(medida.presion/10); LCD_PutChar('.'); LCD_PrintNumber(medida.presion%10);
            
            /**** Rutina de visualizacion de Presion****/
            if (variable.unidades_presion == 0){ // kPa
                LCD_Position(2,0); LCD_PrintString("Pres: ");
                LCD_Position(2,12); LCD_PrintString("kPa");
                
                LCD_Position(2,6); LCD_PrintNumber(medida.presion/10); LCD_PutChar('.'); LCD_PrintNumber(medida.presion%10);
            }
            else{       // mmHg
                LCD_Position(2,0); LCD_PrintString("Pres: ");
                LCD_Position(2,13); LCD_PrintString("mmHg");
                
                LCD_Position(2,6); LCD_PrintNumber(medida.presion/100); LCD_PutChar('.'); 
                if (medida.presion%100 < 10) LCD_PutChar('0');
                LCD_PrintNumber(medida.presion%100);
            }
            break;
            
        case 4:                                                                 // Menu de visualizacion de la fecha almacenada
            LCD_ClearDisplay();                                                 // en la EEPROM
            LCD_Position(1,7); LCD_PrintString("Fecha");
            LCD_Position(2,5); LCD_PrintString(fecha);
            break;
            
        case 5:                                                                 // Menu de espera del mensaje de bienvenida que sera
            LCD_ClearDisplay();                                                 // actualizado
            LCD_Position(0,1);LCD_PrintString("Ingrese el mensaje");
            LCD_Position(1,3);LCD_PrintString("desde el PC:");
            break;
    }
}

CY_ISR(teclado){                                                                // Interrupcion del teclado matricial
    variable.tecla_presionada = 1;
}

CY_ISR(UART_Rx){                                                                // Interrupcion de la comunicacion Serial
    dato = UART_ReadRxData();
    variable.dato_UART_recibido = 1;
    LCD_Position(0,0); LCD_PutChar(dato);
}

CY_ISR(cronometro){                                                             // Interrupcion del Cronometro
    tiempo.ms++;      
    if (tiempo.ms == 1000) {
        tiempo.ms = 0;
        tiempo.seg++;
    }
}

/* [] END OF FILE */
