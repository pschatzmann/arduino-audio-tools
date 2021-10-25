
#!/bin/bash

arduino-cli lib upgrade
arduino-cli lib linstall 
git -C .. pu
git -C ../../ESP32-A2DP pull

function compile_example {
  FILES=$1
  for f in $FILES
  do
    echo "Processing $f ..."
    # take action on each file. $f store current file name
    arduino-cli compile -b esp32:esp32:esp32 "$f"
    EC=$?
    #if [ $EC -ne 0 ]; then
      #break
      echo -e "$f -> rc=$EC" >> "build-examples-log.txt"
    #fi
  done
}

compile_example "../examples/examples-basic-api/base*"
compile_example "../examples/examples-player/player*"
compile_example "../examples/examples-webserver/str*"
compile_example "../examples/tests/test*"
compile_example "../examples/streams*"

