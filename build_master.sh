#!/bin/bash

if [[ "${1}" != "skip" ]] ; then
	git clean -fdx
	./build_kernel.sh "$@" || exit 1
fi

VERSION="$(cat version)-$(date +%F | sed s@-@@g)"

if [ -e boot.img ] ; then
	rm bz-kernel-$VERSION.zip 2>/dev/null
	cp boot.img bz-kernel-$VERSION.img

	# Pack AnyKernel3
	rm -rf kernelzip
	mkdir kernelzip
	echo "
kernel.string=bz kernel $(cat version) @ xda-developers
do.devicecheck=0
do.modules=0
do.cleanup=1
do.cleanuponabort=0
block=/dev/block/bootdevice/by-name/boot
is_slot_device=1
ramdisk_compression=gzip
" > kernelzip/props
	cp -rp /home/piyush/OP9P/AnyKernel3/* kernelzip/
	find . -name '*.dtb' -exec cat {} + > kernelzip/dtb
	cp arch/arm64/boot/dtbo.img kernelzip/
	touch kernelzip/vendor_boot
	cd kernelzip/
	7z a -mx9 bz-kernel-$VERSION-tmp.zip *
	7z a -mx0 bz-kernel-$VERSION-tmp.zip ../arch/arm64/boot/Image.gz
	zipalign -v 4 bz-kernel-$VERSION-tmp.zip ../bz-kernel-$VERSION.zip
	rm bz-kernel-$VERSION-tmp.zip
	cd ..
	ls -al bz-kernel-$VERSION.zip
fi
