/*
 * Controll mains electricity device by simple text UDP messages
 *
 * by Roman Hoog Antink (c) 2014
 *
 * This code is in the public domain.
 */

#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008

#define DYNAMIC_IP 1             // set to 0 for static IP configuration; 1 for configuration by DHCP

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {  
  0x4E, 0x02, 0xA8, 0x1F, 0x00, 0x77 };

#if DYNAMIC_IP
EthernetClient dhcpclient;
#else
IPAddress ip(192, 168, 4, 77);
#endif

unsigned int localPort = 80;      // local port to listen on

#define OUT_PIN 9
#define DEFAULT_TIMEOUT 30000    // default timeout in ms

unsigned long timeout = DEFAULT_TIMEOUT;
unsigned long timer_start = 0;

// command function matrix
typedef struct s_cmd_matrix {
  const char *cmd;
  const char *(*process_message)(const char *message);
} cmd_matrix;

cmd_matrix commands[] = {
   { "help", &cmd_help },
   { "on",   &cmd_on },
   { "off",  &cmd_off },
   { "timeout", &cmd_update_timeout },
   { "", 0 } 
};

const char *help_text = "supported commands:\n"
  "timeout [N]  get or set timeout in seconds\n" 
  "on [N]       activate relay with optional timeout\n"
  "             (the first argument will update the default timeout setting)\n"
  "off          switch relay off\n";

// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet
char replyBuffer[UDP_TX_PACKET_MAX_SIZE];
char successReplyBuffer[] = "ok\n";
char failReplyBuffer[] = "?\n";

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup() {
  pinMode(OUT_PIN, OUTPUT);
  digitalWrite(OUT_PIN, LOW);
    
  // start the Ethernet and UDP:
#if DYNAMIC_IP
  Ethernet.begin(mac);
#else
  Ethernet.begin(mac,ip);
#endif
  Udp.begin(localPort);

  Serial.begin(9600);
}

void loop() {
  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if(packetSize)
  {
    // read the packet into packetBufffer
    Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);
    const char *response = processMessage(packetBuffer);

    // send a reply, to the IP address and port that sent us the packet we received
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(response);
    Udp.endPacket();
  }
  
  checkTimeout();
  
  delay(10);
}

void checkTimeout() {
  if(timer_start && timeout && timer_start + timeout < millis()) {
    timer_start = 0;
    digitalWrite(OUT_PIN, LOW);
    Serial.println("timeout reached\n");
  }
}

const char *processMessage(const char *msg) {
   Serial.write("received: ");
   Serial.write(msg);
   for(cmd_matrix *p=commands; p->process_message; p++) {
      if(!strncmp(msg, p->cmd, strlen(p->cmd))) {
         return (p->process_message(msg));
      } 
   }
   return failReplyBuffer;
}

const char *cmd_help(const char *msg) {
  return help_text;
}


// supports optional argument for timeout in seconds
const char *cmd_on(const char *msg) {
  digitalWrite(OUT_PIN, HIGH);
  timer_start = millis(); // begin timeout
  
  if(msg[2] == ' ' && !set_timeout(msg)) {
    return failReplyBuffer;
  }
  
  return successReplyBuffer;
}

const char *cmd_off(const char *msg) {
  digitalWrite(OUT_PIN, LOW);
  timer_start = 0; // abort timeout
  return successReplyBuffer;
}

// msg format: "timeout 1234", where 1234 is the timeout in seconds
const char *cmd_update_timeout(const char *msg) {
  Serial.write(msg);
  
  if(msg[7] != ' ') {
    String t = String(timeout/1000);
    t.toCharArray(replyBuffer, UDP_TX_PACKET_MAX_SIZE);
    strcat(replyBuffer, "\n");
    return replyBuffer;
  }
  
  // process argument
  if(!set_timeout(msg)) {
    return failReplyBuffer;
  }
  
  return successReplyBuffer; 
}

boolean set_timeout(const char *msg) {
  // skip first word
  const char *p = msg;
  while(*p != ' ' && *p) { p++; };
  if(!*p || p[1] < '0' || p[1] > '9') {
    Serial.print("set_timeout failed: ");
    Serial.println(msg);
    return false;
  }
  p++; // eat whitespace
  
  timeout = atoi(p) * 1000;
  Serial.print("timeout updated to ");
  Serial.print(timeout);
  Serial.println(" seconds");
  return true;  
}
