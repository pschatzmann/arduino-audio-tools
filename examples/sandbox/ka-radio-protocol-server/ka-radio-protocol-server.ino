
/**
 * A simple web server that handles the following commands that are
 * supported by ka-radio
 *
    version : return 'Release: x.x, Revision: x'
    infos : return
        vol: 46 :the current volume
        num: 240 :the current station number
        stn: Hotmix 80 :the name of the current station
        tit: BANANARAMA - Venus :the title of the son playing
        sts: 1 :the state of the player: 0 stopped, 1: playing.
    list=xxx : return the name of the station xxx (0 to 254)
    stop : stop playing the current station.
    start : start playing the current station.
    next : play the next station
    prev : play the previous station
    play=xxx : with xxx from 0 to 254 play the station xxx
    volume=xxx : with xxx from 0 to 254 change the volume to xxx
    volume+ : increment the volume by 5
    volume- : decrement the volume by -5
    uart : uart baudrate at 115200 not saved on next reset
    instant="http://your url" : instant play a site
    Example:
    http://192.168.1.253/?instant=http://api.voicerss.org/?f=32khz_16bit_stereo&key=29334415954d491b85535df4eb4dd821&hl=en-us&src=hello?the-sun-is-yellow?and-i-like-it"
    the url must begin with http:// and be surrounded by " and no space char
    allowed. Volume may be combined with other command.
 */
#include "HttpServer.h"
#include "KA-Radio-Protocol.h"
#include <ESPmDNS.h>

const char *ssid = "SSID";
const char *password = "PASSWORD";
WiFiServer wifi;
HttpServer server(wifi);

void setup() {
  Serial.begin(115200);
  HttpLogger.begin(Serial, Info);

  server.on("/start", T_GET, start);
  server.on("/stop", T_GET, stop);
  server.on("/prev", T_GET, prev);
  server.on("/next", T_GET, next);
  server.on("/volume-", T_GET, volumeDown);
  server.on("/volume+", T_GET, volumeUp);
  server.on("/infos", T_GET, infos);
  // list, play, volume, instant
  server.on("/", T_GET, cmdExt);

  // connect to WIFI
  server.begin(80, ssid, password);

  // announce karadio on the network
  if (!MDNS.begin("karadio")) {
    Serial.println("Error setting up MDNS responder!");
  }
}

void loop() { server.doLoop(); }