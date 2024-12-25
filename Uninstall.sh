#!/bin/sh
echo "Removing binaries from /mnt/us/KOI"
rm -rf /mnt/us/KOI
echo "Removing koreader_helper.sh from /mnt/us/koreader"
rm -f /mnt/us/koreader/koreader_helper.sh
echo "Unpatching KOReaders device.lua to exit correctly"
patch /mnt/us/koreader/device.lua --reverse < device.lua.patch
echo "Unregistering extractors"
sqlite3 /var/local/appreg.db "DELETE FROM properties WHERE handlerId = 'com.notmarek.extractor';"
sqlite3 /var/local/appreg.db "DELETE FROM associations WHERE handlerId = 'com.notmarek.extractor';"
sqlite3 /var/local/appreg.db "DELETE FROM extenstions WHERE ext = 'epub' or ext like 'fb2' or ext = 'fbz';"
sqlite3 /var/local/appreg.db "DELETE FROM mimetypes WHERE ext = 'epub' or ext like 'fb2' or ext = 'fbz';"
sqlite3 /var/local/appreg.db "DELETE FROM handlerIds WHERE handlerId = 'com.notmarek.extractor';"
echo "Unregistering KOReader integration"
sqlite3 /var/local/appreg.db "DELETE FROM properties WHERE handlerId = 'com.github.koreader.helper';"
sqlite3 /var/local/appreg.db "DELETE FROM associations WHERE handlerId = 'com.github.koreader.helper';"
sqlite3 /var/local/appreg.db "DELETE FROM handlerIds WHERE handlerId = 'com.github.koreader.helper';"
echo "Done! (ain't got no clue what will happen to your already indexed epub/fb2 files)"
