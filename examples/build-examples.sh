
#!/bin/bash
##
# We compile all examples using arduino-cli in order to identify compile errors 
# The return codes are made available in the build-examples-log.txt file. 
# -> rc=0: success
# -> rc=1: error
##
#arduino-cli lib upgrade
#arduino-cli lib linstall 
git -C .. pull
git -C ../../ESP32-A2DP pull
git -C ../../arduino-audio-driver pull
git -C ../../arduino-libhelix pull

# Examples that are not buildable for esp32:esp32:esp32 by design (wrong
# platform, or not an arduino-cli sketch at all) and are therefore skipped
# rather than reported as a build failure.
EXCLUDE_LIST=(
  "streams-mp34dt05-serial"   # Nano 33 BLE Sense only (needs PDM.h)
  "fft-cmsis"                 # ARM Cortex-M only (STM32/RP2040/Renesas)
  "streams-stk-desktop"       # desktop build via CMake/PortAudio, not an ESP32 sketch
  "python-post-server"        # Python script, not an Arduino sketch
)

function is_excluded {
  local name
  name=$(basename "$1")
  for ex in "${EXCLUDE_LIST[@]}"; do
    if [[ "$name" == "$ex" ]]; then
      return 0
    fi
  done
  return 1
}

function compile_example {
  ARCH=$1
  FILES=$2
  for f in $FILES
  do
    # Skip README.md files
    if [[ $(basename "$f") == "README.md" ]]; then
      echo "Skipping README.md file: $f"
      continue
    fi

    if is_excluded "$f"; then
      echo "Skipping excluded example: $f"
      continue
    fi

    # A sketch directory must contain a .ino file with the same base name.
    # If it doesn't, it's a container of sub-sketches (e.g.
    # serial/mp3/{send-mp3,receive-mp3}) - recurse into it one level instead
    # of trying to compile the container itself.
    if [[ -d "$f" && ! -f "$f/$(basename "$f").ino" ]]; then
      compile_example "$ARCH" "$f/*"
      continue
    fi

    echo "Processing $f ..."
    # take action on each file. $f store current file name
    #arduino-cli compile  -b "$ARCH"  "$f"
    arduino-cli compile  -b "$ARCH"  --build-property "build.partitions=rainmaker" --build-property "upload.maximum_size=3145728" "$f"
    EC=$?
    #if [ $EC -ne 0 ]; then
      #break
      echo -e "$f -> rc=$EC" >> "build-examples-log.txt"
    #fi
  done
}

rm build-examples-log.txt
compile_example "esp32:esp32:esp32" "../examples/examples-basic-api/base*"
compile_example "esp32:esp32:esp32" "../examples/examples-player/player*"
compile_example "esp32:esp32:esp32" "../examples/examples-stream/streams*"
compile_example "esp32:esp32:esp32" "../examples/examples-audiokit/*"
compile_example "esp32:esp32:esp32" "../examples/examples-tts/streams*"
compile_example "esp32:esp32:esp32" "../examples/examples-dsp/examples-maximilian/*"
compile_example "esp32:esp32:esp32" "../examples/examples-dsp/examples-mozzi/*"
compile_example "esp32:esp32:esp32" "../examples/examples-dsp/examples-pd/*"
compile_example "esp32:esp32:esp32" "../examples/examples-dsp/examples-stk/*"
compile_example "esp32:esp32:esp32" "../examples/examples-dsp/examples-faust/streams*"
compile_example "esp32:esp32:esp32" "../examples/examples-communication/a2dp/*"
compile_example "esp32:esp32:esp32" "../examples/examples-communication/esp-now/codec/*"
compile_example "esp32:esp32:esp32" "../examples/examples-communication/esp-now/pcm/*"
compile_example "esp32:esp32:esp32" "../examples/examples-communication/esp-now/speed-test/*"
compile_example "esp32:esp32:esp32" "../examples/examples-communication/ip/*"
compile_example "esp32:esp32:esp32" "../examples/examples-communication/udp/*"
compile_example "esp32:esp32:esp32" "../examples/examples-communication/vban/*"
compile_example "esp32:esp32:esp32" "../examples/examples-communication/rtsp/*"
compile_example "esp32:esp32:esp32" "../examples/examples-communication/serial/*"
compile_example "esp32:esp32:esp32" "../examples/examples-communication/snapcast/*"
compile_example "esp32:esp32:esp32" "../examples/examples-communication/http-client/*"
compile_example "esp32:esp32:esp32" "../examples/examples-communication/http-server/*"
compile_example "esp32:esp32:esp32" "../examples/tests/adc/*"
compile_example "esp32:esp32:esp32" "../examples/tests/basic/*"
compile_example "esp32:esp32:esp32" "../examples/tests/codecs/*"
compile_example "esp32:esp32:esp32" "../examples/tests/concurrency/*"
compile_example "esp32:esp32:esp32" "../examples/tests/conversion/*"
compile_example "esp32:esp32:esp32" "../examples/tests/effects/*"
compile_example "esp32:esp32:esp32" "../examples/tests/etc/*"
compile_example "esp32:esp32:esp32" "../examples/tests/fft/*"
compile_example "esp32:esp32:esp32" "../examples/tests/filters/*"
compile_example "esp32:esp32:esp32" "../examples/tests/performance/*"
compile_example "esp32:esp32:esp32" "../examples/tests/player/*"
compile_example "esp32:esp32:esp32" "../examples/tests/timer/*"
rm -rf /tmp/arduino

./cleanup.sh