#!/bin/sh
mkdir -p /mnt/us/EPUBIntegration

echo "Moving binaries to /mnt/us/EPUBIntegration"
if [ -f /lib/ld-linux-armhf.so.3 ]; then
    echo "Detected firmware above 5.16.2.1.1, using hardfp binaries"
    mv koreader_helper /mnt/us/EPUBIntegration
    mv libepubExtractor.so /mnt/us/EPUBIntegration
else
    echo "Detected firmware <= 5.16.2.1.1, using softfp binaries"
    mv koreader_helper_softfp /mnt/us/EPUBIntegration/koreader_helper
    mv libepubExtractor_softfp.so /mnt/us/EPUBIntegration/libepubExtractor.so
fi

echo "Moving koreader_helper.sh to /mnt/us/koreader"
mv koreader_helper.sh /mnt/us/koreader
echo "Patching KOReaders device.lua to exit correctly"
patch /mnt/us/koreader/device.lua < device.lua.patch
echo "Registering EPUB extractor"
/usr/bin/register EpubExtractor.install
echo "Registering KOReader integration"
/usr/bin/register KoreaderIntegration.install
echo "Done! Triggering a full scan of the library"
lipc-set-prop com.lab126.scanner doFullScan 1
