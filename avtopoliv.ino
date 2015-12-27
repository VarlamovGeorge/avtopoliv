#define BLYNK_DEBUG
#define BLYNK_PRINT Serial 
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <ESP8266_HardSer.h>
#include <BlynkSimpleShieldEsp8266_HardSer.h>
#include <SimpleTimer.h>

//Настраиваем UART
#define EspSerial Serial3
ESP8266 wifi(EspSerial);

//Настройки виджетов Blynk
WidgetLED led1(1);
WidgetLED led2(29);
WidgetLED led3(30);
WidgetLED led4(31);
WidgetTerminal terminal(V4);

//Настройка почты, на которую будут отправляться уведомления:
#define EMAIL "varlamovg@mail.ru"

SimpleTimer timer;

//Подключаем экран к пинам 4-8:
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

unsigned long currentMillis, currentMillis2;
int humidity = 0;
int mode = 2;//По умолчанию выставлен режим работы по сенсору
//Опрелеляем значения для считывания кнопок с кейпадшилда:
#define noneVal 900
#define rightVal 50
#define upVal 200
#define downVal 350
#define leftVal 550
#define selectVal 850

//Пин, отвечающий за подсветку дисплея
#define BACKLIGHT_PIN 10
//Пины, отвечающие за управление реле крана
#define VALVE_POWER_PIN 23
#define VALVE_SWITCH_PIN 22
//Пин, подача напряжения на который подает питание на датчик влажности
#define HUMIDITY_POWER_PIN 24
//Аналоговый пин, на который подается значение с датчика влажности
#define HUMIDITY_READ_PIN A7

//Прочие переменные:
bool resultConnection;
boolean isWatering=false;
boolean humc=false;
//Массив для навигации по меню:
int arrMenu[4]={2, 1, 0, 0};//По умолчанию - режим по сенсору
//Параметры полива:
byte watDuration; //Длительность полива (мин.). Дальше будет ограничена от 1 до 100
byte watDelay;//Задержка между поливами (12, 24, 36 или 48 часов)
byte humTreshold;//Пороговое значение влажности почвы в относительныъ единицах. Будет ограничена значениями 1-100 ед.
char buffer[64];

//Параметры подключения к Wi-Fi:
char Ssid[16];
char Pass[32];
char Auth[32];
int ssidMenu[2]={0, 65};//0-первоначальная длина SSID; 65-ASCII номер символа, который выводится на экране (первоначально - А)
int passMenu[2]={0, 65};//0-первоначальная длина SSID; 65-ASCII номер символа, который выводится на экране (первоначально - А)
int authMenu[2]={0, 65};//0-первоначальная длина SSID; 65-ASCII номер символа, который выводится на экране (первоначально - А)
String ssidName, passName, authName;

//Вспомогательные переменные для отсчета времени:
unsigned int backlightTime=0;
int backlightSec;

//---------------------------------------
void setup()
{
    Serial.begin(9600);  
  delay(10);  
  EspSerial.begin(115200);
  delay(10);
  
  lcd.begin(16, 2); //Инициализируем экран
  LightLCDon();//Включаем подсветку
  uint8_t del[8]  = {0x11,0x11,0xA,0x4,0xA,0x11,0x11};
  lcd.createChar(0, del);
// Выводим сообщение 
  lcd.setCursor(0,0); 
//выводим строку 1
  lcd.print("Avtopoliv v2.13");
//аналогично выводим вторую строку
  lcd.setCursor(1,1);
  lcd.print("Varlamov G.2015");
  delay(1000);
  
//EEPROM:
//Адрес: 0-Длительность полива; 1-Задержка между поливами; 2-порог влажности почвы; 10-Длина SSID; 11-27-Символы SSID Wi-Fi;
//50-Длина Password Wi-Fi; 51-82-Символы Password Wi-Fi; 90-Длина Blynk auth-идентификатора; 91-122-символы Blynk auth-идентификатора
watDuration=EEPROM.read(0);//Считываем значение длительность полива из памяти EEPROM 0
watDelay=EEPROM.read(1);//Считываем задержку между поливами из памяти EEPROM 1
humTreshold=EEPROM.read(2);//Считываем порог влажности почвы из памяти EEPROM 2
ssidMenu[0]=EEPROM.read(10); //Считываем длительность имеющегося SSID из EEPROM 10
if (ssidMenu[0]>16) ssidMenu[0]=0;
for (int i=0; i<ssidMenu[0]; i+=1) {//Последовательно считываем символы SSID сети Wi-Fi из EEPROM 11-49
  Ssid[i]=EEPROM.read(11+i);
  ssidName=ssidName+Ssid[i];
  }
passMenu[0]=EEPROM.read(50); //Считываем длительность имеющегося Password из EEPROM 50
if (passMenu[0]>32) passMenu[0]=0;
for (int i=0; i<passMenu[0]; i+=1) {//Последовательно считываем символы Password сети Wi-Fi из EEPROM 51-89
  Pass[i]=EEPROM.read(51+i);
  passName=passName+Pass[i];
  }
authMenu[0]=EEPROM.read(90); //Считываем длительность имеющегося Blynk Auth из EEPROM 90
if (authMenu[0]>32) authMenu[0]=0;
for (int i=0; i<authMenu[0]; i+=1) {//Последовательно считываем символы Password сети Wi-Fi из EEPROM 91-122
  Auth[i]=EEPROM.read(91+i);
  authName=authName+Auth[i];
}

//Группа выходов, отвечающая за открытие-закрытие крана:
pinMode(VALVE_POWER_PIN, OUTPUT);//Низкий уроень подает питание к реле
pinMode(VALVE_SWITCH_PIN, OUTPUT);//Низкий-высокий уровень меняют полярность питания, подающегося на кран
digitalWrite(VALVE_POWER_PIN, HIGH);
digitalWrite(VALVE_SWITCH_PIN, HIGH);
pinMode(HUMIDITY_POWER_PIN, OUTPUT);//Подача питания на сенсор влажности
digitalWrite(HUMIDITY_POWER_PIN, LOW);

//Контакт, отвечающий за подсветку экрана
pinMode(BACKLIGHT_PIN, OUTPUT);//
Valve(0);//Закрытие крана если он по какой-то причине был открыт

//Подключаемся к Wi-Fi + Blynk
LCDWright("Start connect"," to Wi-Fi&Blynk");
delay(2000);
LCDWright(ssidName,passName);
Serial.println("SSID: "+ssidName+"; Pass: "+passName+"; Auth: "+authName);
Blynk.begin(authName.c_str(), wifi, ssidName.c_str(), passName.c_str());

//Пробуем подключиться к серверу и в положительном случае обновляем в приложении значения led-виджетов:
resultConnection = Blynk.connect();
if (resultConnection){
  LCDWright("Connection", "status: OK!");
  Blynk.notify("Avtopoliv connected");
  Initialize();//Актуализируем led-виджеты
  humCheck();//Сразу считываем влажность почвы
}
else{
  LCDWright("Connection", "status: ERROR!");
  delay(2000);
}

//Периодически запускаемые действия:
timer.setInterval(10000L, humCheck); //Опрашиваем датчик влажности каждые 10 секунд и отправляем в приложение
timer.setInterval(3600000L, sensorAction); //Раз в час проверяем уровень влажности и если он ниже заданного значения - включаем полив и оповещаем в приложении и по почте
timer.setInterval(60000L, schedulerAction); //Раз в 

}//End Setup()

//----------------------------------
void loop()
{
 if(resultConnection) Blynk.run();//Запускаем блинк только если удалось подключиться к серверу!
 timer.run();
 
 backlightTime=backlightTime+1;
 backlightSec=backlightTime/20;
 readLCDbtn();//Опрос кнопки
 Menu(arrMenu);
 LightLCDoff();//Отключение дисплея при простое
 if (humc==true && millis()-currentMillis2>2500){//Если датчик уже включен и работает больше 2,5 секунд то считать значение влажности
  humidity = map(analogRead(HUMIDITY_READ_PIN), 100, 400, 100, 0); //Самая большое значение на датчике влажности соответствует наименьшей влажности (сопротивление максимально)
  Blynk.virtualWrite(V8, humidity);
  digitalWrite(HUMIDITY_POWER_PIN, LOW);
  humc=false;
 }
 delay(50);
}

//---------------------------------
//Функция считывания кнопки (глобально)
void readLCDbtn()
{
  int keyIN = analogRead(0); //Чтение нажатой кнопки с аналогового пина 0
  
  if (keyIN > noneVal) {}

  else if (keyIN < rightVal) { //Нажата кнопка Right
    RightPress();
  }

  else if (keyIN < upVal && keyIN>rightVal) {//Нажата кнопка Up
    UpPress();
  }

  else if (keyIN < downVal && keyIN>upVal) {//Нажата кнопка DOWN
    DownPress();
  }

  else if (keyIN < leftVal && keyIN>downVal) { //Нажата кнопка Left
    LeftPress();
  }

  else if (keyIN< selectVal && keyIN>leftVal){ //Нажата кнопка Select
    SelectPress();
  }
  
}

//-----------------------------------
//Функция вывода на экран двух произвольных строк:
void LCDWright(String a1, String b1) {
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print(a1);
 lcd.setCursor(0, 1);
 lcd.print(b1);
}

//-----------------------------------
//Функция вывода МЕНЮ на LCD-дисплей:
void Menu(int arr[4]){
  int keyIN2;
  int intArr=1000*arr[0]+100*arr[1]+10*arr[2]+arr[3];
  switch (intArr){
//Страница, включающаяся только во время полива
case 0000:
{
  LCDWright("WATERING", "Minutes left:"+String(watDuration-(millis()-currentMillis)/60000));
  if(((millis()-currentMillis)%60000)<100){//Каждую минуту выводи на терминал количество минут до конца полива
    terminal.println(String(watDuration-(millis()-currentMillis)/60000)+" minutes left");
    terminal.flush();
  }  
  if(millis()-currentMillis>watDuration*60000){//Заканчиваем полив по истечению заданного в минутах времени
    LCDWright("STOP WATERING", "");
    terminal.println("Watering finished!");
    terminal.flush();
    Valve(0);
    switch (mode){//Возвращаемся в пункт меню, откуда был запущен полив
      case 0://Ручной режим
      {
      arrMenu[0]=3;
      arrMenu[1]=0;
      arrMenu[2]=0;
      arrMenu[3]=0;
      }
      break;
      
      case 1://Режим по расписанию
      {
      arrMenu[0]=1;
      arrMenu[1]=1;
      arrMenu[2]=1;
      arrMenu[3]=0;    
      }
      break;
      
      case 2://Режим по сенсору
      {
      arrMenu[0]=2;
      arrMenu[1]=1;
      arrMenu[2]=0;
      arrMenu[3]=0; 
      }
      break;
    }
  }
}
break;    
    
//Верхний уровень меню (1-й)    
case 1000:
  LCDWright("Schedule mode", "");
  break;
case 2000:
  LCDWright("Sensor mode", "");
  break;
case 3000:
  LCDWright("Manual mode", "");
  break;
case 4000:
  LCDWright("Watering", " settings");
  break;
case 5000:
  LCDWright("Wi-Fi settings", "");
  break;
//2-й уровень меню
case 1100:
  LCDWright("Press SELECT to", "launch schedule");
//  При нажатии кнопки select должен запускаться обратный отсчет (см. функцию "SelectPress()")
  break;
case 2100:
  LCDWright("Sensor mode on", "Humidity:"+String(humidity)+"%");
  break;
case 3100:
  LCDWright("Press SELECT to", "launch watering");
//  При нажатии кнопки select должен запускаться полив (см. функцию "SelectPress()")
  break;
case 4100:
  LCDWright("Set watering", " duration");
  break;
case 4200:
  LCDWright("Set watering", " delay");
  break;
case 4300:
  LCDWright("Set humidity", " treshold");
  break;
case 5100:
  LCDWright("Wi-Fi connection", " status");
  break;
case 5200:
  LCDWright("Turn On/Off", " Wi-Fi");
  break;
case 5300:
  LCDWright("Reconnection", " to Wi-Fi");
  break;
case 5400:
  LCDWright("Set SSID", " of Wi-Fi");
  break;
case 5500:
  LCDWright("Set PASSWORD", " of Wi-Fi");
  break;
case 5600:
  LCDWright("Set Blynk Auth", " of Wi-Fi");
  break;

//3-й уровень меню
case 1110://Включение режима "По расписанию"
{
  LCDWright("Schedule mode on", "Time left: "+String((watDelay*3600000+currentMillis-millis())/3600000)+":"+String(((watDelay*3600000+currentMillis-millis())%3600000)/60000));
  if (((watDelay*3600000+currentMillis-millis())/3600000)==0 && (((watDelay*3600000+currentMillis-millis())%3600000)/60000)==0)
  {
    currentMillis=millis();
    Blynk.email(EMAIL, "Avtopoliv", "Hi! The timer in scheduler is over. Watering started!");
    Blynk.notify("Watering started!");
    watering();
  }
}
break;

case 4110: //настройки длительности полива
    LCDWright("Set duration:",String(watDuration)+" min");//Выводим текущую длительность
    keyIN2 = analogRead(0);//Считываем кнопки
    //delay(50);
    if (keyIN2 < upVal) {//Нажата кнопка Up
    watDuration+=1;//Увеличиваем значение длительности, но не записываем его в EEPROM
    if (watDuration>100)
    watDuration=100;
    }
    
    else if (keyIN2 < downVal) {//Нажата кнопка DOWN
    watDuration-=1;
    if (watDuration<1)//Уменьшаем значение длительности, но не записываем его в EEPROM
    watDuration=1;
    }
    
    else if (keyIN2< selectVal && keyIN2>leftVal){//Нажата кнопка select
      EEPROM.update(0, watDuration);//Записываем в EEPROM новое значение
      LCDWright("Duration updated",String(watDuration)+" min");
      delay(1000);
      arr[2]=0;//Выходим из пункта меню
    }
break;
  
case 4210: //Настройка задержки между поливами (12, 24 или 48 часов)
  LCDWright("Set delay:", String(watDelay)+" hours");
  keyIN2 = analogRead(0);//Считываем кнопки
    if (keyIN2 < upVal) {//Нажата кнопка Up
    watDelay+=12;//Увеличиваем значение длительности, но не записываем его в EEPROM
    if (watDelay>48)
    watDelay=48;
    }

    else if (keyIN2 < downVal) {//Нажата кнопка DOWN
    watDelay-=12;
    if (watDelay<12)//Уменьшаем значение длительности, но не записываем его в EEPROM
    watDelay=12;
    }

    else if (keyIN2< selectVal && keyIN2>leftVal){//Нажата кнопка select
      EEPROM.update(1, watDelay);//Записываем в EEPROM новое значение
      LCDWright("Delay updated:",String(watDelay)+" hours");
      delay(1000);
      arr[2]=0;//Выходим из пункта меню
    } 
break;
  
case 4310: //Настройка порогового значения влажности почвы
  LCDWright("Set treshold:", String(humTreshold)+"  (1-100 pts)");
  keyIN2 = analogRead(0);//Считываем кнопки
  if (keyIN2 < upVal) {//Нажата кнопка Up
    humTreshold+=1;//Увеличиваем значение длительности, но не записываем его в EEPROM
    if (humTreshold>100)
    humTreshold=100;
    }

    else if (keyIN2 < downVal) {//Нажата кнопка DOWN
    humTreshold-=1;
    if (humTreshold<1)//Уменьшаем значение длительности, но не записываем его в EEPROM
    humTreshold=1;
    }

    else if (keyIN2< selectVal && keyIN2>leftVal){//Нажата кнопка select
      EEPROM.update(2, humTreshold);//Записываем в EEPROM новое значение
      LCDWright("Treshold updated",String(humTreshold)+" pts.");
      delay(1000);
      arr[2]=0;//Выходим из пункта меню
    }
break;
  
case 5110:  
  if (resultConnection){
  LCDWright("Wi-Fi connected", "");
  }
  else LCDWright("Wi-Fi is NOT", "connected");
  break;
case 5210:
  if (resultConnection){//Blynk уже подключен
    LCDWright("Wi-Fi status:", " connected");
  }
  else {//Если Blynk не подключен
    LCDWright("Press Select to", "turn On Wi-Fi");
    keyIN2 = analogRead(0);//Считываем кнопки
    if (keyIN2< selectVal && keyIN2>leftVal) {//Нажата кнопка Select
      LCDWright("Trying to connect", "to Wi-Fi...");
      resultConnection = Blynk.connect();
      if (resultConnection){//Удалось подключиться к Blynk
        LCDWright("The Wi-Fi is", "turned ON!");
        Blynk.notify("Avtopoliv connected");
        Initialize();//Актуализируем led-виджеты
      }
      else{//Не удалось подключиться к Blynk
        LCDWright("Connection", "status: ERROR!");
        delay(2000);
      }
    }
  }
break;

case 5220:
  if (!resultConnection){//Blynk уже не подключен
    LCDWright("Wi-Fi status:", "not connected");
  }
  else {//Если Blynk подключен
    LCDWright("Press Select to", "turn Off Wi-Fi");
    keyIN2 = analogRead(0);//Считываем кнопки
    if (keyIN2< selectVal && keyIN2>leftVal) {//Нажата кнопка Select
      LCDWright("Trying to switch", "off the Wi-Fi...");
      Blynk.disconnect();
      resultConnection=Blynk.connected();
      if (!resultConnection){
        LCDWright("The Wi-Fi is", "turned Off!");
      }
      else{
        LCDWright("Switching off", "status: ERROR!");
        delay(1000);
      }
    }
  }
break;

case 5310:
  LCDWright("Press Select to", "reconnect Wi-Fi");
  keyIN2 = analogRead(0);//Считываем кнопки
    if (keyIN2< selectVal && keyIN2>leftVal) {//Нажата кнопка Select
    LCDWright("The Wi-Fi is", "reconnecting");
    if (resultConnection){//Blynk еще подключен
      Blynk.disconnect();
      resultConnection=Blynk.connected();
    }
    resultConnection = Blynk.connect();
      if (resultConnection){//Удалось подключиться к Blynk
        LCDWright("The Wi-Fi is", "reconnected!");
        Blynk.notify("Avtopoliv connected");
        Initialize();//Актуализируем led-виджеты
      }
      else{//Не удалось подключиться к Blynk
        LCDWright("Reconnection", "status: ERROR!");
        delay(2000);
      }
    }
break;

case 5410://Установка SSID Wi-Fi:
  LCDWright("SSID: "+ssidName, "   "+String(ssidMenu[0]));
  lcd.setCursor(0,1);
  lcd.write(ssidMenu[1]);
  keyIN2 = analogRead(0);//Считываем кнопки
  if (keyIN2 < rightVal) { //Нажата кнопка Right
    if(ssidMenu[1]==0) {//Если выбран символ удаления, то удаляем последний символ из строки SSID
      ssidName=ssidName.substring(0,ssidMenu[0]-1);
      if (ssidMenu[0]>0) {
        ssidMenu[0]-=1;
      }
      else {
        ssidMenu[0]=0;
      }
    }
    else {//Добавляем указанный символ  конец строки SSID
    Ssid[ssidMenu[0]]=ssidMenu[1];
    ssidName=ssidName+Ssid[ssidMenu[0]];
    ssidMenu[0]+=1;
    }
  }

  else if (keyIN2 < upVal) {//Нажата кнопка Up
    ssidMenu[1]+=1;
      if (ssidMenu[1]<48) {
        ssidMenu[1]=48;
    }
      else if (ssidMenu[1]>122) {
        ssidMenu[1]=122;
    }
    }

  else if (keyIN2 < downVal) {//Нажата кнопка DOWN
    ssidMenu[1]-=1;
      if (ssidMenu[1]<=48) {
        ssidMenu[1]=0;
      }
    }

  else if (keyIN2 < leftVal) { //Нажата кнопка Left
  ssidMenu[0]-=1;
    if (ssidMenu[0]<0) {
    ssidMenu[0]=0;
    }
  }
  
  else if (keyIN2< selectVal && keyIN2>leftVal){//Нажата кнопка select
      EEPROM.update(10, ssidMenu[0]);//Записываем в EEPROM новое значение длины SSID сети Wi-Fi
      for (int i=1; i<=ssidMenu[0]; i+=1) {//Записываем в EEPROM последовательно ASCII коды символов SSID сети Wi-Fi
        EEPROM.update(10+i, Ssid[i-1]);
      }
      LCDWright("SSID updated",ssidName + " (" + String(ssidMenu[0]) + ")");
      delay(2000);
      //espSerial.println("3,"+ssidName);
      Serial.println("3,"+ssidName);
      LCDWright("SSID is loading","to Wi-Fi module");
      delay(2000);
      arr[2]=0;//Выходим из пункта меню
    }
break;

case 5510://Установка Password сети Wi-Fi:
  LCDWright("Password: "+passName, "   "+String(passMenu[0]));
  lcd.setCursor(0,1);
  lcd.write(passMenu[1]);
  keyIN2 = analogRead(0);//Считываем кнопки
  if (keyIN2 < rightVal) { //Нажата кнопка Right
    if(passMenu[1]==0) {//Если выбран символ удаления, то удаляем последний символ из строки Password
      passName=passName.substring(0,passMenu[0]-1);
      if (passMenu[0]>0) {
        passMenu[0]-=1;
      }
      else {
        passMenu[0]=0;
      }
    }
    else {//Добавляем указанный символ  конец строки Password
    Pass[passMenu[0]]=passMenu[1];
    passName=passName+Pass[passMenu[0]];
    passMenu[0]+=1;
    }
  }

  else if (keyIN2 < upVal) {//Нажата кнопка Up
    passMenu[1]+=1;
      if (passMenu[1]<48) {
        passMenu[1]=48;
    }
      else if (passMenu[1]>122) {
        passMenu[1]=122;
    }
    }

  else if (keyIN2 < downVal) {//Нажата кнопка DOWN
    passMenu[1]-=1;
      if (passMenu[1]<=48) {
        passMenu[1]=0;
      }
    }

  else if (keyIN2 < leftVal) { //Нажата кнопка Left
  passMenu[0]-=1;
    if (passMenu[0]<0) {
    passMenu[0]=0;
    }
  }
  
  else if (keyIN2< selectVal && keyIN2>leftVal){//Нажата кнопка select
      EEPROM.update(50, passMenu[0]);//Записываем в EEPROM новое значение длины Password сети Wi-Fi
      for (int i=1; i<=passMenu[0]; i+=1) {//Записываем в EEPROM последовательно ASCII коды символов Password сети Wi-Fi
        EEPROM.update(50+i, Pass[i-1]);
      }
      LCDWright("Pass updated",passName + " (" + String(passMenu[0]) + ")");
      delay(2000);
      //espSerial.println("4,"+passName);
      Serial.println("4,"+passName);
      LCDWright("Pass is loading","to Wi-Fi module");
      delay(2000);
      arr[2]=0;//Выходим из пункта меню
    }
break;  
  
case 5610://Установка Blynk Auth для подключения Blynk к серверу:
  LCDWright("Auth: "+authName, "   "+String(authMenu[0]));
  lcd.setCursor(0,1);
  lcd.write(authMenu[1]);
  keyIN2 = analogRead(0);//Считываем кнопки
  if (keyIN2 < rightVal) { //Нажата кнопка Right
    if(authMenu[1]==0) {//Если выбран символ удаления, то удаляем последний символ из строки Auth
      authName=authName.substring(0,authMenu[0]-1);
      if (authMenu[0]>0) {
        authMenu[0]-=1;
      }
      else {
        authMenu[0]=0;
      }
    }
    else {//Добавляем указанный символ  конец строки Auth
    Auth[authMenu[0]]=authMenu[1];
    authName=authName+Auth[authMenu[0]];
    authMenu[0]+=1;
    }
  }

  else if (keyIN2 < upVal) {//Нажата кнопка Up
    authMenu[1]+=1;
      if (authMenu[1]<48) {
        authMenu[1]=48;
    }
      else if (authMenu[1]>122) {
        authMenu[1]=122;
    }
    }

  else if (keyIN2 < downVal) {//Нажата кнопка DOWN
    authMenu[1]-=1;
      if (authMenu[1]<=48) {
        authMenu[1]=0;
      }
    }

  else if (keyIN2 < leftVal) { //Нажата кнопка Left
  authMenu[0]-=1;
    if (authMenu[0]<0) {
    authMenu[0]=0;
    }
  }
  
  else if (keyIN2< selectVal && keyIN2>leftVal){//Нажата кнопка select
      EEPROM.update(90, authMenu[0]);//Записываем в EEPROM новое значение длины Blynk Auth
      for (int i=1; i<=authMenu[0]; i+=1) {//Записываем в EEPROM последовательно ASCII коды символов Blynk Auth
        EEPROM.update(90+i, Auth[i-1]);
      }
      LCDWright("Auth updated",authName + " (" + String(authMenu[0]) + ")");
      delay(2000);
      //espSerial.println("5,"+authName);
      Serial.println("5,"+authName);
      LCDWright("Auth is loading","to Wi-Fi module");
      delay(2000);
      arr[2]=0;//Выходим из пункта меню
    }
break;
  
//4-й уровень
case 5211:
  LCDWright("Turning On", " the Wi-Fi");
  break;
case 5221:
  LCDWright("Turning Off", " the Wi-Fi");
  break;
  }
}

//----------------------------------
//Функция открытия-закрытия крана:
void Valve(boolean ValveState) {
  if (ValveState==true){//Открыть кран
    digitalWrite(VALVE_SWITCH_PIN, LOW);
    isWatering=true;
    led1.on();
  }
  else if (ValveState==false){//Закрыть кран
    digitalWrite(VALVE_SWITCH_PIN, HIGH);
    isWatering=false;
    led1.off();
  }
 Serial.println("1,"+String(ValveState)); 
 delay(500);
 digitalWrite(VALVE_POWER_PIN, LOW);
 delay(4000);
 digitalWrite(VALVE_POWER_PIN, HIGH);//Отключаем все реле чтобы не тратить энергию
 delay(500);
 digitalWrite(VALVE_SWITCH_PIN, HIGH);//Отключаем все реле чтобы не тратить энергию

}

//----------------------------------
//Функция отключения подсветки экрана:
void LightLCDoff(){
 if  (backlightSec>30){
 digitalWrite(BACKLIGHT_PIN, 0);
 }
}

//----------------------------------
//Функция включения подсветки экрана:
void LightLCDon(){
 backlightTime=0;
 backlightSec=0;
 digitalWrite(BACKLIGHT_PIN, 1);
}

//----------------------------------
//Функция опроса датчика влажности почвы и отправки данных в мобильное приложение:
void humCheck(){
  digitalWrite(HUMIDITY_POWER_PIN, HIGH);
  humc=true;
  currentMillis2=millis();
/*  delay(2000);
  humidity = map(analogRead(HUMIDITY_READ_PIN), 100, 400, 100, 0); //Самая большое значение на датчике влажности соответствует наименьшей влажности (сопротивление максимально)
  //humidity = map(analogRead(HUMIDITY_READ_PIN), 0, 1023, 100, 0);
  Serial.println("2,"+String(humidity));
  Blynk.virtualWrite(V8, humidity);
//  delay(100);
  digitalWrite(HUMIDITY_POWER_PIN, LOW);*/
}

//----------------------------------
//Функция актуализации LED-виджетов и отправки текущих настроек в терминал приложения:
void Initialize(){
  Serial.println("Initialize started!");
  if (isWatering) led1.on(); 
  else led1.off();

 switch (mode){
      case 0://Ручной режим
      {led2.on();
      led3.off();
      led4.off();
      terminal.println("Manual mode selected");
      }
      break;
      
      case 1://Режим по расписанию
      {led2.off();
      led3.on();
      led4.off();
      terminal.println("Schedule mode selected");           
      }
      break;
      
      case 2://Режим по сенсору
      {led2.off();
      led3.off();
      led4.on();
      terminal.println("Sensor mode selected");           
      }
      break;
    }
   terminal.println("----------------------------------");
   terminal.println("Congratulation! CONNECTION DONE!");
   terminal.println("Current settings from EEPROM:");
   terminal.println("Water duration (min): "+ String(watDuration));
   terminal.println("Watering delay (hours): "+ String(watDelay));
   terminal.println("Humidity treshold (pts): "+ String(humTreshold));
   terminal.println("----------------------------------");
   terminal.flush();
}

//----------------------------------
//Функция запуска полива:
void watering(){
  currentMillis=millis();
  for(int i = 0; i < 4; i = i + 1) {
    arrMenu[i]=0; //Переходим на страничку полива (0,0,0,0)
  }
  terminal.println("Watering started!");
  terminal.println(String(watDuration)+" minutes left");
  terminal.flush();
  Valve(1);//Открытие крана
}

//----------------------------------
//Функция обработки нажатия клавиши "ВПРАВО":
void RightPress()
{
    LightLCDon();//Сразу включаем экран
  if (arrMenu[0]!=0) //Проверка того, что полив не осуществляется
  {
    if (arrMenu[1]==0) //Проверка того, что мы находимся на верхнем уровне меню (1-м)
    {
      arrMenu[1]=1; //Проваливаемся на нижестоящий пункт меню-2-й уровень
      if(arrMenu[0]==2){//Проверяем, что если запущен режим сенсора (пункт меню 2100), то в приложение вывести соответствующую информацию
        mode=2;
        ledModeCheck();
      }
    }
   else if (arrMenu[2]==0 && (arrMenu[0]==4 || arrMenu[0]==5)) //только в случае, если верхний уровень меню равен 4 или 5
    {
      arrMenu[2]=1; //Проваливаемся на нижестоящий пункт меню-3-й уровень 
    }
   else if (arrMenu[3]==0 && arrMenu[0]==5 && arrMenu[1]==2 && (arrMenu[2]==1 || arrMenu[2])==2) //только в случае, если верхний уровень меню равен 5, 2-й равен 2, 3-й - 1 или 2
    {
      arrMenu[3]=1; //Проваливаемся на нижестоящий пункт меню-4-й уровень
    }
   else
    {
      // do nothing
    }
    delay(150);
  }
}

//----------------------------------
//Функция обработки нажатия клавиши "ВЛЕВО":
void LeftPress()
{
    LightLCDon();//Сразу включаем экран
    if (arrMenu[3]!=0) //Проверка того, что мы находимся 4-м уровне меню
    {
      arrMenu[3]=0; //Поднимаемся на вышестоящий пункт меню-3-й уровень
    }
   else if (arrMenu[2]!=0) //Проверка того, что мы находимся 3-м уровне меню
    {
      arrMenu[2]=0; //Поднимаемся на вышестоящий пункт меню-2-й уровень 
      if(arrMenu[0]==1 && arrMenu[1]==1){//Проверяем, если мы вышли из режима по расписанию (пункт меню 1110)
        mode=0;//Отображаем в приложении, что система перешла в режим ручного управления
        ledModeCheck();
      }
    }
   else if (arrMenu[1]!=0) //Проверка того, что мы находимся 2-м уровне меню
    {
      arrMenu[1]=0; //Поднимаемся на вышестоящий пункт меню-1-й уровень
      if(arrMenu[0]==2){//Проверяем, если мы вышли из режима по сенсору (пункт меню 2100)
        mode=0;//Отображаем в приложении, что система перешла в режим ручного управления
        ledModeCheck();
      }
    }
   else if (arrMenu[0]==0) //Проверка того, что идет полив
    {
      LCDWright("STOP WATERING", "");
      terminal.println("Watering manualy stopped!");
      terminal.flush();
      Valve(0);
      arrMenu[0]=1; //Переходим в главное меню (1,0,0,0)
    }
    delay(150); 
}

//----------------------------------
//Функция обработки нажатия клавиши "ВНИЗ":
void DownPress()
{
  if (arrMenu[0]!=0) //Проверка того, что полив не осуществляется
  {
  LightLCDon();//Сразу включаем экран
  if (arrMenu[1]==0) //Проверка того, что мы находимся 1-м уровне меню
    {
      arrMenu[0] += 1; //Двигаемся по тому же уровню вверх
        if (arrMenu[0]>5){ //На самом верхнем уровне только 5 пунктов
          arrMenu[0]=1;
        }
    }
   else if (arrMenu[2]==0) //Проверка того, что мы находимся 2-м уровне меню
    {
         if (arrMenu[0]==1 || arrMenu[0]==3 || arrMenu[0]==3){ //На 2-м уровне меня для пунктов 1,2,3 - 3-й уровень состоит только из одного элемента
          arrMenu[1]=1;
        }
        else if (arrMenu[0]==4){
          arrMenu[1]+=1;
          if (arrMenu[1]>3){ //Для пункта 4 верхнего уровня меню есть только 3 значения: 1,2,3
          arrMenu[1]=1;
        }
        }
        else if (arrMenu[0]==5){
          arrMenu[1]+=1;
          if (arrMenu[1]>6){ //Для пункта 5 верхнего уровня меню есть только 6 значений: 1,2,3,4,5,6
          arrMenu[1]=1;
        }
        }
    }
    else if (arrMenu[3]==0) //Проверка того, что мы находимся 3-м уровне меню
    {
      if ((arrMenu[0]==4  && (arrMenu[1]==1 || arrMenu[1]==2 || arrMenu[1]==3)) || (arrMenu[0]==5 && (arrMenu[1]==1 || arrMenu[1]==3 || arrMenu[1]==4 || arrMenu[1]==5 || arrMenu[1]==6))){
        arrMenu[2]=1; //Для пунктов 4.1, 4.2, 4.3, 5.1, 5.3, 5.4, 5.5, 5.6 - есть только один пункт 3-го уровня меню
      }
      else if (arrMenu[0]==5 && arrMenu[1]==2) {
       arrMenu[2] += 1;
       if (arrMenu[2]>2){ //Для пункта 5.2 - 3-й уровень может принимать только значения 1 или 2
          arrMenu[2]=1;
        }
      }
    }
  delay(150); 
  }
}

//----------------------------------
//Функция обработки нажатия клавиши "ВВЕРХ":
void UpPress()
{
  LightLCDon();//Сразу включаем экран
  if (arrMenu[0]!=0) //Проверка того, что полив не осущестляется
  {
    if (arrMenu[1]==0) //Проверка того, что мы находимся 1-м уровне меню
    {
      arrMenu[0] -= 1; //Двигаемся по тому же уровню вниз
        if (arrMenu[0]<1){ //На самом верхнем уровне только 5 пунктов
          arrMenu[0]=5;
        }
    }
   else if (arrMenu[2]==0) //Проверка того, что мы находимся 2-м уровне меню
    {
         if (arrMenu[0]==1 || arrMenu[0]==3 || arrMenu[0]==3){ //На 2-м уровне меня для пунктов 1,2,3 - 3-й уровень состоит только из одного элемента
          arrMenu[1]=1;
        }
        else if (arrMenu[0]==4){
          arrMenu[1]-=1;
          if (arrMenu[1]<1){ //Для пункта 4 верхнего уровня меню есть только 3 значения: 1,2,3
          arrMenu[1]=3;
        }
        }
        else if (arrMenu[0]==5){
          arrMenu[1]-=1;
          if (arrMenu[1]<1){ //Для пункта 5 верхнего уровня меню есть только 6 значений: 1,2,3,4,5,6
          arrMenu[1]=6;
        }
        }
    }
    else if (arrMenu[3]==0) //Проверка того, что мы находимся 3-м уровне меню
    {
      if ((arrMenu[0]==4  && (arrMenu[1]==1 || arrMenu[1]==2 || arrMenu[1]==3)) || (arrMenu[0]==5 && (arrMenu[1]==1 || arrMenu[1]==3 || arrMenu[1]==4 || arrMenu[1]==5))){
        arrMenu[2]=1; //Для пунктов 4.1, 4.2, 4.3, 5.1, 5.3, 5.4, 5.5 - есть только один пункт 3-го уровня меню
      }
      else if (arrMenu[0]==5 && arrMenu[1]==2) {
       arrMenu[2] -= 1;
       if (arrMenu[2]<1){ //Для пункта 5.2 - 3-й уровень может принимать только значения 1 или 2
          arrMenu[2]=2;
        }
      }
    }
  delay(150);  
  }
}

//----------------------------------
//Функция обработки нажатия клавиши "ВЫБОР":
void SelectPress()
{
    LightLCDon();//Сразу включаем экран
    if (arrMenu[2]==0) //Проверка того, что мы находимся 2-м уровне меню
    {
      if (arrMenu[0]==3 && arrMenu[1]==1){//Пункт меню 3.1
        LCDWright("Launching","  watering!");
        delay(500);
        watering();
      }
      else if(arrMenu[0]==1 && arrMenu[1]==1){//Пункт меню 1.1
        currentMillis=millis();
        arrMenu[2]=1;//Переход на отдельно обрабатываемый пункт меню 1.1.1
        mode=1;//Отображаем в приложении, что система перешла в режим по расписанию
        ledModeCheck();
      }
    }
    delay(150);
}

//----------------------------------
//Функция обработки нажатия кнопки V2 в приложении (отвечает за запуск полива в ручном режиме):
BLYNK_WRITE(V2)
{
  if(param.asInt()==1 && isWatering==false){//Нажата кнопка и полив в данный момент не осуществляется
    if(mode==0){
      Serial.println("Sent command to start watering.");
      terminal.println("Sent command to start watering.");
      terminal.flush();
      watering();
    }
    else {
      terminal.println("ERROR! Select manual mode to start watering.");
      terminal.flush();      
    }
  }
}

//----------------------------------
//Функция обработки нажатия кнопки V3 в приложении (отвечает за смену режимов работы):
BLYNK_WRITE(V3)
{
  if(param.asInt()==1 && isWatering==false){
    mode+=1;
    if(mode>2) {mode=0;}
    ledModeCheck();
  }
}

//----------------------------------
//Функция работы с терминалом в приложении:
BLYNK_WRITE(V4)
{
  String terminalString = param.asStr();
  if (terminalString == "?") {
    Initialize();
  }
  else if (String("help") == param.asStr()){
    terminal.println("Here is the list of allowed commands:");
    terminal.println("'?' - current confuguration;");
    terminal.println("'1.X' - set watering duration to X minutes;");
    terminal.println("'2.X' - set delay between watering to X hours for Schedule Mode (only 12, 24, 36 or 48 is allowed!);");
    terminal.println("'3.X' - set humidity treshold for Sensor Mode (1-100 points allowed).");
  }
  else {
    String a, b;
    a=terminalString.substring(0,1);
    b=terminalString.substring(2);
    switch (a.toInt()){//Обработка ID пришедшей команды: 
    case 1://Установка длительности полива в минутах (1-100)
       watDuration=constrain(b.toInt(), 1, 100);//Ограничиваем интервалом 1-100
       terminal.println("Watering duration is updated: "+String(watDuration));
       EEPROM.update(0, watDuration);//Записываем в EEPROM новое значение
    break;
    case 2://Установка задержки между поливами в часах (12, 24, 36 или 48)
        if (b.toInt()==12 || b.toInt()==24 || b.toInt()==36 || b.toInt()==48){
          watDelay=b.toInt();
        }
        else{
         watDelay=12;
        }
        terminal.println("Watering delay is updated: "+String(watDelay));
        EEPROM.update(1, watDelay);//Записываем в EEPROM новое значение
    break;
    case 3://Установка порогового значения влажности почвы (1-100)
        humTreshold=constrain(b.toInt(), 1, 100);//Ограничиваем интервалом 1-100
        terminal.println("Humidity treshold is updated: "+String(humTreshold));
        EEPROM.update(2, humTreshold);//Записываем в EEPROM новое значение
    break;
    default:
    terminal.println("Unknown command! To get more information type 'help' in this terminal.");    
    break;
   }
  }
  terminal.flush();
}

//----------------------------------
//Периодическая функция в режиме "по сенсору":
void sensorAction(){//Если система в режиме работы по сенсору и влажность опустилась ниже заданного уровня, то начать полив
  if (humidity<humTreshold && mode==2){
    Blynk.email(EMAIL, "Avtopoliv", "Hi! The humidity ("+String(humidity)+") is less then treshold value ("+String(humTreshold)+"). Watering started!");
    Blynk.notify("Watering started!");
    watering();
  }
}

//----------------------------------
//Периодическая функция в режиме "по расписанию":
void schedulerAction(){
  if (mode==1){
    terminal.println("Schedule mode. Time left: "+String((watDelay*3600000+currentMillis-millis())/3600000)+":"+String(((watDelay*3600000+currentMillis-millis())%3600000)/60000));
    terminal.flush();
  }
}

//----------------------------------
//Функция актуализации LED-виджетов, отвечающих за отображение режима работы:
void ledModeCheck(){
   switch (mode){
      case 0://Ручной режим
      {led2.on();
      led3.off();
      led4.off();
      terminal.println("Manual mode selected");
      arrMenu[0]=3;
      arrMenu[1]=0;
      arrMenu[2]=0;
      arrMenu[3]=0;
      }
      break;
      
      case 1://Режим по расписанию
      {led2.off();
      led3.on();
      led4.off();
      terminal.println("Schedule mode selected");
      arrMenu[0]=1;
      arrMenu[1]=1;
      arrMenu[2]=1;
      arrMenu[3]=0;
      currentMillis=millis();    
      }
      break;
      
      case 2://Режим по сенсору
      {led2.off();
      led3.off();
      led4.on();
      terminal.println("Sensor mode selected");
      arrMenu[0]=2;
      arrMenu[1]=1;
      arrMenu[2]=0;
      arrMenu[3]=0; 
      }
      break;
    }
    terminal.flush();
}
