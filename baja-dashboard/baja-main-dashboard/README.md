# UPON CLONING FOLLOW THESE INSTRUCTIONS OR CRY LIKE WE DID

## Export ESP-IDF Environment
Every new user must run these commands after cloning the repo:

### MAC/LINUX
```
source ~/esp/v5.5.1/esp-idf/export.sh
```
^^ if above path is not found, just check where your exact installation is and adjust accordingly

### WINDOWS
```
.\export.ps1
```
If this is skiped then CMake breaks

## Delete any existing build artifacts

### MAC//LINUX
```
rm -rf build 
rm sdkconfig
```
## Reconfigure the project
```
idf.py set-target esp32   # or esp32s3, etc.
idf.py menuconfig         # optional
idf.py build
```

## Should be good to go!

