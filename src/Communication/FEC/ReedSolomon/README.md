# Reed-Solomon
Reed Solomon BCH encoder and decoder library

## Overview

This RS implementation was designed for embedded purposes, so all the memory allocations performed on the stack.<br>
If somebody wants to reimplement memory management with heap usage, pull requests are welcome

## Getting the source

If you want only Reed-Solomon code, just clone repository.<br>
If you want to get tests and examples also, do 
```
git clone --recursive git@github.com:mersinvald/Reed-Solomon.git
```

## Build

There is no need in building RS library, cause all the implementation is in headers.<br>
To build tests and examples simply run <b>make</b> in the folder with cloned repo and executables will emerge in the 
./build folder

## Usage

All the Reed-Solomon code is in folder **include**, you just need to include header <b>rs.hpp</b>

Template class ReedSolomon accepts two template arguments: message length and ecc length. <br>
Simple example: <br>
```
    char message[] = "Some very important message ought to be delivered";
    const int msglen = sizeof(message);
    const int ecclen = 8;
    
    char repaired[msglen];
    char encoded[msglen + ecclen];


    RS::ReedSolomon<msglen, ecclen> rs;

    rs.Encode(message, encoded);

    // Corrupting first 8 bytes of message (any 4 bytes can be repaired, no more)
    for(uint i = 0; i < ecclen / 2; i++) {
        encoded[i] = 'E';
    }

    rs.Decode(encoded, repaired);

    std::cout << "Original:  " << message  << std::endl;
    std::cout << "Corrupted: " << encoded  << std::endl;
    std::cout << "Repaired:  " << repaired << std::endl;

    std::cout << ((memcmp(message, repaired, msglen) == 0) ? "SUCCESS" : "FAILURE") << std::endl;
```

## Regards

Huge thanks to authors of [wikiversity page about Reed-Solomon BCH](https://en.wikiversity.org/wiki/Reed–Solomon_codes_for_coders)
## Related projects

[Arduino Reed-Solomon Forward Error Correction library](https://github.com/simonyipeter/Arduino-FEC)
