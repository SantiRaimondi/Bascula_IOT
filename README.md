# Bascula_IOT
Repository with information and files of the IOT Scale project
---

## Índice / Index:
1. [Español](#español).
	1.  [Descripcion general de funcionamiento](#descripcion-general-de-funcionamiento).
	2.  [Esquemático](#esquemático).
	3.  [PCB](#pcb).
	4.  [Vista 3D](#vista-3d).
3. [English](#english).
	1. [Functional description](#functional-description).
	2. [Schematic](#schematic).
	3. [PCB layout](#pcb-layout).
	4. [3D View](#3d-view).


### Español

#### Descripcion general de funcionamiento

Cuando se inicia la placa, si no fue anteriormente conectada a una red WiFi
se habilita un portal AP para configurar el dispositivo al cual se ingresa cuando 
se conecta a la red privada del dispositivo llamada BASCULA y cuya contraseña es 
123456789 (parametros configurables SSID, PASS). 

Independientemente de la conexion al WiFi que sirve para enviar los datos a una 
de datos (parametro configurable BD_URL), se inician los perificos y se procede
loop.

En el loop se hace la medicion del peso y en caso de que el usuario presione los
botones correspondientes se comienzan a solicitar los datos del recolector (por 
teclado o tarjeta RFID) y luego a la transmision de los datos a la BD (o almacenamiento
en EEPROM hasta tener conexion y se puedan transmitir). La transmision se hace utilizando
protocolo HTTP.

En caso de estar conectado a una red WiFi y se desee desconectar, hay un boton
fisico para tal proposito, que borra las credenciales ingresadas anteriormente e 
inicia el AP para configurar nuevamente el dispositivo. Si se desconecta de forma 
involuntaria (ej. corte del ISP), el dispositivo se intentara reconectar a la ultima
red guardada.

Detalles importantes sobre conexionado a la placa:

	+) Puertos del nodeMCU ESP8266-E:
		D1 (GPIO5): SCL (I2C)	-> Usado por LCD y placa PCF8574T
		D2 (GPIO4): SDA (I2C)	-> Usado por LCD y placa PCF8574T
		D5 (GPIO14):SCLK (SPI)	-> Usado por dispositivo RFID
		D6 (GPIO12):MISO (SPI)	-> Usado por dispositivo RFID
		D4 (GPIO2): LED principal. Pin en PullUp interno	-> Usado por HX711 (data_pin)
		D8 (GPIO15): PullDown. CS (SPI)	-> Usado por dispositivo RFID
		D3 (GPIO0): PullUp		-> Boton desconexion WiFi
		D7 (GPIO13): MOSI (SPI)	-> Usado por dispositivo RFID
		D0 (GPIO16) Segundo LED de la placa	-> Usado por HX711 (clk_pin)
		Rx	->	Tx de la impresora
		Tx	-> 	Rx de la impresora
		
	+) IMPORTANTE sobre display LCD:
		Calibrar correctamente el brillo.
		Si se enciende pero no aparece texto (y se calibro correctamente el brillo),
		cambiar en "LiquidCrystal_I2C lcd(0x27,20,4);" el 0x27 por 0x3f.
		
	+) Configurar el valor de la masa de calibración a usar (variable: masa_calibracion).

	+) Información de utilidad sobre la placa con el PCF8574T:
  
		Con el dipswich se cambia la dirección del dipositivo:
    
            Posicion  |	Direccion de 
            del DipSw |	la placa
        _________________________
          000	  |   0x20
          001	  |	  0x21
          010	  |	  0x22
          011	  |	  0x23
          100	  |	  0x24
          101	  |	  0x25
          110	  |	  0x26
          111	  |	  0x27
      
		Conexión de la placa:
			pines p0-p3 filas
			pines p4-p7 columnas 
	
	+) Mapeo de memoria EEPROM: Tamaño de dato = 7 Bytes (2->ID 5->Peso)
		N° byte		|	Informacion
		_____________________________

		   100		|	Conectarse (bandera que indica conexion a red WiFi)
		   101		|	cant_datos_guardados_H
		   102		|	cant_datos_guardados_L
		   103		|	Datos que se guardaron si se perdió conexión WiFi
		   4090		|		"
		_____________________________
		
		La capacidad máxima de guardar datos es de (4096-101 = 3995) bytes por lo tanto puedo 
		guardar	3995/TAMANO_DATO = 570.
	
Dependencias de librerias utilizadas (lib_deps):

    lib_deps = 
		marcoschwartz/LiquidCrystal_I2C@^1.1.4
		robtillaart/I2CKeyPad@0.1.2
		miguelbalboa/MFRC522@^1.4.8
		arkhipenko/Dictionary@^3.2.2
		queuetue/Queuetue HX711 Library@^1.0.2
		adafruit/Adafruit Thermal Printer Library@^1.4.0
		https://github.com/tzapu/WiFiManager.git

#### Esquemático

![sch](https://github.com/SantiRaimondi/Bascula_IOT/blob/main/img/sch.jpg?raw=true)


#### PCB 

![pcb](https://github.com/SantiRaimondi/Bascula_IOT/blob/main/img/pcb.jpg?raw=true)

#### Vista 3D

![3d](https://github.com/SantiRaimondi/Bascula_IOT/blob/main/img/3dpcb.jpg?raw=true)

### English

#### Functional description
  
  When the board begins, it checks for WiFi networks in the area, if it doesn't find
  a networks whose credentials have been stored previously it starts an AP portal to 
  configure the device. To connect to the SoftAP, the user has to connect a device (mobile 
  phone or computer) to a WiFi network called BASCULA, which has a password (123456789).
  Both parameters are configurables in the device firmware (SSID, PASS).
  
  Independently of the WiFi connection that is needed in order to send data to the database
  (that is a configurable parameter BD_URL), the other peripherals are iniciated and the 
  main loop begins.
  
  In the loop the weight is measured and if the user pushes the corresponding buttons, 
  the device ask the user to log in the workers' data (using a keypad or an RFID card). 
  Then it transmits the data to the DB or stores them in the local EEPROM until a proper
  WiFi connection is stablished. The transmition has been implemented using HTTP protocol.
  Then, it prints a receipt for the worker.
  
  if there is a WiFi connection and the user wants to disconnect, there is an external button 
  configured to be used as a connection reset. It basically erases the credentials previously 
  configured and starts a new SoftAP portal. If the connection is lost involuntarily (eg. external
  connection lost), the device will try to reconnect to the las saved network.
	
Important details about the board connections:

	+) NodeMCU ESP8266-E ports connections:
		D1 (GPIO5): SCL (I2C)	-> Used by LCD and PCF8574T I/O extension board.
		D2 (GPIO4): SDA (I2C)	-> Used by LCD and PCF8574T I/O extension board.
		D5 (GPIO14):SCLK (SPI)	-> Used by RFID module.
		D6 (GPIO12):MISO (SPI)	-> Used by RFID module.
		D4 (GPIO2): Main LED. Internal PullUp	-> Used by HX711 ADC module (data_pin).
		D8 (GPIO15): PullDown. CS (SPI)	-> Used by RFID module.
		D3 (GPIO0): PullUp		-> WiFi disconnect pushbutton.
		D7 (GPIO13): MOSI (SPI)	-> Used by RFID module.
		D0 (GPIO16) Secondary onboard LED	-> Used by HX711 ADC module (clk_pin).
		Rx	->	Thermal printer Tx.
		Tx	-> 	Thermal printer Rx.
		
	+) IMPORTANT about LCD display:
    Calibrate the brightness correctly.
    If the display turns on but there is no text (and the brightness is properly calibrated),
    change in "LiquidCrystal_I2C lcd(0x27,20,4);" the 0x27 for 0x3f.

	+) Configure the mass value with the calibration mass to be used (variable: masa_calibracion).

	+)  Useful infomration about PCF8574T I/O expansion board:
  
    With the dipswich you can change the device address:
    
            DipSw    |	Board  
            position |	address
          _________________________
          000	  |       0x20
          001	  |	  0x21
          010	  |	  0x22
          011	  |	  0x23
          100	  |	  0x24
          101	  |	  0x25
          110	  |	  0x26
          111	  |	  0x27
          
		Board connection:
			pins p0-p3 rows.
			pins p4-p7 columns. 
	
	+) EEPROM memory maping: data size = 7 Bytes (2->ID 5->Peso)
		N° byte		|	Information
		_____________________________

		   100		|	Conectarse (flag that indicates is there is a WiFi connection)
		   101		|	cant_datos_guardados_H
		   102		|	cant_datos_guardados_L
		   103		|	Saved data if there is no WiFi connection
		   4090		|		"
		_____________________________
		
		The maximum capacity is (4096-101 = 3995) bytes, therefore I can save 3995/TAMANO_DATO = 570 pieces of data.
    	
Library dependencies used (lib_deps):

	lib_deps = 
		marcoschwartz/LiquidCrystal_I2C@^1.1.4
		robtillaart/I2CKeyPad@0.1.2
		miguelbalboa/MFRC522@^1.4.8
		arkhipenko/Dictionary@^3.2.2
		queuetue/Queuetue HX711 Library@^1.0.2
		adafruit/Adafruit Thermal Printer Library@^1.4.0
		https://github.com/tzapu/WiFiManager.git

#### Schematic

![sch](https://github.com/SantiRaimondi/Bascula_IOT/blob/main/img/sch.jpg?raw=true)


#### PCB layout

![pcb](https://github.com/SantiRaimondi/Bascula_IOT/blob/main/img/pcb.jpg?raw=true)

#### 3D View

![3d](https://github.com/SantiRaimondi/Bascula_IOT/blob/main/img/3dpcb.jpg?raw=true)
