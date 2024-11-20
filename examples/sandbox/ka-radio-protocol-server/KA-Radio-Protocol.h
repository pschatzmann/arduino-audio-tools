#pragma once
#include "HttpServer.h"

void start(HttpServer *server, const char *requestPath,
           HttpRequestHandlerLine *hl) {
  Serial.println("start");
  server->replyOK();
}

void stop(HttpServer *server, const char *requestPath,
          HttpRequestHandlerLine *hl) {
  Serial.println("stop");
  server->replyOK();
}

void prev(HttpServer *server, const char *requestPath,
          HttpRequestHandlerLine *hl) {
  Serial.println("prev");
  server->replyOK();
}

void next(HttpServer *server, const char *requestPath,
          HttpRequestHandlerLine *hl) {
  Serial.println("next");
  server->replyOK();
}

void volumeUp(HttpServer *server, const char *requestPath,
           HttpRequestHandlerLine *hl) {
  Serial.println("volumeUp");
  server->replyOK();
}

void volumeDown(HttpServer *server, const char *requestPath,
             HttpRequestHandlerLine *hl) {
  Serial.println("volumeDown");
  server->replyOK();
}

void infos(HttpServer *server, const char *requestPath,
           HttpRequestHandlerLine *hl) {
  Serial.println("infos");
  //  vol: 46 :the current volume
  // num: 240 :the current station number
  // stn: Hotmix 80 :the name of the current station
  // tit: BANANARAMA - Venus :the title of the son playing
  // sts: 1 :the state of the player: 0 stopped, 1: playing.

  Str str;
  str.add("vol: 46\n\r");
  str.add("num: 0\n\r");
  str.add("stn: dummy station\n\r");
  str.add("tit: BANANARAMA - Venus\n\r");
  str.add("stat: 0\n\r");
  server->reply("text/plain", str.c_str(), 200, SUCCESS);
}

int getNumber(StrView &str) {
  int pos = str.indexOf("=");
  int result = 0;
  if (pos > 0) {
    // extract number
    StrView number = str.c_str() + pos + 1;
    result = number.toInt();
  }
  return result;
}

const char *getInstant(StrView &str) {
  int pos = str.indexOf("=");
  int result = 0;
  if (pos < 0) {
    return "";
  }
  // extract url
  StrView url = str.c_str() + pos + 1;
  url.replaceAll("\"", "");
  return url.c_str();
}

void cmdExt(HttpServer *server, const char *requestPath,
            HttpRequestHandlerLine *hl) {
  Serial.println("requestPath");
  StrView req(requestPath);
  if (req.contains("list=")) {
    Serial.print("=> list=");
    Serial.println(getNumber(req));
    server->replyOK();
    return;
  }
  if (req.contains("play=")) {
    Serial.print("=> play=");
    Serial.println(getNumber(req));
    server->replyOK();
    return;
  }
  if (req.contains("volume=")) {
    Serial.print("=> volume=");
    Serial.println(getNumber(req));
    server->replyOK();
    return;
  }
  if (req.contains("instant=")) {
    Serial.println("=> instant");
    Serial.println(getInstant(req));
    server->replyOK();
    return;
  }

  server->replyNotFound();
}
