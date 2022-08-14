#pragma once

#include "AudioTools/AudioLogger.h"

namespace audio_tools {

/**
* @brief We read a single line. A terminating 0 is added to the string to make it
* compliant for c string functions.
* @author Phil Schatzmann
* @copyright GPLv3
*/

class HttpLineReader {
   public:
       HttpLineReader(){}
       // reads up the the next CR LF - but never more then the indicated len. returns the number of characters read including crlf
       virtual int readlnInternal(Stream &client, uint8_t* str, int len, bool incl_nl=true){
           int result = 0;
           LOGD( "HttpLineReader %s","readlnInternal");
           // wait for first character
           for (int w=0;w<20 && client.available()==0; w++){
               delay(100);
           }
           // if we do not have any data we stop
           if (client.available()==0) {
               LOGW( "HttpLineReader %s","readlnInternal->no Data");
               str[0]=0;
               return 0;
           }

           // process characters until we find a new line
           bool is_buffer_owerflow = false;
           int j=0;
           while (true){
               int c = client.read();
               if (c==-1){
                   break;
               }

               if (j<len){
                   result++;
               } else {
                   is_buffer_owerflow = true;
               }

               if (c=='\n'){
                   if (incl_nl){
                       str[j]=c;
                       break;
                   } else {
                       // remove cr lf
                       if (j>=1){
                           if (str[j-1]=='\r'){
                               // remove cr
                               str[j-1]=0;
                               break;;
                           } else {
                               // remove nl
                               str[j]=0;
                               break;
                           }
                       }
                   }
               }
               if (!is_buffer_owerflow){
                   str[j] = c;
               }
               j++;
           }
           str[result-1]=0;
           if (is_buffer_owerflow){
               LOGE("Line cut off: %s", str);
           }

           return result;
       }
};

}
