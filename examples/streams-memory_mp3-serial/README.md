# Decoding a MP3 file

In this example we decode a MP3 file into RAW output and send it to the Serial output. The audio processing chain is:
 
MemoryStream -> AudioOutputStream -> MP3Decoder -> CsvStream
