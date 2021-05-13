#Decoding a WAV file

In this example we decode a WAV file into RAW output and send it to the Serial output. The audio chain is:

MemoryStream -> AudioOutputStream -> WAVDecoder -> CsvStream
