#include <Adafruit_SleepyDog.h>
#include <MKRGSM.h>
#include <RTCZero.h>
#define RF_TRANSMITER 7         //Selecionando el PIN 7 para RF 
//Numero PIN de la SIM
const char PINNUMBER[]     = "";
// APN data
const char GPRS_APN[]      = "";
const char GPRS_LOGIN[]    = "";
const char GPRS_PASSWORD[] = "";
// Iniciar las instancias de librerias
GSMClient client;
GPRS gprs;
GSM gsmAccess;
GSMVoiceCall vcs;
GSM_SMS sms;
GSMPIN pin;
RTCZero rtc; 

#define MAX_NR 2000                       //El tope de numeros permitidos
#define TIMES  81                         //La señal de RF tiene 81 LOW's y 81 HIGH's
#define ADMIN  "652569476"                //El numero de administrador
//Variables
int w8, pingDay;                          //w8 = esperar, pingDay = el dia cuando se hará la peticion a la API
int pingHour = 17;                        //La hora cuando se hará la petición a la API. 
int pingMinute = 0;                       //Los minutos cuando se hará la petición a la API.
bool doLog;                               //Al recibir llamada doLog se cambiará al TRUE para mandar la informacion al servidor
char logBuf[50];                          //Al recibir llamada en logBuf se guardará la path para hacer petción
char server[] = "garage.losescoriales.es";   //La URL al servidor
char path[] = "/numbers?id=puerta_norte";                 //La path para hacer peticion y recibir todos numeros autorizados
int port = 10811;                            // port 10811 is the default for HTTP

char numtel[20];                          // Array para tener el numero entrante
long phoneList[MAX_NR] = {};              // Array de numeros authorizados
bool isResetMsg = false;                  //Variable para saber si hay que imprimir en Serial la mensaje o no;
bool isDoneReset = false;                 //Variable para saber si CPU se ha reseteado.
// Array de HIGH retraso para RF transmsor
int h_delay[TIMES] = 
{
  7300, 1250, 1250, 478, 455, 1255, 1255, 455, 1307, 1255, 480, 1255,
  480, 480, 480, 1280, 1334, 507, 507, 507, 507, 507, 1307, 507,    
  560, 1280, 1280, 480, 1280, 480, 1280, 480, 1334, 506, 1280, 506,
  506, 1280, 1280, 507, 534, 1280, 1280, 480, 507, 507, 1280, 1280,
  534, 1280, 1280, 1280, 1280, 507, 507, 1280, 560, 1280, 507, 1280,
  480, 1280, 1280, 480,  1334, 480, 507, 480, 480, 1280, 480, 507,                 
  1334, 1280, 480, 480, 480, 1280, 480, 480, 1307    
  };
// Array de LOW retraso para RF transmsor
int l_delay[TIMES] = 
{
  350, 350, 1112, 1155, 350, 350, 1150, 350, 350, 1120, 350, 1120, 
  1120,  1120, 295, 320, 1095, 1095, 1095, 1095, 1095, 295, 1094, 1094,
  294, 320, 1120, 320, 1094, 320, 1094, 320, 1094, 320, 1094, 1094, 
  320, 294, 1094, 1094, 320, 320, 1120, 1094, 1094, 320, 320, 1094,
  320, 320, 320, 320, 1094, 1094, 320, 1094, 294, 1120, 294, 1120,
  320, 320, 1120, 320, 1120, 1094, 1120, 1120, 320, 1120, 1094, 320,
  294, 1120, 1120, 1120, 320, 1120, 1120, 320, 2106     
  };



void setup() {
  pinMode(RF_TRANSMITER, OUTPUT);             //Iniciar el RF transmisor 
  Serial.begin(9600);                           // inicialar las comunicaciones en serie y espere a que se abra el puerto:
 
  /*while(!Serial) {
    ; //Espere a que se conecte el puerto serie. Necesario solo para puerto USB nativo.
      //COMENTAR CUANDO LAS PRUEBAS ESTÁN FINALIZADAS
  }*/
  
  Serial.println("Connecting to network...");
  while(!startGSM());                        //Configurar el GSM
  
  vcs.hangCall();                           //Esto asegura que el módem informa correctamente los eventos entrantes.

  
  Serial.println("Connected!");
  Watchdog.enable(20000);                   //Setear el perro guardian a 20 segundos.
  doLog = false;                            //No hay ninguna informacion para mandar. doLog = FALSE

  // Iniciar el RTC
  rtc.begin(); 
  rtc.setEpoch(gsmAccess.getTime());        //Setear el tiempo basandose en GSM tiempo. (-2h)

  //Los numeros autorizados ya han sido conseguidos. La proxima peticion será mañana
  pingDay = rtc.getDay() + 1;
   if(pingDay >= 30)          //Fin de mes! 
      pingDay = 1;            //Setar la fecha para el dia 1
  
  
    
  
}


void loop() {

  
  if(!isDoneReset)                                  //Acaba de resetearse. Avisar a Serial y a Servidor
  {
    Serial.println("CPU Succesfuly reseted!"); 
    isDoneReset = true;
    Watchdog.reset();                               //Reiniciar el temporizador de perro de guardia(20segundos). 
    connectToApi("/ping?action=reset");             //Conectarse al servidor para avisar que CPU ha sido reseteado
  }
  if(doLog)                                         //Hay información para mandar al servidor
  {
    if(millis() > (w8 + 5000))                      //Esperar 5 segundos para asegurarse que no se produce error por falta de tiempo.
    {
      doLog = false;
      Watchdog.reset();                             //Reiniciar el temporizador de perro de guardia(20segundos). 
      connectToApi(logBuf);                         //Conectarse al servidor para mandar el Log
    }
  }
     
  
  if(gsmAccess.isAccessAlive())                     //Tiene conección
    { 
      Watchdog.reset();                             //Reiniciar el temporizador de perro de guardia(20segundos). 
      look_for_call();                              //Esperar lamadas
      look_for_msg();                               //Esperar SMS
      blink_led(200, 1);                            //Parpadeo de led 200 ms Hight 1000ms LOW => Conectado
      delay(1000);                                  
    }
    else                                          //Conecion perdida
    {
    blink_led(100,1);                             //Parpadear el LED. Parpadeo rapido => Sin conneccion. 
      if(!isResetMsg)                             //Imprimir en el Serial 
      {
      Serial.println("Not allive! Reset CPU soon...."); 
      isResetMsg = true;
      }
    }

    //Compbrobación de proxima peticion para conseguir los numeros autorizados.
    if(rtc.getHours() == pingHour && rtc.getMinutes() >= pingMinute && rtc.getDay() == pingDay ){
      /* El caso Ideal
      pingDay = rtc.getDay() + 1;
      if(pingDay >= 30)
        pingDay = 1;
        
      Serial.println("Next Ping day = ");
      Serial.print(pingDay);
      Serial.println();  
      */
      //Hay momentos cuando no hay connexion a la red por lo tanto no se va a poder obtener toda la lista de numeros autorizados.
      //Por eso más seguro es entrar en infinite loop hasta el reseteo de CPU
      while(1);
    } 
}
/*Funcion de Espera de llamada entrante
 * 1. Espera la llamada
 * 2. Validar el numero entrante
 * 3. Cortar la llamada
*/
void look_for_call()
{
 // Check the status of the voice call
  switch (vcs.getvoiceCallStatus()) {
    case IDLE_CALL: // No está pasando nada
      break;
    case RECEIVINGCALL: //Alguien está llamando
      Serial.println("RECEIVING CALL");
      //Sacar el numero de llamada entrante
      vcs.retrieveCallingNumber(numtel, 20);
      // Imprimir en Serial el numero
      Serial.print("Number:");
      Serial.println(numtel);
      validate_nr(numtel);                //Validar el numero
      vcs.hangCall();                     //Cortar llamada
      break;

   
  }
}
/*Funcion de espera de SMS
 * 1. Ver si hay mensajes disponibles
 * 2. Sacar el numero que manda el mensaje
 * 3. Si es administrador - hacer algo. Si no - borrar mensaje
 * 4. Leer el mensaje
 * 5. Manejar el mensaje
 * 6. Borrar el mensaje
*/
void look_for_msg()
{
  int c;

  
  if (sms.available()) {                      //Hay mensaje nuevo
    Serial.println("Message received from:");

    //Sacar numero
    sms.remoteNumber(numtel, 20);
    Serial.println(numtel);
    String nr = numtel;
    if(nr.indexOf(ADMIN) < 0) {                //No es administrador
      Serial.println("Unautorized number! Deleting message");
      sms.flush();                           //Borrar mensaje
    }
    else                                    //Es administrador
    {
    // Leer los bytes del mensaje e imprímalos
    String msg = "";
    while ((c = sms.read()) != -1) {
      Serial.print((char)c);
      msg.concat((char)c);
    }
    Serial.println("\nEND OF MESSAGE");
    //Borrar el mensaje
    sms.flush();
    Serial.println("MESSAGE DELETED");
    handle_sms(msg);                      //Manejar el mensaje
    }
  }
}


/*Funcion para validar el numero
 * 
*/
void validate_nr(String nr)
{
 Serial.println("Validating number...");
 int i = 0;
 bool is_auth = false;
 String p;
 
 while (phoneList[i]) {
     if(get_nr(nr) == phoneList[i])
      {
        is_auth = true;
        break;
      }
    i++;  
  }

  if(is_auth)
  {
    Serial.println("Authorized!");
    send_signal();
    p = "/ping?action=authorized&nr=" + nr;
  }
  else
  {
    p = "/ping?action=unauthorized&nr=" + nr;
    Serial.println("Unauthorized!");  
  }
  
  p.toCharArray(logBuf, 50);
  doLog = true;
  w8 = millis();  
}
/*Funcion para Manejar el SMS
*/
void handle_sms(String msg)
{
 if(msg == "update")              //Actualizar la lista de numeros autorizados
     connectToApi(path);
  if(msg == "reset")              //Resetear el CPU
      while(1);
  
}

//String to long
long get_nr(String nr)
{
  char nr_buf[10];
  nr.toCharArray(nr_buf, 10);
  return atol(nr_buf);
}

//Mandar la señal RF
void send_signal()
{
  Serial.println("Opening...");

  //La señal es de 7 circulos identicos
  for (int j = 0; j < 7; j++)
    single_signal();          //Un circulo
}

//Crear un circulo de señal RF
void single_signal()
{
    int i = 0;
        while(i < TIMES)
        {
            digitalWrite(RF_TRANSMITER, HIGH);
            delayMicroseconds(h_delay[i]);
            digitalWrite(RF_TRANSMITER, LOW);
            delayMicroseconds(l_delay[i]);
        i++;
        }
    
      delayMicroseconds(800);        //El retraso entre los circulos
}

//El parpadeo de led
void blink_led(int dur, int t)
{
  int i = 0;
  while(i < t)
  {
    digitalWrite(6, HIGH);
    delay(dur);
    digitalWrite(6, LOW);
    delay(dur);
    i++;
  }
}


//Configuracion de GSM
bool startGSM()
{
   
    int gsmBeginStatus;
    int gprsAttachStatus;
    int startWebClientInitializationCount = 0;
    int gsmReadyStatus = 0;

    Serial.println("Start GSM");

    // Initialize the GSM with a modem restart and asynchronous operation mode.
    // I modified the MODEM instance in the MKRGSM 1400 library to initialize with a baud rate of 115200.
    gsmBeginStatus = gsmAccess.begin(PINNUMBER, true, false);
    if (gsmBeginStatus == 0)
    {
        // Modem has been restarted. Delay for 2 seconds and loop while GSM component initializes and registers with network
        delay(2000);

        // March thru the GSM state machine one AT command at a time using the ready() method.
        // This allows us to detect a hang condition on the SARA U201 UART interface
        do
        {
            gsmReadyStatus = gsmAccess.ready();
            startWebClientInitializationCount++;
            delay(100);
        } while ((gsmReadyStatus == 0) && (startWebClientInitializationCount < 600));

        // If the GSM registered with the network attach to the GPRS network with the APN, login and password
        if (gsmReadyStatus == 1  && (gprs.attachGPRS(GPRS_APN, GPRS_LOGIN, GPRS_PASSWORD) == GPRS_READY))
        {
            Serial.print("GSM registered successfully after ");
            Serial.print(startWebClientInitializationCount * 100);
            Serial.println(" ms");
            connectToApi(path);
            return true;
        }
        else if (gsmReadyStatus == 0)
        {
            Serial.print("GSM Ready status timed OUT after ");
            Serial.print(startWebClientInitializationCount * 100);
            Serial.println(" ms");
            return false;
        }
        else
        {
            // Print gsmReadyStatus as ASCII encoded hex because occasionally we get garbage return characters when the network is in turmoil.
            Serial.print("GSM Ready status: ");
            Serial.println(gsmReadyStatus, HEX);
            return false;
        }
    }
    else
    {
        Serial.print("GSM Begin status: ");
        Serial.println(gsmBeginStatus);
        return false;
    }
}

//Conneccion al servidor
void connectToApi(char p[])
{
  Serial.println("connecting...");
   if (client.connect(server, port)) {
    Serial.println("connected");
    // Make a HTTP request:
    client.print("GET ");
    client.print(p);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(server);
    client.println("Connection: close");
    client.println();
  } else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }
  delay(3000);
   String con = "";
  char c;
   while (client.available()) {
     c = client.read();
     con += char(c);
  }
  // Si el servidor está desconectado, detenga el cliente:
  if (!client.available() && !client.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
  }
  Serial.print("MSG FROM API: ");
  Serial.println(con);

  if(con.indexOf("numbers") > 0)
    {
      makePhoneList(con);                     //Crear la lista de numeros autorizados
    }
}


void makePhoneList(String phones)
{
  int startI = phones.indexOf("[") + 1;
  int endI = phones.indexOf("]");
  phones = phones.substring(startI, endI);
  Serial.println(phones[0]);
  Serial.println(phones.length());
  String curPhone = "";
  int totalPhones = 0;
  int i = 0;
   while(phoneList[i])
  {
    phoneList[i] = 0;
    i++;
  }
  for(int i = 0; i < phones.length(); i++)
  {
    curPhone += phones[i];
    if(curPhone.length() < 9)
      if(curPhone.indexOf("\"") >= 0 || curPhone.indexOf(",") >= 0)
        curPhone = "";
  
    if(curPhone.length()== 9)
      {
        if(totalPhones < MAX_NR)
        {
          
          phoneList[totalPhones] = get_nr(curPhone);
           Serial.println(phoneList[totalPhones]);
          totalPhones++;
        }
        
        curPhone = "";
      }
  }
  i = 0;
  Serial.println("Printing all numbers from long []");
  while(phoneList[i])
  {
    Serial.println(phoneList[i]);
    i++;
  }
}
