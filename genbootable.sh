#!/bin/bash

cd $1
mkdir -p TMP/EFI/BOOT
cd TMP
cp ../$2 EFI/BOOT/BOOTX64.EFI

mkdir IMG
mkdir IMG_MNT
cd IMG
dd if=/dev/zero of=efiboot.img bs=1k count=2048
mkfs.vfat efiboot.img

sudo mount -o loop efiboot.img ../IMG_MNT
sudo cp -r ../EFI ../IMG_MNT
sudo umount ../IMG_MNT

xorriso -as mkisofs \
        -R -J \
        -V $3 \
        -no-emul-boot \
        -append_partition 2 0xef efiboot.img \
        -eltorito-alt-boot \
        -e --interval:appended_partition_2:all:: \
        -no-emul-boot \
        -isohybrid-gpt-basdat \
        -o $2.iso \
        .
mv $2.iso ../../$2.iso
cd ../../
sudo rm -rf TMP






