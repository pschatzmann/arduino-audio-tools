#pragma once

#include "AudioBasic/StrExt.h"
#include "AudioTools/AudioLogger.h"

namespace audio_tools {


/**
 * @brief URL parser which breaks a full url string up into its individual parts
 * 
 * http://pschatzmann.ch:80/path1/path2
 * -> protocol: http
 * -> host: pschatzmann.ch
 * -> port: 80
 * -> url: http://pschatzmann.ch:80/path1/path2
 * -> root: http://pschatzmann.ch:80
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3

 */
class Url {
    public:
        // empty url
        Url() {
            LOGD("Url");
        }
        
        ~Url() {
            LOGD("~Url");
            pathStr.clear();
            hostStr.clear();
            protocolStr.clear();
            urlRootStr.clear();
            urlStr.clear();
        }

        // setup url with string
        Url(const char *url){
            LOGD("Url %s",url);
            setUrl(url);
        }

        // copy constructor
        Url(Url &url){
            LOGD("Url %s",url.url());
            setUrl(url.url());
        }

        const char* url() {return urlStr.c_str();}
        const char* path() { return pathStr.c_str(); }
        const char* host() { return hostStr.c_str();}
        const char* protocol() {return protocolStr.c_str();}
        const char* urlRoot() {return urlRootStr.c_str();} // prefix w/o path -> https://host:port
        int port() {return portInt;}
        bool isSecure() { return portInt==443; }

        void setUrl(const char* url){
            LOGD("setUrl %s",url);
            this->urlStr = url;
            parse();
        }

    protected:
        StrExt pathStr = StrExt(40);
        StrExt hostStr = StrExt(20);
        StrExt protocolStr = StrExt(6);
        StrExt urlRootStr = StrExt(40);
        StrExt urlStr = StrExt(40);
        int portInt;

        void parse() {
            LOGI("Url::parse");
            
            int protocolEnd = urlStr.indexOf("://");
            if (protocolEnd==-1){
                return;
            }
            protocolStr.substring(urlStr, 0,protocolEnd);
            int portStart = urlStr.indexOf(":",protocolEnd+3);            
            int hostEnd = portStart!=-1 ? portStart : urlStr.indexOf("/",protocolEnd+4);
            // we have not path -> so then host end is the end of string
            if (hostEnd==-1){
                hostEnd = urlStr.length();                
            }
            hostStr.substring(urlStr, protocolEnd+3,hostEnd);
            if (portStart>0){
                portInt = atoi(urlStr.c_str()+portStart+1);
            } else {
                if (protocolStr.startsWith("https"))
                    portInt = 443;
                else if (protocolStr.startsWith("http"))
                    portInt = 80;
                else if (protocolStr.startsWith("ftp"))
                    portInt = 21;
                else 
                    portInt = -1; // undefined
            }
            int pathStart = urlStr.indexOf("/",protocolEnd+4);
            if (pathStart==0){
                // we have no path
                pathStr = "/";
                urlRootStr = urlStr;
            } else {
                pathStr.substring(urlStr, pathStart, urlStr.length());
                pathStr.trim();
                urlRootStr.substring(urlStr, 0, pathStart);
            }
            LOGI("url->%s",url());
            LOGI("host->%s",host());
            LOGI("protocol->%s",protocol());
            LOGI("path->%s",path());
            LOGI("port->%d",port());
           
        }

};

}

