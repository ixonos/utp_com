#!/bin/bash

DEVICE=

IMX_USB_PATH=/home/piirote/bin
UTP_COM_PATH=/home/piirote/utp_com/

FLASH_IMAGE_DIR=/home/piirote/dfi_bin
MKSDCARD_DIR=/home/piirote/dfi_bin

# Flash flashing os
cd $IMX_USB_PATH
IMX_USB_PRINT=`./imx_usb 2>&1`

if `echo "$IMX_USB_PRINT" | grep -q "Could not open device"`; then
	echo "imx_usb returned error: Could not open device"
	exit 1
fi

if `echo "$IMX_USB_PRINT" | grep -q "err=-"`; then
	echo "imx_usb returned error:"
	echo $IMX_USB_PRINT
	exit 1
fi

# Find the correct device
for i in {1..30}
do
	DEVICE=`ls -l /dev/sd* | grep "8,\s*16" | sed s/[^\/]*\//`

	if [ -n "$DEVICE" ]; then
		break
	fi

	sleep 1
done

if [ ! -e $DEVICE ]; then
	echo "Device $DEVICE not found" 1>&2
	exit 1
fi

# Flash erase
cd $UTP_COM_PATH
./utp_com -d $DEVICE -c "$ flash_erase /dev/mtd0 0 0"

# Uboot
./utp_com -d $DEVICE -c "send" -f ${FLASH_IMAGE_DIR}/u-boot.bin
./utp_com -d $DEVICE -c "$ dd if=\$FILE of=/dev/mtd0 bs=512"

# Kernel
./utp_com -d $DEVICE -c "send" -f ${FLASH_IMAGE_DIR}/uImage
./utp_com -d $DEVICE -c "$ dd if=\$FILE of=/dev/mmcblk0 bs=1M seek=1 conv=fsync"

# Mksdcard
./utp_com -d $DEVICE -c "send" -f ${MKSDCARD_DIR}/mksdcard.sh.tar
./utp_com -d $DEVICE -c "$ tar xf \$FILE "
./utp_com -d $DEVICE -c "$ sh mksdcard.sh /dev/mmcblk0"

# Format rootfs
./utp_com -d $DEVICE -c "$ mkfs.ext4 -j /dev/mmcblk0p1"
./utp_com -d $DEVICE -c "$ mkdir -p /mnt/mmcblk0p1"
./utp_com -d $DEVICE -c "$ mount -t ext4 /dev/mmcblk0p1 /mnt/mmcblk0p1"

# Rootfs
./utp_com -d $DEVICE -c "pipe tar -jxv -C /mnt/mmcblk0p1 >/dev/null" -f ${FLASH_IMAGE_DIR}/rootfs.tar.bz2
./utp_com -d $DEVICE -c "frf"
./utp_com -d $DEVICE -c "$ umount /mnt/mmcblk0p1"

# Done
./utp_com -d $DEVICE -c "$ echo Update ready"
