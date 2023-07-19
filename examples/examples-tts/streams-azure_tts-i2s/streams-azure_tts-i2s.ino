/**
 * @file streams-azure_tts-i2s.ino
 * @author Kevin Saye
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"
    
String speechKey = "....";              // deploy a Speech Service in Azure and get both the key and the region. info here: https://azure.microsoft.com/en-us/products/cognitive-services/text-to-speech/
String spechregion = "....";
String voice = "en-US-JennyNeural";     // for the next 3 settings, chose from: https://learn.microsoft.com/en-us/azure/cognitive-services/speech-service/language-support?tabs=stt#prebuilt-neural-voices
String gender = "Female";
String language = "en-US";
String url_str = "https://" + spechregion + ".tts.speech.microsoft.com/cognitiveservices/v1";
String msg = "This is a demonstration of Phil Schatzmann's AudioTools integrating with the Azure Speech Service.  Hope you like it.";

URLStream AzureURLStream("ssid", "pwd");
I2SStream i2s;                          // or I2SStream 
WAVDecoder decoder;
EncodedAudioStream out(&i2s, &decoder);   // Decoder stream
StreamCopy copier(out, AzureURLStream); // copy in to out

void setup(){
  Serial.begin(115200);  
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  config.sample_rate = 16000; 
  config.bits_per_sample = 16;
  config.channels = 1;
  config.pin_ws = GPIO_NUM_12;            //LCK
  config.pin_bck = GPIO_NUM_13;           //BCK
  config.pin_data = GPIO_NUM_21;          //DIN
  i2s.begin(config);

  // Source: https://learn.microsoft.com/en-us/azure/cognitive-services/speech-service/get-started-text-to-speech?tabs=windows%2Cterminal&pivots=programming-language-rest
  String ssml = "<speak version='1.0' xml:lang='" + language + "'><voice xml:lang='" + language + "' xml:gender='" + gender + "' name='" + voice + "'>" + msg + "</voice></speak>";
  AzureURLStream.addRequestHeader("Ocp-Apim-Subscription-Key", speechKey.c_str());
  AzureURLStream.addRequestHeader("X-Microsoft-OutputFormat", "riff-16khz-16bit-mono-pcm");   // if you change this, change the settings for i2s and the decoder
  AzureURLStream.addRequestHeader(USER_AGENT, String("Arduino with Audiotools version:" + String(AUDIOTOOLS_VERSION)).c_str());

  AzureURLStream.begin(url_str.c_str(), "audio/wav", POST, "application/ssml+xml",  ssml.c_str());  
}

void loop(){
  copier.copy();
}
