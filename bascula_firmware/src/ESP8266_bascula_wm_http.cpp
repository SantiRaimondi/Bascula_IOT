 /*
	Fecha: 25/09/2021
	Autores: Santiago Raimondi, Fernando Segura Atencio @RSAsolucionesTecnologicas
	Version: 0.0.5

	Descripcion general de funcionamiento:
		Cuando se inicia la placa, si no fue anteriormente conectada a una red WiFi
	se habilita un portal AP para configurar el dispositivo al cual se ingresa cuando 
	se conecta a la red privada del dispositivo llamada BASCULA y cuya contraseña es 
	123456789 (parametros configurables SSID, PASS). 
		Independientemente de la conexion al WiFi que sirve para enviar los datos a una 
	base de datos (parametro configurable BD_URL), se inician los perificos y se procede
	al loop.
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
		D3 (GPIO0): PullUp		-> Boton desconexion WiFi.
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
			------------------------
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
			0		| SSID WiFi (ESPACIOS NO UTILIZADOS CON LA LIBRERIA WiFiManager)
		   ...		|		"	(ESPACIOS NO UTILIZADOS CON LA LIBRERIA WiFiManager
		    50		|	PASSWORD WiFi	(ESPACIOS NO UTILIZADOS CON LA LIBRERIA WiFiManager
		   ...		|		"			(ESPACIOS NO UTILIZADOS CON LA LIBRERIA WiFiManager
		   100		|	Conectarse (bandera que indica conexion a red WiFi)
		   101		|	cant_datos_guardados_H
		   102		|	cant_datos_guardados_L
		   103		|	Datos que se guardaron si se perdió conexión WiFi
		   4090		|		"
		_____________________________
		
		La capacidad máxima de guardar datos es de (4096-101 = 3995) bytes por lo tanto puedo 
		guardar	3995/TAMANO_DATO = 570.
	
	+) Dependencias de librerias utilizadas (lib_deps):
		lib_deps = 
			marcoschwartz/LiquidCrystal_I2C@^1.1.4
			robtillaart/I2CKeyPad@0.1.2
			miguelbalboa/MFRC522@^1.4.8
			arkhipenko/Dictionary@^3.2.2
			queuetue/Queuetue HX711 Library@^1.0.2
			adafruit/Adafruit Thermal Printer Library@^1.4.0
			https://github.com/tzapu/WiFiManager.git
*/

#include "MiConfig.h"

// Variables globales:

// ======= Para impresora =======
Adafruit_Thermal impresora(&Serial);  // Se pasa la dirección del puerto serie que va a usar la impresora.
// ======= Para display LCD =======
LiquidCrystal_I2C lcd(LCD_ADDRESS,20,4);	// Objeto para manejar el display LCD
// ======= Para placa PCF8574T (teclado) =======
I2CKeyPad teclado;
const char TECLAS[] = "*0#D789C456B123ANF";  // Para decodificar la tecla presionada (N = Nokey, F = Fail).
bool aceptar_presionado = false;	// Para indicar que se ingreso un valor y se puede hacer la transmision.
bool borrar_presionado = false;		// Para indicar que se cancelo la transmision.
bool bandera_numeros = false;		// Para indicar que se estan ingresando datos para 'id'.
// ======= Para receptor RFID =======
MFRC522 mfrc522;
Dictionary &usuarios = *(new Dictionary(1000));
String lectura_RFID="";
// ======= Para memoria EEPROM =======
int posicion;					// Indice de posicion del stack de memoria no volatil donde esta almacenado el ultimo dato que se guardó
uint16_t cant_datos_guardados; 	// Cantidad de datos guardados. Se almacena en la memoria en la posicion 101 y 102
uint16_t id_trabajador = 0;		// Para almacenar el id del recolector correspondiente al pesaje 
String id = "";					// Variable intermedia para ingresar por teclado.
uint8_t surco = 0;
// ======= Para WiFi =======
WiFiManager wm;
bool desconectarse = false;			// Bandera que indica si se pulso el boton de desconexion.
// ======= Para ADC HX711 =======
Q2HX711 hx711(HX711_DATA_PIN, HX711_CLOCK_PIN);	// Objeto para manejar el ADC HX711 y la celda de carga asociada
// ======= Para bascula =======
float masa_calibracion = 894.0;	      // Masa de calibracion. Ajustar valor al que se vaya a utilizar
long x1 = 0L;
long x0 = 0L;
float promedio = 10.0; 			      // Cantidad de muestras que se toman para obtener un promedio y ser más preciso
int peso = 0;					      // En gramos
int tara = 0;
bool tara_presionado = false;	      // Bandera para saber si se presionó el botón

// Prototipos de funciones:

// ======= Para memoria EEPROM =======

void iniciarEEPROM();
void grabarMediciones();
String leerMediciones();

// ======= Para WiFi =======

void iniciarWiFi();
void reconnectWifi();
IRAM_ATTR void desconectarWifi();
void checkDisconnect();

// ======= Para hacer la transmision de datos =======

void transmitirDatos();
void transmitirDatosGuardados();

// ======= Para display LCD =======

void iniciarLCD();

// ======= Para teclado y receptor RFID =======

void configPines();				
void leerTeclado();
void crearID();

// ======= Para bascula =======

void iniciarBascula();
void pesar();
void ingresarId();
void ingresarSurco();

// ======= Para impresora =======

void iniciarImpresora();
void imprimir();

// Código principal:
void setup() 
{
	Serial.begin(115200);
	Serial.println();

	// ------- Configuración de teclado ------- 
	configPines();

	// ------- Inicialización y búsqueda de datos previos en EEPROM -------
	iniciarEEPROM();

	// ------- Inicialización display LCD -------
	iniciarLCD();

	// ------- Conexion del WiFi y generacion de red propia o conexion a red guardada -------
	iniciarWiFi();

	// ------- Carga de valores de UID a memoria -------
 	crearID();

	// ------- Inicialización y calibración de la báscula -------
	// iniciarBascula();

	// ------- Inicialización impresora -------
	iniciarImpresora();

	lcd.clear();
}

void loop() 
{
	// pesar();	// Se sensa el peso

	checkDisconnect();
	wm.process();

	/* ESTA PARTE NO FUNCIONA */
	// if(!WiFi.isConnected())
	// {
	// 	/* En caso de que haya sido una desconexion involuntaria del WiFi (si se corta por 
	// 	ejemplo) se intenta reconectar */
	// 	reconnectWifi();
	// 	/* Si se desconecto a proposito se analiza la informacion del AP generado. */
	// 	wm.process();	/* Se procesa la informacion del AP */
	// }


	if(__DEBUGG)
		Serial.println("peso = " + String(peso) + " gr.");

	leerTeclado();
	
	// Impresión de mensajes en pantalla LCD
	lcd.setCursor(0,0);
	lcd.print("Peso medido: " + String((int)(peso)) + " g");
	lcd.setCursor(0,1);
	(WiFi.isConnected()) ? lcd.print("Conectado WiFi?: SI ") : lcd.print("Conectado WiFi?: NO ");

	if(cant_datos_guardados == DATOS_MAXIMOS_EEPROM)
	{
		// Avisamos visualmente en caso de memoria llena
		lcd.setCursor(0,2);
		lcd.print("NO PESAR !!!!!      ");
		lcd.setCursor(0,3);
		lcd.print("MEMORIA LLENA !!!!  ");
	}
	else
	{
		// Si se puede ingresar una nueva medicion se pide que se oprima el botón de siguiente
		lcd.setCursor(0,2);
		lcd.print("Presione siguiente  ");
		lcd.setCursor(0,3);
		lcd.print("                    ");
	}

	if(aceptar_presionado)
	{		
		aceptar_presionado = false;
		
		// Una vez presionado siguiente, procedemos a pedir que se ingrese el id_trabajador
		ingresarId();
	
		if(id_trabajador!= 0)	// Dejamos reservado el id=0 para poder chekear si hay un ID valido
		{

			ingresarSurco();
			
			if(surco != 0)		// Dejamos reservado el surco=0 para poder chekear si hay un ID valido
			{
				if(WiFi.isConnected())
				{
					// Si esta conectado a internet se envian los datos
					transmitirDatos();
				}
				else
				{
					// Si no hay internet, mientras en la memoria haya espacio se guardan los datos
					if(cant_datos_guardados <= DATOS_MAXIMOS_EEPROM)
					{
						grabarMediciones();
					}
				}

				// Sin importar si hay o no conexion WiFi, se imprime el comprobante (avisando si hay o no conexion en el momento de impresion)
				imprimir();

				// Impresion de mensaje de exito.
				lcd.setCursor(0,2);
				lcd.print("Listo...            ");
			}	
		}
	}

	// Si se reconecta a WiFi envía todos los datos guardados que tenía
	if(cant_datos_guardados > 0 && WiFi.isConnected())
	{
		transmitirDatosGuardados();
	}

}

// ======= Para WiFi =======

/*
 * Se inicializa el objeto WiFiManager en modo no bloqueante.
 * Se intenta conectar de forma automatica con las credenciales que encuentra guardadas.
 * Si no, inicia en modo AP con una red de nombre y contraseña establecida por referencia.
*/
void iniciarWiFi()
{
	wm.setDebugOutput(__DEBUGG);	/* Para debugguear */

	WiFi.mode(WIFI_STA); /* Modo de funcionamiento seteado explicitamente. Por defecto STA+AP */     

	// if(__DEBUGG)
		// wm.resetSettings();	/* Se resetean las credenciales, solo para debugg, luego las recuerda (EEPROM) */

	wm.setConfigPortalBlocking(false);	/* Trabaja en modo no bloqueante */

	/* Esta funcion si no se conecta automaticamente, crea un AP */
	if(wm.autoConnect (SSID, PASS) && __DEBUGG){
		Serial.println("Conectada al wifi...");
	}
	else{
		if(__DEBUGG)
			Serial.println("Se inicio el portal de configuracion en modo AP");
		
		/* Se  configura la pagina del AP */
		wm.setTitle("RSA - IOT");
		wm.setDarkMode(true);
	}
}

/**
 * Funcion que revisa si hay informacion de una red guardada y se intenta reconectar. 
 * FUNCION NO UTILIZADA DESDE V0.0.3
*/
void reconnectWifi()
{	
	if(wm.getWiFiIsSaved())
		wm.autoConnect();	/* Se intenta conectar nuevamente al WiFi guardado si no hay internet */

	if(__DEBUGG)
		Serial.println("intentando reconectarse a: " + wm.getWiFiSSID(true));	
}

/**
 * Funcion que desconecta del wifi y elimina las credenciales si se presiono
 * el boton de desconexion.
*/
void checkDisconnect()
{
	/* Por unica vez se hace la desconexion del wifi y del broker y se inicia el modo AP */
	if(desconectarse)
	{
		// clientMQTT.disconnect();	
		WiFi.disconnect();
		wm.disconnect();
		/* Elimina las credenciales para que no se conecte a la misma red. 
		   Esto se hace SOLO en el caso que la desconexion sea voluntaria. */
		wm.resetSettings();	
		iniciarWiFi();
		desconectarse = false;
	}
}

// ========= Para memoria EEPROM =========
/**
 * Se inicializan los 4kB de la memoria y se leen los bytes relacionados a la 
 * cantidad de datos guardados.
*/
void iniciarEEPROM()
{
	EEPROM.begin(4096);	// Cantidad total de bytes a usar de la memoria EEPROM (valor maximo es de 4096 Bytes)
	
	uint8_t cant_datos_guardadosH = EEPROM.read(101);
	
	/**
	 * Si la memoria alta esta en 255, quiere decir que es la primera vez que se enciende el dispositivo, 
	 * por eso se llena con 0 y asi la segunda vez que se encienda el dispositivo este if() no se ejecutara.
	 * El if() solo se ejecuta la primera vez que se utiliza el dispositivo ya que luego de haberse ejecutado,
	 * siempre habra un numero distinto a 255 en esa parte de la memoria.
	*/
	if(cant_datos_guardadosH == 255)
	{
		cant_datos_guardados = 0;
		EEPROM.write(101, (uint8_t)cant_datos_guardados);
		EEPROM.write(102, (uint8_t)cant_datos_guardados);
		EEPROM.commit();
	}

	posicion = 103 + (cant_datos_guardados*TAMANO_DATO);	// Se apunta al byte siguiente al del ultimo dato guardado
}

/**
 * Mientras haya lugar en la memoria EEPROM realiza el guardado de datos si no 
 * hay internet e incrementa las variables de puntero.
*/
void grabarMediciones() 
{  
	if(cant_datos_guardados == DATOS_MAXIMOS_EEPROM)
		return; 	// No se pueden agregar mas datos. Se hace esto para evitar overflow de 254 a 0 y el 255 esta reservado
	else
	{
		// Primero se almacena la id del recolector 
		uint8_t id_trabajadorH = uint8_t(id_trabajador/256);	
		uint8_t id_trabajadorL = uint8_t(id_trabajador);  		
		EEPROM.write(posicion, id_trabajadorH);	// Primero se guarda el High byte y luego el Low byte.
		posicion ++;
		EEPROM.write(posicion, id_trabajadorL);
		posicion ++;

		// Segundo se almacena el surco
		EEPROM.write(posicion, surco);
		posicion ++;

		// Finalmente se castea y almacena el peso
		uint8_t pesoH = (uint8_t)(peso/256);
		uint8_t pesoL = (uint8_t)(peso);
		EEPROM.write(posicion, pesoH);	// Primero se guarda el High byte y luego el Low byte.
		posicion ++;
		EEPROM.write(posicion, pesoL);
		posicion ++;

		// Respaldamos la cantidad de datos guardados en caso de corte de suministro de energia
		cant_datos_guardados ++;
		uint8_t cant_datos_guardadosH = uint8_t(cant_datos_guardados/256);
		uint8_t cant_datos_guardadosL = uint8_t(cant_datos_guardados);
		EEPROM.write(101, cant_datos_guardadosH);	// Primero se guarda el High byte y luego el Low byte.
		EEPROM.write(102, cant_datos_guardadosL);

		posicion = 103 + (cant_datos_guardados*TAMANO_DATO);	// Se apunta al siguiente byte libre para un proximo grabado de mediciones

		EEPROM.commit();

		return;
	}	
}

/**
 * Esta funcion devuelve una cadena que es la que hay que enviar a la BD
 * una vez que se restituye la conexion a internet.
 * @return: String "id_trabajador=id&surco=surco&peso=peso" en formato para hacer un HTTP request.
*/
String leerMediciones()
{
	if(cant_datos_guardados == 0)
		return "";	// No hay datos para leer.
	else
	{
		String dato_enviar = "id_trabajador=";

		int i = posicion-TAMANO_DATO;

		// Reconstituyo el valor de id_trabajador almacenado.
		uint8_t idH= EEPROM.read(i);
		i++;
		uint8_t idL= EEPROM.read(i);
		uint16_t id_trabajador_a_enviar=(idH * 256) + idL;
		i++;
		
		// Agrego las partes al mensaje y agrego tambien el surco.
		dato_enviar += String(id_trabajador_a_enviar) + "&surco=" + String(EEPROM.read(i));
		i++;
		
		// Reconstituyo el valor del peso almacenado.
		uint8_t pesoH= EEPROM.read(i);
		i++;
		uint8_t pesoL= EEPROM.read(i);
		uint16_t peso_enviar=(pesoH * 256) + pesoL;
		
		// Termino de construir el mensaje.
		dato_enviar += "&peso=" + String(peso_enviar);
	
		return dato_enviar;
	}
}

// ======= Para display LCD =======
void iniciarLCD()
{
	lcd.init();			// Inicializa el display
	lcd.backlight();    // Enciende la luz  
	lcd.clear();		// Limpia la pantalla 
}

// ======= Para configurar pines y sensores =======

/**
 * Se inicializan los buses y pines necesarios para interrupciones.
*/
void configPines()
{
	// inicializa bus I2C y teclado
  	Wire.begin(SDA, SCL);	
	teclado.begin(TECLADO_ADDRESS);

	// inicializa bus SPI y modulo RFID
	SPI.begin();		
	mfrc522.PCD_Init();

	pinMode(WIFI_DISCONNECT_GPIO, INPUT_PULLUP);	/* Pin para interrupcion (ver conectarseAWifi()) */	
	/* Se habilita interrupcion para desconectar el dispositivo */
	attachInterrupt(digitalPinToInterrupt(WIFI_DISCONNECT_GPIO), desconectarWifi, CHANGE); /* Con high anda, con low no*/
}

/**
 * Funcion por pulling. De acuerdo a la tecla presionada se levantan banderas
 * que son luego revisadas en el programa principal. Hay teclas que hacen que 
 * se le asignen valores a variables especiales.
*/
void leerTeclado()
{
	static String ultima_tecla,tecla_cargada;

	String tecla_presionada = String(TECLAS[teclado.getKey()]);
	
	// Si no se esta apretando una tecla o la tecla presionada es la misma que en la pasada anterior, se desestima la lectura.
	if (tecla_presionada == ultima_tecla || tecla_presionada == "N" || tecla_presionada == "F")
	{
		tecla_cargada = "";
	}
	else
	{
		tecla_cargada = tecla_presionada;
	}

	if(tecla_cargada == "C")
	{
		tara_presionado = true;
	}

	// Si se estan leyendo numeros, se asigna la lectura a 'id'.
	if(bandera_numeros && tecla_cargada !="A")
	{

		id += tecla_cargada;
	}
	
	if(tecla_cargada == "A")
	{
		aceptar_presionado = true;	
	}

	if(tecla_cargada == "B")
	{
		borrar_presionado = true;
		id = "";
	}
	
	ultima_tecla = tecla_presionada;
}

/**
 * Interrupcion que levanta bandera para desconectar de WiFi.
*/
IRAM_ATTR void desconectarWifi()
{
	if(WiFi.isConnected())
	{	
		if(__DEBUGG)
		{
			Serial.println("Desconectando WiFi...");
		}
		desconectarse = true;
	}
	else{
		if(__DEBUGG)
		{
			Serial.println("Ya esta desconectado del WiFi...");
		}
	}

}

/*
	Modificar este segmento para incluir los UID de las nuevas tarjetas.
	El valor izquierdo es el UID de tarjeta (en valor decimal, no en hexadecimal) y el
	valor de la derecha la referencia a numero de trabajador. Ej: usuarios("761443243", "1");
	Observar en este caso, el numero decimal '761443243' corresponde al UID hexadecimal '4C90202B'.
*/
void crearID()   
{
  usuarios("761443243", "1");	// Usuario de ejemplo...
}

// ======= Para bascula =======

/**
 * Se inicia el sensor HX711 para lo cual se requiere que se posicione sobre la 
 * balanza una masa de calibracion.
 * Se encuentran dos puntos para encontrar una curva de calibracion.
 * ES UNA FUNCION BLOQUEANTE!!
*/
void iniciarBascula()
{
	// Procedimiento de tara
	for (int ii=0;ii<int(promedio);ii++)
	{
		yield();	// Necesario para que el ESP8266 no se resetee por WDT
		delay(10);
		x0+=hx711.read();
	}
	x0/=long(promedio);

	if(__DEBUGG)
		Serial.println("Colocar masa de calibracion");
	
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("    Colocar masa    ");
	lcd.setCursor(0,1);
	lcd.print("   de calibracion   ");

	// Proceso de calibración (la masa agregada debería ser igual a masa_calibracion)
	while(true)
	{
		if (hx711.read()<x0+10000)
		{
			yield();	// Necesario para que el ESP8266 no se resetee por WDT
			//Mientras no se detecte la masa de calibración, no se hace nada
		}
		else 
		{
			delay(2000);
			for (int jj=0;jj<int(promedio);jj++)
			{
				yield();	// Necesario para que el ESP8266 no se resetee por WDT
				x1+=hx711.read();
			}
			x1/=long(promedio);

			break;
		}
	}

	if(__DEBUGG)
		Serial.println("Calibración completada");

	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("    Calibracion     ");
	lcd.setCursor(0,1);
	lcd.print("     Completada     ");
	delay(1500);	/* Para poder terminar de visualizar los mensajes */
}

/**
 * Se toman una serie de muestras (segun variable promedio) y se las promedia
 * para obtener el valor de la masa sobre la bascula. Si se presiono el boton 
 * de TARA se hace la operacion de tara.
*/
void pesar()
{
	// Se toma un promedio del pesaje
	long lectura = 0;
	for (int jj=0;jj<int(promedio);jj++)
	{
		yield();	// Necesario para que el ESP8266 no se resetee por WDT
		lectura+=hx711.read();
	}
	lectura/=long(promedio);

	// Se calcula la masa en base a proporciones lineales definidas en la calibración de la bascula
	float proporcion_1 = (float) (lectura-x0);
	float proporcion_2 = (float) (x1-x0);
	float proporcion = proporcion_1/proporcion_2;

	peso = int(masa_calibracion*proporcion);

	// Si se presiono el boton de tara, se asigna la medicion de peso a la variable tara
	if(tara_presionado)
	{
		tara = peso;
		tara_presionado = false;
		if(__DEBUGG)
			Serial.println("TARA");
	}

	peso=peso-tara; 
}

// ======= Para la impresora =======
void iniciarImpresora()
{
	impresora.begin();	/* Inicializo la impresora */
	impresora.flush();	/* Vacio el buffer de entrada por si tiene basura*/	
	impresora.online();	/* Pongo la impresora en estado Activo*/
	impresora.setDefault();	/* Reseteo por si habia quedado alguna configuracion indeseada */
}

/* Se imprime el mensaje del comprobante */
void imprimir()
{
	impresora.online();	/* Activamos la impresora */

	impresora.justify('C');	/* Centrado del texto */
	impresora.doubleHeightOn();	/* Texto mediano */
	impresora.println("REGISTRO DE COSECHA");
	impresora.doubleHeightOff();

	impresora.justify('L');	/* Justificacion a la izq. del texto*/
	impresora.println();	/* Se utiliza para espaciar el mensaje */
	impresora.println("No control:" + String(id_trabajador));
	impresora.println("Surco: " + String(surco));
	impresora.println();
	impresora.println("******************");

	impresora.justify('C');	
	impresora.doubleHeightOn();	/* Texto mediano */
	impresora.println("PRIMERA");
	impresora.doubleHeightOff();
	impresora.justify('L');	/* Justificacion a la izq. del texto*/
	impresora.println();

	impresora.println("Peso en grs: " + String(peso));
	(WiFi.isConnected()) ?  impresora.println("Conectado a internet?: SI") : impresora.println("Conectado a internet?: NO");
	impresora.println();

	impresora.justify('C');	
	impresora.setSize('L');
	impresora.println("ACUMULADO");
	impresora.justify('L');	/* Justificacion a la izq. del texto*/
	impresora.setSize('M');

	impresora.println("Total grs: " + String(peso));
	impresora.println();
	impresora.println();
	impresora.println();
	impresora.println();
	
	impresora.offline();	/* Desactivamos la impresora */
}

// ======= Para hacer la transmision de datos =======
void ingresarId()
{
	// Activamos la deteccion de numeros por teclado
	bandera_numeros=true;

	// Cartel para avisar que se esta ingresando el id_trabajador
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Acerque su tarjeta  ");
	lcd.setCursor(0,1);
	lcd.print("O ingrese su id:    ");
	lcd.setCursor(0,2);
	lcd.print("B=borrar A=siguiente");

	id = "";	// Reseteo el valor por si tenia basura.
	while(id.length() < 5 && aceptar_presionado == false)
	{
		yield();	// Para que no crashee el nodemcu

		leerTeclado();

		// Si se acerca una tarjeta:
		if(mfrc522.PICC_IsNewCardPresent())
		{	
			lectura_RFID = "";
			
			if(mfrc522.PICC_ReadCardSerial())
			{
				for (byte i = 0; i < mfrc522.uid.size; i++)
					lectura_RFID += mfrc522.uid.uidByte[i];	// Se lee el UID de la tarjeta para compararlo con 
				
				mfrc522.PICC_HaltA();  		// detiene comunicacion con tarjeta 
			}
			if(usuarios[lectura_RFID]=="")
			{	
				lcd.setCursor(0,3);
				lcd.print("TARJETA INVALIDA    ");
				delay(1500);
				lcd.setCursor(0,3);
				lcd.print("                    ");
			}
			else
			{
				id = usuarios[lectura_RFID];
				break;	// No hace falta seguir en el bucle. Tampoco haria falta este break si se asigna un id de 5 caracteres
			}
			 
		}

		// Se muestra el valor que se está ingresando
		lcd.setCursor(0,3);
		lcd.print("id: " + id + "             ");
	}

	// Bajo bandera para no modificar el 'id' ya ingresado.
	bandera_numeros=false;
	
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Peso: " + String(peso) + "         ");
	lcd.setCursor(0,1);
	lcd.print("id: " + id + "           " );
	lcd.setCursor(0,2);
	lcd.print("A = siguiente       ");
	lcd.setCursor(0,3);
	lcd.print("B = cancelar        ");

	// Bajo banderas por si tenian valores basura.
	aceptar_presionado = false;
	borrar_presionado = false;

	while(true)
	{
		yield();	// Para evitar problemas con WDT

		leerTeclado();

		if(aceptar_presionado)
		{
			// Se convierte el numero ingresado a un entero
			id_trabajador = (uint16_t)id.toInt();

			lcd.setCursor(0,2);
			lcd.print("Procesando...       ");
			lcd.setCursor(0,3);
			lcd.print("                    ");	// Se limpian las otras lineas

			break;	// Se sale del while
		}
		if(borrar_presionado)
		{
			// Se escribe un 0 para que en el loop se detecte que no hay que transmitir
			id_trabajador = 0;

			lcd.setCursor(0,2);
			lcd.print("Cancelando...       ");
			lcd.setCursor(0,3);
			lcd.print("                    ");	// Se limpian las otras lineas

			break;	// Se sale del while
		}
	}

	// Bajo banderas 
	aceptar_presionado = false;
	borrar_presionado = false;
}

void ingresarSurco()
{
	// Activamos la deteccion de numeros por teclado.
	bandera_numeros=true;

	// Cartel para avisar que se esta ingresando el surco
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Ingrese surco       ");
	lcd.setCursor(0,1);
	lcd.print("B=Borrar A=Siguiente");
	lcd.setCursor(0,3);
	lcd.print("                    ");	// Se limpia esta linea

	// Reseteo valores por si tenian valores basura.
	surco = 0;	
	id="";
    while(id.length() < 3 && aceptar_presionado == false)
	{
		yield();	// Para que no crashee el nodemcu

		leerTeclado();	// Se leen los valores ingresados.

		// Se muestra el valor que se está ingresando
		lcd.setCursor(0,2);
		lcd.print("Surco: " + id + "          ");
	}

	// Dejo de leer numeros para no modificar el valor ingresado.
    bandera_numeros=false;

    lcd.clear();
	lcd.setCursor(0,0);
  	lcd.print("Peso: " + String(peso) + "         ");
  	lcd.setCursor(0,1);
  	lcd.print("id: " + String(id_trabajador) + " surco: " + String((uint8_t)id.toInt()));
  	lcd.setCursor(0,2);
  	lcd.print("A = ENVIAR          ");
  	lcd.setCursor(0,3);
  	lcd.print("B = Cancelar        ");

  	// Bajo banderas por si tenian valores basura.
	aceptar_presionado = false;
  	borrar_presionado = false;
    while(true)
	{
		yield();	// Para evitar problemas con WDT

		leerTeclado();

		if(aceptar_presionado)
		{
			surco = (uint8_t)id.toInt();	// Se convierte el numero ingresado a un entero.

			lcd.setCursor(0,2);
			lcd.print("Procesando...       ");
			lcd.setCursor(0,3);
			lcd.print("                    ");	// Se limpian las otras lineas

			break;	// Se sale del while
		}
		if(borrar_presionado)
		{
			// Se escribe un 0 para que en el loop se detecte que no hay que transmitir
			surco = 0;

			lcd.setCursor(0,2);
			lcd.print("Cancelando...       ");
			lcd.setCursor(0,3);
			lcd.print("                    ");	// Se limpian las otras lineas

			break;	// Se sale del while
		}
	}

	// Bajo banderas 
	aceptar_presionado = false;
  	borrar_presionado = false;

}

/**
 * Se transmiten los datos, se hace una serie de intentos y si no se puede
 * hace la transmision se guarda el dato en memoria.  
*/
void transmitirDatos()
{
	// Impresion de mensaje por pantalla.
	lcd.setCursor(0,2);
	lcd.print("Transmitiendo...    ");

	// Se empieza a construir el url a enviar, con 3 decimales de precisión.
	String postData ="id_trabajador="+ String(id_trabajador)+ "&peso=" + String(peso)+ "&surco=" + String(surco);

	int httpCode = 0;   
	int intentos = 10;

	while(httpCode != HTTP_CODE_OK && intentos > 0)	// Intento enviar el valor 10 veces más por las dudas.
	{
		// Se crea un objeto http de la clase HTTPClient
		WiFiClient client;
		HTTPClient http;  

		// Se pueden postear valores al archivo PHP como: example.com/dbwrite.php?name1=val1&name2=val2&name3=val3
		// Para mayor informacion visitar:- https://www.tutorialspoint.com/php/php_get_post.htm

		// Cambiar por el url del host propio con la ubicacion del PHP que recibira los datos  
		
		// http.begin(client, "http://agroteech.com/my_php/dbwrite.php");	// Se conecta al host donde esta la base de datos MySQL
		http.begin(client, BD_URL);
		http.addHeader("Content-Type", "application/x-www-form-urlencoded");	// Se especifica el content-type del header

		// Se hace el request y se cierra la conexion
		httpCode = http.POST(postData); 
		
		// http.getString().substring(40);	/* Para obtener la fecha del PHP de dbwrite_bascula.php */

		http.end(); 

		if(httpCode == HTTP_CODE_OK)
			break;	// Si se pudo enviar, salgo del ciclo

		intentos --;	// Si no se pudo enviar, disminuyo el contador para no quedar en bucle infinito

		delay(20);	// Delay para esperar hasta el próximo intento 

	}

	if(intentos == 0)
		grabarMediciones();	// Si no pude enviar el dato, lo guardo

}

void transmitirDatosGuardados()
{
	String postData = "";
	int httpCode;
	int intentos = 0;

	// Mensaje informativo que se imprime para que no se opriman botones
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Espere un momento   ");
	lcd.setCursor(0,1);
	lcd.print("Transmitiendo datos ");
	lcd.setCursor(0,2);
	lcd.print("NO PULSE BOTONES !! ");
	
	// Mientras haya datos guardados que los envie a la BD
	while(cant_datos_guardados > 0)
	{
		// Se crea un nuevo cliente HTTP cada vez que se quiere enviar un dato
		WiFiClient client;
		HTTPClient http;	

		// Conexión a la base de datos, cambiar con el nombre del servidor y ubicación del PHP
		// http.begin(client, "http://agroteech.com/my_php/dbwrite.php");
		http.begin(client, BD_URL);
		http.addHeader("Content-Type", "application/x-www-form-urlencoded");	

		// Mostramos cuantos datos faltan de enviar
		lcd.setCursor(0,3);
		lcd.print("Datos a enviar:  " + String(cant_datos_guardados));

		postData = leerMediciones();
		
		httpCode = http.POST(postData);   

		if (httpCode == HTTP_CODE_OK) 
		{ 
			// Si se pudo enviar el dato, se actualiza la cantidad de datos guardados
			cant_datos_guardados --;
			posicion = 103 + (cant_datos_guardados*TAMANO_DATO);	// Se actualiza el puntero
			intentos = 0;	// Se resetea contador de intentos
		}
		else
			intentos ++;
		
		if(intentos >= 10)	// Si no se pudo enviar durante 10 veces seguidas, no quedar en un bucle infinito
		{
			http.end();
			break;
		}

		http.end();	// Se desconecta de forma limpia luego de manejar el mensaje.

		delay(10);	// Delay para esperar para la creacion de un nuevo cliente
	}

	// Actualizamos los valores en eeprom
	uint8_t cant_datos_guardadosH = (uint8_t)(cant_datos_guardados/256);
	EEPROM.write(101, cant_datos_guardadosH);
	uint8_t cant_datos_guardadosL = (uint8_t)(cant_datos_guardados);
	EEPROM.write(102, cant_datos_guardadosL);
	EEPROM.commit();
}
