# Using Mozzi with UDP

I am providing a simple integration for [Mozzi](https://sensorium.github.io/Mozzi/).  

We can send the output via TCP/IP or UDP. In this example I am using UDP:

....

  UDPStream out;                                            // UDP Output - A2DPStream is a singleton!

  // connect to WIFI
  WiFi.begin("wifi", "password");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500); 
  }

  // We send the sound via UDP
  IPAddress ip(192, 168, 1, 255);
  out.begin(ip, 9999);
....



On the other side we can use netcat to play the sound
....
nc -u 192.168.0.255 9999|play –c 1 –b 16 –e signed –t raw –r 44k -
....

