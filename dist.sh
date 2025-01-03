mkdir -p dist/documents/koi_installer
cp -r koreader_helper libKOIExtractor.so koreader_helper.sh device.lua.patch EpubExtractor.install KoreaderIntegration.install dist/documents/koi_installer
cp -r Install.sh dist
tar -czf dist.tar.gz dist
rm -rf dist
