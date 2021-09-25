// Librerías:

#include <Arduino.h>
// ======= Para display LCD =======
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>	// Librería de Frank de Brabander
// ======= Para placa PCF8574T =======
#include <I2CKeyPad.h>	// Librería de Rob Tillaart
// ======= Para lector RFID =======
#include <MFRC522.h>
#include <Dictionary.h>   //Libreria de Anatoli Arkhipenko
// ======= Para memoria EEPROM =======
#include <EEPROM.h>
// ======= Para WiFi =======
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
// ======= Para ADC HX711 =======
#include <Q2HX711.h>	// Librería de  Scott Russell
// ======= Para impresora =======
#include <Adafruit_Thermal.h>

/* === Variable para debugg === */
// #define __DEBUGG (bool) false
#ifndef	__DEBUGG
#define __DEBUGG (bool) true
#endif

/* Almacenamiento en "EEPROM" */
#define TAMANO_DATO 5	// Cantidad de bytes que ocupa en la memoria un dato completo (id_trabajador + peso_tarado)
#define DATOS_MAXIMOS_EEPROM (uint16_t)799  // Maxima cantidad de datos que se pueden almacenar.					

/* Pines para comunicacion I2C */
#define	SDA	(uint8_t)4		// Pin de datos de I2C
#define	SCL (uint8_t)5		// Pin de reloj de I2C

/* Pines para comunicacion con HX711 */
#define HX711_DATA_PIN (uint8_t)2		// Pin de data para HX711  
#define HX711_CLOCK_PIN (uint8_t)16  	// Pin de reloj para HX711 

/* Para placa PCF8574 */
#define TECLADO_ADDRESS (uint8_t)0x20	// Dirección de la placa PCF8574T

/* Para LCD */
#define LCD_ADDRESS (uint8_t)0x27		// Dirección deL display LCD

/* Parametros para WiFi*/
#define WIFI_DISCONNECT_GPIO (uint8_t) 0	/* Pin del boton de desconexion de WiFi */
#define SSID (const char *) "BASCULA"
#define PASS (const char *) "123456789"
#define BD_URL (String) "http://irresponsible-toolb.000webhostapp.com/my_php/dbwrite_bascula.php"



