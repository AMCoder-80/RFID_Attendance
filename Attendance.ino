#include <SPI.h>
#include <MFRC522.h>
#include <SD.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "SSD1306.h"
#include <DNSServer.h>
#include <ESP8266mDNS.h>

#define ss_pin D3
#define RST_pin D4
#define buzzer D8
#define relay D6

MFRC522 rfid(ss_pin, RST_pin);
SSD1306 display(0x3c, 4, 5);
HTTPClient http;
ESP8266WebServer server(80);
DNSServer dns;
File myfile;

String user = "admin";
String pass = "admin";
String response;
String rfid_server;
String content;
String Name;
String Auth_str;
String sta_ssid;
String ID_sender;
String sta_pass;
String ap_ssid = "RFID Managment";
String ap_pass = "12345678";
String wifi_mode;
String IP_str;
String config_str;
String mask_str;
String gateway_str;
String DNS_str;
String DHCP_str = "on";

int wifi_num;

boolean normal = true;
boolean saved = false;

IPAddress IP;
IPAddress Gateway;
IPAddress Subnetmask;
IPAddress DNS;
IPAddress ap_ip(192,168,4,1);
IPAddress net_mask(255,255,255,0);

void setup() {
  WiFi.disconnect();
  Serial.begin(115200);
  pinMode(buzzer, OUTPUT);
  pinMode(relay, OUTPUT);
  digitalWrite(buzzer, HIGH);
  digitalWrite(relay, HIGH);
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(20, 0 , "RFID System");
  display.drawString(0, 15, "Initializing SD...");
  display.display();
  
  SPI.begin();

  if (!SD.begin(16)){
    display.clear();
    display.drawString(0, 0, "SD Failed!!!");
    display.display();
    delay(1500);
    ESP.restart();
  }
  display.clear();
  display.drawString(0, 0, "SD is OK :)");
  display.display();
  delay(1500);

  read_storage();

  server.on("/request", requested);
  server.on("/toggle_mode", toggling);
  server.on("/scan", scan_wifi);
  server.on("/WiFi_Setup", wifi_setup);
  server.on("/Network_Setup", network_setup);
  server.on("/Authentication_Setup", auth_setup);
  server.on("/Commands", commands);
  server.onNotFound(load_page);
  server.begin();

  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", ap_ip);

  rfid.PCD_Init();
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);

  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(15,0, "Normal Mode");
  display.drawString(0, 19, "Wainting for");
  display.drawString(0, 36, "RFID Card >>>");
  display.display();
  
}

void loop() {
  dns.processNextRequest();
  server.handleClient();

  if (normal){
    normal_mode();
  }
  else{
    register_mode();
  }
}
// ------------------------------------------------------------------
void normal_mode(){
  if (!rfid.PICC_IsNewCardPresent()){
    return; 
  }
  if (!rfid.PICC_ReadCardSerial()){
    return;
  }

  content = "";

  for (byte i=0;i<rfid.uid.size;i++){
    Serial.print(rfid.uid.uidByte[i], HEX);
    content.concat(String(rfid.uid.uidByte[i], HEX));
  }

  content.toUpperCase();

  display.clear();
  display.drawString(15, 0, "Normal Mode");
  if (content.length() < 10){
  display.drawString(25, 23,content);
  }
  else{
  display.drawString(5, 23,content);    
  }
  display.display();

  http.begin(rfid_server+"/normal?ID="+content);
  http.setUserAgent("ESP8266-http-Client");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int respcode = http.GET();
  Serial.println("");
  Serial.println(respcode);

  if (respcode == 200){
    response = http.getString();
    Serial.println("");
    Serial.println(response);
    http.end();
  }
  else{
    Serial.println("");
    Serial.println(respcode);
    http.end();

    display.drawString(25, 43, "Error: "+String(respcode));
    display.display();
    rfid.PICC_HaltA();
    return;
  }

  DynamicJsonBuffer jsbf;
  JsonObject& js = jsbf.parse(response);

  if (!js.success()){
    Serial.println("Faild to Pars in 177");
    display.drawString(0, 0, "Failed to Parse");
    display.display();
    return;
  }

  if (js["Auth"] == "Allow"){
    Serial.println("");
    Serial.println("Allowed");
    display.drawString(22, 43, "Autherized");
    display.display();
    buzzing(80, 50, 2);
    digitalWrite(relay, LOW);
    delay(1500);
    digitalWrite(relay, HIGH); 
  }
  else if (js["Auth"] == "NotAllow"){
    Serial.println("");
    Serial.println("Not Allowed");
    display.drawString(18, 43, "UnAutherized");
    display.display();
    buzzing(100, 50, 3);
  }
  else if (js["Auth"] == "Undefined"){
    Serial.println("");
    Serial.println("Undefined");
    display.drawString(22, 43, "Undefined");
    display.display();
    buzzing(150, 50, 3);   
  }
  rfid.PICC_HaltA();
  content = "";
}

// ---------------------------------------------------

void register_mode(){
  if (!rfid.PICC_IsNewCardPresent()){
    return;
  }
  if (!rfid.PICC_ReadCardSerial()){
    return;
  }

  content = "";
  
  for (int i=0;i<rfid.uid.size;i++){
    Serial.print(rfid.uid.uidByte[i], HEX);
    content.concat(String(rfid.uid.uidByte[i], HEX));
  }

  content.toUpperCase();
  rfid.PICC_HaltA();

  http.begin(rfid_server+"/register?ID="+content);
  http.setUserAgent("ESP8266-http-Client");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int respcode = http.GET();
  Serial.println("");
  Serial.println(respcode);

  if (respcode == 200){
    response = http.getString();
    Serial.println("");
    Serial.println(response);
    http.end();
  }
  else{
    Serial.print("Error: ");
    Serial.println(respcode);
    http.end();
    display.clear();
    display.drawString(18, 43, "Error: "+String(respcode));
    display.display();
    return;
  }

  DynamicJsonBuffer jsbf;
  JsonObject& js = jsbf.parseObject(response);

  if (js["saved"] == "false"){
    saved = false;
    Name = js["name"].as<String>();
    Auth_str = js["auth"].as<String>();
  }
  else{
    saved = true;
    Name = js["name"].as<String>();
    Auth_str = js["auth"].as<String>();
  }
  ID_sender = js["ID"].as<String>();
  buzzing(150, 50, 2);
  display.clear();
  if (content.length() < 10){
  display.drawString(25, 10, content);
  }
  else{
  display.drawString(5, 10, content);
  }
  display.drawString(35, 45, Auth_str);
  display.display();
  
}


void buzzing(int On, int off, int quantity){
  for (int i=0;i<quantity;i++){
    digitalWrite(buzzer, LOW);
    delay(On);
    digitalWrite(buzzer, HIGH);
    delay(off);
  }
}

void wifi_setup(){

    sta_ssid = server.arg("STA_SSID");
    sta_pass = server.arg("STA_PASS");
    ap_ssid = server.arg("AP_SSID");
    ap_pass = server.arg("AP_PASS");
    rfid_server = server.arg("HOST");
    wifi_mode = server.arg("MODE");

    DynamicJsonBuffer jsbf;
    JsonObject& js = jsbf.createObject();
    js["STA_SSID"] = sta_ssid;
    js["STA_PASS"] = sta_pass;
    js["AP_SSID"] = ap_ssid;
    js["AP_PASS"] = ap_pass;
    js["HOST"] = rfid_server;
    js["WiFi_Mode"] = wifi_mode;
    
    SD.remove("wifi.txt");
    myfile = SD.open("wifi.txt", FILE_WRITE);
    if (!myfile){
      Serial.println("Failed to open wifi.txt in 304");
      display.drawString(10, 20, "Opening Error");
      display.display();
      return;
    }

    js.printTo(myfile);
    myfile.close();
    server.send(200, "text/plain", "WiFi Setup Saved :)"); 
}

void network_setup(){

    IP_str = server.arg("IP_box");
    mask_str = server.arg("mask_box");
    DNS_str = server.arg("DNS_box");
    gateway_str = server.arg("gateway_box");
    DHCP_str = server.arg("DHCP_box");

    DynamicJsonBuffer jsbf;
    JsonObject& js = jsbf.createObject();

    js["IP_str"] = IP_str;
    js["mask_str"] = mask_str;
    js["DNS_str"] = DNS_str;
    js["gateway_str"] = gateway_str;
    js["DHCP_str"] = DHCP_str;
    SD.remove("net.txt");
    myfile = SD.open("net.txt", FILE_WRITE);
    if (!myfile){
      Serial.println("Failed to open net.txt in 334");
      display.drawString(10, 20, "Opening Error");
      display.display();
      return;      
    }

    js.printTo(myfile);
    myfile.close();
    server.send(200, "text/plain", "Network Setup saved :)");
}

void auth_setup(){

    user = server.arg("user_Auth");
    pass = server.arg("pass_Auth");

    DynamicJsonBuffer jsbf;
    JsonObject& js = jsbf.createObject();

    js["user"] = user;
    js["pass"] = pass;
    SD.remove("auth.txt");
    myfile = SD.open("auth.txt", FILE_WRITE);
    if (!myfile){
      Serial.println("Failed to open auth.txt in 358");
      display.drawString(10, 20, "Opening Error");
      display.display();
      return;      
    }

    js.printTo(myfile);
    myfile.close();
    server.send(200, "text/plain", "Authentication Setup saved :)");  
  
}

void commands(){

    if (server.arg("Comm") == "Reset"){

      DynamicJsonBuffer jsbf1;
      JsonObject& js1 = jsbf1.createObject();
      js1["STA_SSID"] = "";
      js1["STA_PASS"] = "";
      js1["AP_SSID"] = "RFID Attendance";
      js1["AP_PASS"] = "123456789";
      js1["HOST"] = "";
      js1["WiFi_Mode"] = "AP";
      SD.remove("wifi.txt");
      myfile = SD.open("wifi.txt", FILE_WRITE);
      if (!myfile){
      Serial.println("Failed to open wifi.txt in 385");
      display.drawString(10, 20, "Opening Error");
      display.display();
      return;  
      }

      js1.printTo(myfile);
      myfile.close();


      DynamicJsonBuffer jsbf2;
      JsonObject& js2 = jsbf2.createObject();
      js2["IP_str"] = "";
      js2["mask_str"] = "";
      js2["DNS_str"] = "";
      js2["gateway_str"] = "";
      js2["DHCP_str"] = "on";
      SD.remove("net.txt");
      myfile = SD.open("net.txt", FILE_WRITE);
      if (!myfile){
      Serial.println("Failed to open net.txt in 405");
      display.drawString(10, 20, "Opening Error");
      display.display();
      return;  
      }

      js2.printTo(myfile);
      myfile.close();


      DynamicJsonBuffer jsbf3;
      JsonObject& js3 = jsbf3.createObject();

      js3["user"] = "admin";
      js3["pass"] = "admin";
      SD.remove("auth.txt");
      myfile = SD.open("auth.txt", FILE_WRITE);
      if (!myfile){
      Serial.println("Failed to open auth.txt in 423");
      display.drawString(10, 20, "Opening Error");
      display.display();
      return;  
      }

      js3.printTo(myfile);
      myfile.close();

      server.send(200, "text/plain", "Device is in default mode and is going to be reboot :)");
      delay(4000);
      ESP.restart();
    }
    else if (server.arg("Comm") == "Reboot"){
      server.send(200, "text/plain", "Rebooting System...");
      delay(4000);
      ESP.restart();
    } 
}

void toggling(){

    if (server.arg("mode") == "normal"){
      normal = true;
      display.clear();
      display.drawString(15,0, "Normal Mode");
      display.drawString(0, 19, "Wainting for");
      display.drawString(0, 36, "RFID Card...");
      display.display();
      DynamicJsonBuffer jsbf;
      JsonObject& js = jsbf.createObject();
      js["kind"] = "TOGGLED";
      js["mode"] = "Device is in Normal mode";
      String message;
      js.printTo(message);
      server.send(200, "text/plain", message); 
    }
    else if (server.arg("mode") == "register"){
      normal = false;
      display.clear();
      display.drawString(10,0, "Register Mode");
      display.drawString(0, 19, "Wainting for");
      display.drawString(0, 36, "RFID Card...");
      display.display();
      DynamicJsonBuffer jsbf;
      JsonObject& js = jsbf.createObject();
      js["kind"] = "TOGGLED";
      js["mode"] = "Device is in Register mode";
      String message;
      js.printTo(message);
      server.send(200, "text/plain", message); 
    }
}

void requested(){
  
  String task = server.arg("mode");
  Serial.println(task);
  if (task == "save"){save_to_db();}
  else if (task == "update"){update_from_db();}
  else if (task == "delete"){delete_from_db();}
  else if (task == "req"){send_card();}
  
}

void send_card(){
  DynamicJsonBuffer jsbf;
  JsonObject& js = jsbf.createObject();

  js["kind"] = "CARD ID";
  if (saved){
    js["name"] = Name;
    js["authorizment"] = Auth_str;
  }
  else{
    js["name"] = "";
    js["authorizment"] = "";
  }
  js["cardID"] = ID_sender;
  String message = "";
  js.printTo(message);
  server.send(200, "text/plain", message);
}

void delete_from_db(){
  String id_str = server.arg("id");
  String named = server.arg("name");
  String authen = server.arg("auth");
  String url = rfid_server+"/delete?ID="+id_str;
  url.replace(" ", "");
  
  http.begin(url);
  http.setUserAgent("ESP8266-http-Client");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int respcode = http.GET();
  Serial.println(respcode);

  if (respcode == 200){
    String response = http.getString();
    Serial.println(response);
    DynamicJsonBuffer jsbf;
    JsonObject& js = jsbf.createObject();
    js["kind"] = "REQUESTED";
    js["Table"] = response;
    String sender = "";
    js.printTo(sender);
    server.send(200, "text/plain", sender);  }
  else{
    Serial.println("ERROR: ");
    Serial.println(respcode);
    display.drawString(10, 30, "Server Error!!");
    display.display();
    server.send(200, "text/plain", "Server Error!!!");
    return;
  }
}

void update_from_db(){
  String id_str = server.arg("id");
  String named = server.arg("name");
  String authen = server.arg("auth");
  String url = rfid_server+"/update?ID="+id_str+"&name="+named+"&auth="+authen;
  url.replace(" ", "");
  Serial.println(url);
  
  http.begin(url);
  http.setUserAgent("ESP8266-http-Client");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int respcode = http.GET();
  Serial.println(respcode);

  if (respcode == 200){
    String response = http.getString();
    Serial.println(response);
    DynamicJsonBuffer jsbf;
    JsonObject& js = jsbf.createObject();
    js["kind"] = "REQUESTED";
    js["Table"] = response;
    String sender = "";
    js.printTo(sender);
    server.send(200, "text/plain", sender);  }
  else{
    Serial.println("ERROR: ");
    Serial.println(respcode);
    display.drawString(10, 30, "Server Error!!");
    display.display();
    server.send(200, "text/plain", "Server Error!!!");
    return;
  }
}

void save_to_db(){
  String id_str = server.arg("id");
  String named = server.arg("name");
  String authen = server.arg("auth");
  String url = rfid_server+"/save?ID="+id_str+"&name="+named+"&auth="+authen;
  url.replace(" ", "");
  http.begin(url);
  Serial.println(url);
  http.setUserAgent("ESP8266-http-Client");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int respcode = http.GET();
  Serial.println(respcode);

  if (respcode == 200){
    String response = http.getString();
    Serial.println(response);
    DynamicJsonBuffer jsbf;
    JsonObject& js = jsbf.createObject();
    js["kind"] = "REQUESTED";
    js["Table"] = response;
    String sender = "";
    js.printTo(sender);
    server.send(200, "text/plain", sender);
  }
  else{
    Serial.println("ERROR: ");
    Serial.println(respcode);
    display.drawString(10, 30, "Server Error!!");
    display.display();
    server.send(200, "text/plain", "Server Error!!!");
    return;
  } 
}

void scan_wifi(){
    wifi_num=WiFi.scanNetworks();
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(0,0,"Scan Done !!!");
    display.display();
    delay(1000);
  if (wifi_num == 0){
    display.setFont(ArialMT_Plain_16);
    display.drawString(0,0,"No Network\nfound");
    display.display();
  }
  else{
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(0,0,String(wifi_num)+"Network\nfound");
    display.display();
    delay(1500);
  }
  for (int i=0;i < wifi_num;i++){
  Serial.print(i+1+".");
  Serial.print(WiFi.SSID(i));
  Serial.print("  Quality : (");
  long rssi=WiFi.RSSI(i);
  rssi+=100;
  Serial.print(rssi+") ");
  Serial.print("Encryption Type : (");
  byte enc=WiFi.encryptionType(i);
  String Enc_str;
  if(enc==2)Enc_str="TKIP (WPA)";
  if(enc==4)Enc_str="AES (WPA)";
  if(enc==5)Enc_str="WEP";/////////////////Translate the numbers of encryptions to Strings
  if(enc==7)Enc_str="NONE";
  if(enc==8)Enc_str="AUTO";
  Serial.println(Enc_str+")");
  }
  DynamicJsonBuffer jsbf4;
  JsonObject& js4=jsbf4.createObject();
  JsonArray& Stations=js4.createNestedArray("Stations");
  for (int i=0;i< wifi_num;i++){
  Stations.add(WiFi.SSID(i));
  }
  String StationObj="";
  js4.printTo(StationObj);
  server.send(200,"text/plain",StationObj);
}

void read_storage(){
  Serial.println("We are at reding");
  delay(2000);
  myfile=SD.open("wifi.txt",FILE_READ);
  config_str=myfile.readString();
  Serial.println(config_str);
  DynamicJsonBuffer jsbf1;
  JsonObject& js1=jsbf1.parseObject(config_str);
  if(!js1.success()){
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0,0,"Faild to\nparse wifi.txt");
    display.display();
    delay(1500);
    run_Accesspoint();
    return;
  }
  wifi_mode=js1["WiFi_Mode"].as<String>();
  sta_ssid=js1["STA_SSID"].as<String>();
  sta_pass=js1["STA_PASS"].as<String>();
  ap_ssid=js1["AP_SSID"].as<String>();
  ap_pass=js1["AP_PASS"].as<String>();
  rfid_server=js1["HOST"].as<String>();
  
  myfile.close();
  Serial.println(wifi_mode);
  Serial.println("WiFi readed");

// /////////////////////////////////////////////////////////  Parse from wifi.txt

  myfile=SD.open("net.txt",FILE_READ);
  config_str=myfile.readString();
  DynamicJsonBuffer jsbf2;
  JsonObject& js2=jsbf2.parseObject(config_str);
  if(!js2.success()){
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0,0,"Faild to\nparse net.txt");
    display.display();
    delay(1500);
    run_Accesspoint();
    return;
  }

  DHCP_str=js2["DHCP_str"].as<String>();
  IP_str=js2["IP_str"].as<String>();
  gateway_str=js2["gateway_str"].as<String>();
  mask_str=js2["mask_str"].as<String>();
  DNS_str=js2["DNS_str"].as<String>();

  myfile.close();
  Serial.println("Net readed");

// /////////////////////////////////////////////////////////  Parse from net.txt

  myfile=SD.open("auth.txt",FILE_READ);
  config_str=myfile.readString();
  DynamicJsonBuffer jsbf3;
  JsonObject& js3=jsbf3.parseObject(config_str);
  if(!js3.success()){
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0,0,"Faild to\nparse auth.txt");
    display.display();
    delay(1500);
    run_Accesspoint();
    return; 
  }

  user=js3["user"].as<String>();
  pass=js3["pass"].as<String>();

  myfile.close();
  Serial.println("Auth readed");

// /////////////////////////////////////////////////////////  Parse from auth.txt
  
  confirmity();
}

void confirmity(){
  Serial.println("Confitmity");
  Serial.println(wifi_mode);
  if (DHCP_str != "on"){
  String_to_IP(IP_str,IP);
  String_to_IP(mask_str,Subnetmask);
  String_to_IP(gateway_str,Gateway);
  String_to_IP(DNS_str,DNS);
  Serial.println("DHCP OFF");
  }
  Serial.println(wifi_mode);
  if(wifi_mode == "STA")run_station();
  else if (wifi_mode == "AP")run_Accesspoint();
  else if (wifi_mode == "multi")run_multi();
  else{
    Serial.println("Faild at 718");
    return;
    }
}

void run_station(){
  Serial.println("Stationing");
    WiFi.disconnect();
    Serial.println("Start connecting to "+sta_ssid);
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0,0,"Start\nconnecting to\n"+sta_ssid);
    display.display(); 

    if(sta_pass == ""){
      WiFi.begin(sta_ssid.c_str());
    }
    else{
      WiFi.begin(sta_ssid.c_str(),sta_pass.c_str());
    }

    if (DHCP_str != "on") WiFi.config(IP,Gateway,Subnetmask,DNS);

    int i=0;

    while(WiFi.status() != WL_CONNECTED && i++ <= 20){
      display.drawString(i*5,40,"*");
      display.display();
      delay(500);
    }

    if (i==21){
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0,0,"Faild to\nmake connection");
    display.display();
    return;
    }
    show_IPs();
}

void run_Accesspoint(){
  Serial.println("Access point");
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,0,"Starting\nAccesspoint mode");
  display.display();
  delay(1000);
  WiFi.softAPConfig(ap_ip,ap_ip,net_mask);
  if (ap_pass != NULL){
    WiFi.softAP(ap_ssid.c_str(),ap_pass.c_str());
  }
  else {
    WiFi.softAP(ap_ssid.c_str());
  }
   display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0,0,"Done");
  display.display();
  delay(1500);
}

void run_multi(){
  run_Accesspoint();
  run_station();
}

void show_IPs(){
  Serial.println("Show IPs");
  String IP_show=WiFi.localIP().toString();
  String gate_show=WiFi.gatewayIP().toString();
  String sub_show=WiFi.subnetMask().toString();
  String dns_show=WiFi.dnsIP().toString();

  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,0,"Connected to "+(WiFi.SSID()));
  display.drawString(0,10,"IP : "+IP_show);
  display.drawString(0,21,"Gateway : "+gate_show);
  display.drawString(0,32,"Submsk : "+sub_show);
  display.drawString(0,43,"DNS : "+dns_show);
  display.drawString(0,54,"DHCP"+DHCP_str);
  display.display();
  delay(5000);
}

void String_to_IP(String str,IPAddress& IP_ad){
  int in1=str.indexOf('.');
  int in2=str.indexOf('.',in1+1);
  int in3=str.indexOf('.',in2+1);
  int in4=str.length();
  IP_ad[0]=str.substring(0,in1).toInt();
  IP_ad[1]=str.substring(in1+1,in2).toInt();
  IP_ad[2]=str.substring(in2+1,in3).toInt();
  IP_ad[3]=str.substring(in3+1,in4).toInt();
}

void load_page(){
  load_from_SD(server.uri());
  String url=server.uri();
  Serial.println(url);
}

void load_from_SD(String path){
  String data_type="text/plain";
  if (path.endsWith("/")) path += "config.htm";
  if (path.endsWith(".src")) path = path.substring(0,path.lastIndexOf("."));
  else if (path.endsWith("htm")) data_type="text/html";
  else if (path.endsWith(".css")) data_type="text/css";
  else if (path.endsWith(".js")) data_type="application/javascript";
  else if (path.endsWith(".png")) data_type="image/png";
  else if (path.endsWith(".gif")) data_type="image/gif";
  else if (path.endsWith(".jpg")) data_type="image/jpeg";
  else if (path.endsWith(".ico")) data_type="image/x-ico";
  else if (path.endsWith(".xml")) data_type="text/xml";
  else if (path.endsWith(".pdf")) data_type="application/pdf";
  else if (path.endsWith(".zip")) data_type="application/zip";

  File data_file=SD.open(path.c_str(), FILE_READ);
  if (data_file.isDirectory()){
    path+="/config.htm";
    data_type="text/plain";
    data_file=SD.open(path.c_str());
  }
    if (!server.authenticate(user.c_str(),pass.c_str()))
  return server.requestAuthentication();

  server.streamFile(data_file,data_type);
  data_file.close();
}
