
#include <SimpleDHT.h>
#include <FileIO.h>
#include <Process.h>
#include <LiquidCrystal.h>

#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>

// Listen to the default port 5555, the Yún webserver
// will forward there all the HTTP requests you send
BridgeServer server;


/*
//define xively connect info
#define APIKEY        "74300b6790fe4b73b88be33eebee2f50"   // replace your xively api key here 
#define FEEDID        "manu-arduino"; //"feea422fecf64b75bd2c39b0bc5f5d8f"                   // replace your xively feed ID

Subdomain: manu-arduino
 * Password: Arduino1Manu
 * â€¢Xively Device ID
Iddevice = 113dec73-5ecd-4ada-ac4e-56303f83badd
â€¢Xively Account ID
idCuenta = 74300b67-90fe-4b73-b88b-e33eebee2f50

Password = e75IOJa5VXcmVEZLG9LAFnRnmJMcNoakoEhWe58SdXs=
username = 113dec73-5ecd-4ada-ac4e-56303f83badd
*/

#define RUN_INTERVAL_MILLIS 2000 // Cada 500 milisegundos se leera la temperatura y humedad del sensor del sensor
#define lecturasMaximas 3// 10 lecturas para que el LCD comienze a parpadear

#define rangeMAX  100 //Rango de lecturas extremas
#define rangeMIN  0

//DEFINIMOS DEBUG
#define DEBUG 1// if we dont want to appears debug messages, just comment this line


//Para determinar los valores maximos y minimos
float highTem = 11.00;
float highHum = 20.00;
float lowTem  = 10.00;
float lowHum  = 10.00;

//Contadores de lecturas maximas y minimas
byte nTemMax = 0;
byte nTemMin = 0;
byte nHumMax = 0;
byte nHumMin = 0;

//Banderas para normalizar cada valor
bool normTemHigh = false;
bool normTemLow = false;
bool normHumHigh = false;
bool normHumLow = false;

//Boolean used in method: escribirSql()
bool firstEntry = true;

//Pin donde colocamos nuestro sensor DTH11
int pinDHT11 = 7;
SimpleDHT11 dht11;

//Pines donde colocamos LCD LiquidCrystal
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

Process proc;
String cmd;
float tempAux = 0  ,humAux = 0;
bool entra = false; 

//Long utilizado para determinar cada cuanto tiempo se quiere escribir
unsigned long lastRun = (unsigned long) - 6000;


void setup()
{
  Bridge.begin();
  lcd.begin(16, 2); // start the library

  // put your setup code here, to run once:
  Bridge.begin();
  Serial.begin(9600);
  FileSystem.begin();

  server.listenOnLocalhost();
  server.begin();

  //Inicializamos Serial
  while (!Serial) { 
      msgShield("Conecta el ", 3, 0 , 0, 0);
      msgShield("Monitor Serie", 1, 1 ,1 , 0);
    }

  //Inicializamos Base de datos sqlite3
  crearSql();

}


void loop()
{
   YunServe();
    unsigned long now = millis();
    if (now - lastRun >= RUN_INTERVAL_MILLIS) {
      lastRun = now;
      tomarTempHum();
    }
}


/*controlValues : Controla que los Valores maximo y minimos impuestos sean coherentes
                  No puede haber un valor mÃ¡ximo menor que un valor minimo*/
bool controlValues(){
 
  if( highTem > lowTem && highHum > lowHum){
      return true;
  }else{
    
      debug("LOS Valores maximos y minimos NO SON COHERENTES\n", (String)__FUNCTION__ , (String) __LINE__ );

      msgSerial("Valores de maximos y minimos no coherentes - ", 0);
      msgSerial("Los valores de los maximos deben de ser mayores a los valores minimos", 1);
      msgSerial("REINICIA EJECUCION", 1);   
        
      msgShield("Valores maximos", 0 , 0 , 0 , 1);
      msgShield("no coherentes", 2, 1, 1 , 0);
      
    return false;
   }
  }


/* tomarTempHum : Recoge la temperatura y humedad del componente DTH11*/
void tomarTempHum()
{

  byte err;
  byte temp = NULL  , hum = NULL;
   
   if ((err = dht11.read(pinDHT11, &temp, &hum, NULL)) != SimpleDHTErrSuccess) {
     
      msgShield(" ERROR  SENSOR", 0 , 0 , 0 , 0 );
      msgShield(" NO CONECTADO", 1, 1, 1, 0);
      
      debug(", LECTURAS DE SENSOR ERRONEAS, error: " + (String)err, (String)__FUNCTION__ , (String) __LINE__ );

      
    }else{ 
      //Comprobacion de lecturas erroneas
      if (  rangeMAX > temp > rangeMIN  &&  rangeMAX > hum > rangeMIN) {
        
        escribirSql((float)temp, (float)hum);

        tempAux = (float)temp;
        humAux = (float)hum;

      //  Xively();
        
      } else {
           debug("LECTURAS DE SENSOR EXTREMAS\n", (String)__FUNCTION__ , (String) __LINE__ );
              
           msgShield("LECTURAS SENSOR", 0 , 0 , 0 , 1);
           msgShield("EXTREMAS", 4 , 1, 1 , 0 ); 
      }
    }


}


/* crearFichero: Creamos directorio dentro de arduino. 
                  Nos ayudamos del Process para realizar MKDIR*/
void crearFichero()
{
  //Process proc;
  
  msgSerial("Creando directorio : /tmp/DTH11/sensor.db", 1);
  
  proc.begin("mkdir");
  proc.addParameter("/tmp/DTH11");
  proc.run();

}


/*crearSql :  Creamos la tabla sqlite3 SENSOR. 
              Si estaba creada en el directorio, hacemos DROP TABLE y la volvemos a crear */
void crearSql()
{
 if(controlValues()){
   //Process proc;
   
   crearFichero();
  
    msgSerial("Creando sqlite con ruta: /tmp/DTH11/sensor.db", 1);
  
    cmd = "sqlite3 ";
    cmd += "/tmp/DTH11/sensor.db ";
    cmd += "'DROP TABLE sensor;'";
  
    proc.runShellCommand(cmd );//hacemos DROP sobre sql
  
    //Comprobacion de errores
    if(comprobarError(proc) == false){
        msgSerial("ERROR DROP", 1);
        debug("ERROR DROP\n", (String)__FUNCTION__ , (String) __LINE__ );
      }
  
    delay(2000); //Le damos tiempo a que realize correctamente la ejecution de Process p
  
    cmd = "sqlite3 ";
    cmd += "/tmp/DTH11/sensor.db ";
    cmd += "'CREATE TABLE sensor(ID INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, DATE DATETIME NOT NULL, TEMP REAL NOT NULL, HUM REAL NOT NULL);'";
  
    proc.runShellCommand(cmd ); //Creamos tabla
  
    if(comprobarError(proc) == false){
         msgSerial("ERROR CREATE", 1);
        debug("ERROR CREATE\n", (String)__FUNCTION__ , (String) __LINE__ );
      }else  msgSerial("SQLite creada correctamente", 1);
  
    Serial.flush();
 }
}

/*escribirSql : Almacena en la base de datos sqlite3 una nueva tupla
PARAMETROS:   temp: Temperatura actual 
              hum: Humedad actual*/
void escribirSql(float temp, float hum)
{
  //Process proc;
  
  //Para que solo se muestre la primera vez que entra
  if (firstEntry == true) {
    msgSerial("\nIntroduciendo datos en la tabla sensor situada en: /tmp/DTH11/sensor.db", 1);
    firstEntry = false;
  }

  cmd = "sqlite3 ";
  cmd += "/tmp/DTH11/sensor.db ";
  cmd += "'INSERT INTO sensor (DATE , TEMP , HUM) VALUES (CURRENT_TIMESTAMP," + String(temp) +  " ," + String(hum) + ");'";
  cmd += "; echo $?";

 proc.runShellCommand(cmd);//Hacemos insert a la base de datos

    #ifdef DEBUG
      getDataDebug(temp, hum);
    #else
      getData(); //Muestra la ultima temperatura
    #endif

  Serial.flush();

}

/*getDataDebug : MÃ©todo alternativo al getData. Creado para cuando este activo la definicion de DEBUG
  PARAMETROS   : Directamente recogidos del sensor
                float debugTemp
                flaot debugHum*/
void getDataDebug(float debugTemp, float debugHum){
  
String b = "  HUM = " + String(debugHum) ;
String a = "TEMP = " + String(debugTemp) ;
  
   imprimirValuesLCD(a, b);
  
//   minMaxValues( debugTemp  ,  a  , debugHum  , b );
//   normalizarValues(debugTemp  ,  a , debugHum  , b );

//   comprobarTemperatura(debugTemp , debugHum);


  msgSerial("", 1);
  Serial.flush();

  }

/*getData: Consultamos la Temperatura y la Humedad de la ultima tupla de la base de datos*/
void getData() {

  char valores;
  int inChar;
  
  //Process proc;
  cmd = "sqlite3 ";
  cmd += "-line ";
  cmd += "/tmp/DTH11/sensor.db ";
  cmd += "'SELECT TEMP FROM sensor ORDER BY ID DESC LIMIT 1;';"; //Query para llamar la data desde SQLite

  proc.runShellCommand(cmd);

  //Recogemos la TEMPERATURA mas reciente de la base de datos sqlite3
  String temp, tempString;
  while (proc.available() > 0) {
    valores = proc.read();

    if (proc.available() != 0) //Eliminamos el ultimo espacio del char, para que se muestre el mensaje correctamente el display
      temp.concat(valores);

     inChar = valores;        //Calculamos el String a int, para poder compararlos con las temperaturas maximas y minimas
     if (isDigit(inChar)) 
       tempString += (char)inChar;
    
  }
  //Recogemos la HUMEDAD mas reciente de la base de datos sqlite3

  cmd = "sqlite3 ";
  cmd += "-line ";
  cmd += "/tmp/DTH11/sensor.db ";
  cmd += "'SELECT HUM FROM sensor ORDER BY ID DESC LIMIT 1;';"; //Query para llamar la data desde SQLite
  proc.runShellCommand(cmd);

  String hum, humString;
  while (proc.available() > 0) {
    valores = proc.read();

    if (proc.available() != 0) //Eliminamos el ultimo espacio del char, para que se muestre el mensaje correctamente el display
      hum.concat(valores);

    inChar = valores; //Calculamos el String a int, para poder compararlos con las humedades maximas y minimas
    if (isDigit(inChar)) 
      humString += (char)inChar;
    
  }


//  comprobarTemperatura(tempString.toFloat()  / 10 , humString.toFloat() / 10);

  msgSerial("", 1);
  Serial.flush();

}

/*imprimirValuesLCD : Para mostrar la informaciÃ³n por del LCD
 PARAMETROS:        String temp : Mensaje a mostrar con la informacion del temperatura
                    String temp : Mensaje a mostrar con la informacion del temperatura*/
void imprimirValuesLCD( String temp, String hum){
  
  #ifdef DEBUG
     temp = " "+temp;
  #endif
  
    msgShield(temp + (char)223 + "C", 0, 0, 0 , 0 ); 
    msgSerial(temp + " ºC", 1);

    msgShield(hum + "%", 0 , 1, 0 , 0 );
    msgSerial(hum + "%" , 1);

      
  //Parpadeo
  delay(3000);

//  //***********si no sobra espacio, borrar este primer if y quitar el else
//  if ((normTemHigh == true || normTemLow == true) && (normHumHigh == true || normHumLow == true)){
//
//     msgShield("       ", 8 , 0 , 0 , 0);
//     msgShield("      ", 8 , 1, 2 , 0);
//     
//  }else 
  if(normHumHigh == true || normHumLow == true){
    
     msgShield("      ", 8 , 1, 2 , 0);
     
  }
   if(normTemHigh == true || normTemLow == true){

     msgShield("       ", 8 , 0, 2 , 0);
   }
  
}


/*METODO Debug : PARAMETROS=> str: mensaje de debug, Function: funcion de debug, Line: linea de debug */
void debug(String str, String Function, String Line){
  
  #ifdef DEBUG
  Serial.print(millis()); \
   Serial.print(", Method: "); \
   Serial.print(Function); \
   Serial.print(", Line: "); \ 
   Serial.print(Line); \
   Serial.print(", Debug: "); \
   Serial.println(str);
#endif
  
  }


bool comprobarError(Process pi)
{
  char err;
  while (pi.available() > 0) {
     err = pi.read();
    if ( err == '1') {
      return false;
    }
  }
  return true;
}



void YunServe()
{
    // Get clients coming from server
    
 BridgeClient client = server.accept();
  // There is a new client?
  if (client) {
    // Process request
    process(client);

    // Close connection and free resources.
    client.stop();
  }
  
}

int process(BridgeClient  client) {
  // read the command
  String command = client.readStringUntil('/');

//  // is "digital" command?
//  if (command == "version") {
//    
//    client.print(F(" -> Version Actual: "));
//    client.println("2.3.2");
//    Bridge.put("version", "2.3.2");
//    
//  }else
  if (command == "valores") {
    modeCommand(client);
  }
//  else{
//     client.println(F("error"));
//    }
}
void modeCommand(BridgeClient client) {
 float pin;
 String mode;
 
 mode = client.readStringUntil('/');
 
 if (mode.startsWith("temperatura")) {

   client.print(F(" -> Temperatura Actual: "));
   client.println(tempAux);
   client.print(F(" -> Temperatura Maxima: "));
   client.println(highTem);
   client.print(F(" -> Temperatura Minima: "));
   client.println(lowTem);

  // void controlarTemperatura();// POR AQUIIIIIIIIII
    
  return;
 }else  if (mode.startsWith("humedad")) {
    // Send feedback to client
    client.print(F(" -> Humedad Actual: "));
    client.println(humAux);
   client.print(F(" -> Humedad Maxima: "));
    client.println(highHum);
     client.print(F(" -> Humedad Minima: "));
    client.println(lowHum);

    return;
    
  }else{
  client.print(F("error: invalid mode "));
  client.print(mode);
  }
}


void Xively()
{
 // Process proc;
  cmd = "sh /root/mosquitoPub.sh Temperatura "+ String(tempAux);
  proc.runShellCommand(cmd);

  //delay(1000);
  
  cmd = "sh /root/mosquitoPub.sh Humedad "+ String(humAux);
  proc.runShellCommand(cmd);

   //delay(1000);
}


//op = println / print
void msgSerial(String msg, byte op)
{
  if(op == 1)
    Serial.println(msg);
  else
    Serial.print(msg);
  
}

//op == parpadea
void msgShield(String msg, int x, int y, int op, int clear)
{
  if(clear == 1)
    lcd.clear();

  if(x >= 0 && y >=0 )
    lcd.setCursor(x, y);
    
  lcd.print(msg);
  if(op == 1){
    delay(1000); lcd.clear(); delay(1000);
  }else if (op == 2){
    delay(250); 
  }else if (op == 3){
    delay(3000);
    lcd.clear();
  }
}

//
//void comprobarTemperatura(float temp, float hum){
//
//  if(normTemHigh == false && normTemLow == false){
//    comprobarMax(temp, highTem, nTemMax, 1 ); //contador max
//    comprobarMax( lowTem , temp, nTemMin, 3 );  //cont min
//  }else if(normTemHigh == true) comprobarMax(highTem, temp, nTemMax, 5 ); //normalizar
//   else comprobarMax( temp ,lowTem , nTemMin, 7 ); //normalizar
//
//
//  //hacer otro para humedad
//   if(normHumHigh == false && normHumLow == false){
//    comprobarMax(hum, highHum, nHumMax, 2 ); //contador max
//    comprobarMax(lowHum ,hum , nHumMin, 4 );  //cont min
//  }else if(normTemHigh == true)  comprobarMax(highHum, hum, nHumMax, 6 );
//   else  comprobarMax(hum ,lowHum , nHumMin, 8 );
//  
//}
//
//
//void comprobarMax(float op1, float op2, byte &cont, byte op)
//{
// String msg, simbolo, email, mensaje;
//
// if (op1 > op2 ) {
//    cont++;
//
//    if (cont >= lecturasMaximas ) { //Controlamos que debe haber 10 lecturas minimos 
//      cont = 0;
//
//      mensajes(op, msg, simbolo, mensaje,  email);  
//        
//      msgSerial("---"+msg+"---", 1);
//      msgShield(msg , 0 , 0 , 0 , 1);
//     
//     if( 2 < op > 6  )  msgShield(op2 + simbolo, 2, 1, 3, 0); //maxima
//     else msgShield(op1 + simbolo, 2, 1, 3, 0); //normalizar
//     
//    }
//
//  } else {//Para asegurarnos que son 10 valores maximos seguidos
//    cont = 0;
//  }
//  
//}
//void mensajes(byte op, String &msg, String &simbolo, String &mensaje, String &email)
//{
// // Process proc;
//
// switch(op){ 
//  case 1: //Temperatura maxima
//    msg = "TEMP MUY ALTA";
//    simbolo = " *C";
//    normTemHigh = true;
//     cmd = "sh /overlay/etc/scripts/email1.sh " + String(tempAux) +" "+ String(highTem) + " mcasillars@alumnos.unex.es";
//  break;
//  
//  case 2: //Humedad maxima
//    msg = "HUM MUY ALTA";
//    simbolo = "%";
//   normHumHigh = true;
//   cmd = "sh /overlay/etc/scripts/email2.sh " + String(humAux) +" "+ String(highHum) + " mcasillars@alumnos.unex.es";
//  break;
//  
//  case 3: //Temperatuar minima
//    msg = "TEMP MUY BAJA";
//    simbolo = "*C";
//     normTemLow = true;
//      cmd = "sh /overlay/etc/scripts/email3.sh " + String(tempAux) +" "+ String(lowTem) + " mcasillars@alumnos.unex.es";
//  break;
//  
//  case 4: //Humedad minima
//    msg = "HUM MUY BAJA";
//    simbolo = "%";
//    normHumLow = true;
//     cmd = "sh /overlay/etc/scripts/email4.sh " + String(humAux) +" "+ String(lowTem) + " mcasillars@alumnos.unex.es";
//  break;
//
//  case 5: //Temperatura maxima
//    msg = "TEMP NORMALIZADA";
//    simbolo = "*C";
//    normTemHigh = false;
//     cmd = "sh /overlay/etc/scripts/email6.sh " + String(tempAux) +" "+ String(highTem) + " mcasillars@alumnos.unex.es";
//  break;
//  
//  case 6: //Humedad maxima
//    msg = "HUM NORMALIZADA";
//    simbolo = "%";
//   normHumHigh = false;
//    cmd = "sh /overlay/etc/scripts/email5.sh " + String(humAux) +" "+ String(highHum) + " mcasillars@alumnos.unex.es";
//  break;
//  
//  case 7: //Temperatuar minima
//    msg = "TEMP NORMALIZADA";
//    simbolo = "*C";
//     normTemLow = false;
//      cmd = "sh /overlay/etc/scripts/email6.sh " + String(tempAux) +" "+ String(lowTem) + " mcasillars@alumnos.unex.es";
//  break;
//  
//  case 8: //Humedad minima
//    msg = "HUM NORMALIZADA";
//    simbolo = "%";
//    normHumLow = false;
//     cmd = "sh /overlay/etc/scripts/email5.sh " + String(tempAux) +" "+ String(lowHum) + " mcasillars@alumnos.unex.es";
//  break;
//
//  default:
//  break;
// }

  //proc.runShellCommand(cmd);
  
//}

