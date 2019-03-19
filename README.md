# utp_com

## Description

utp_com tool is used to send commands to hardware via Freescale's UTP protocol.

## Usage                                                                                                                                                                                                      
                                                                                                                                                                                                        
```bash                                                                                                                                                                                                       
git clone git@github.com:ixonos/utp_com.git                                                                                                                                                                   
```                                                                                                                                                                                                           

### Dependencies                                                                                                                                                                                               

```bash                                                                                                                                                                                                       
sudo apt install libsgutils2-dev                                                                                                                                                                              
```                                                                                                                                                                                                           

### Compile                                                                                                                                                                                                   
```bash                                                                                                                                                                                                       
cd utp_com                                                                                                                                                                                                    
make
```

### Run

Project directory contains flash.sh script which show how tool is used to send files and run command line commands. imx_usb ( https://github.com/boundarydevices/imx_usb_loader ) is used to flash the flashing OS.
