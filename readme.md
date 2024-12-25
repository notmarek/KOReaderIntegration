# Kindle epub extractors and more

# Technical info
  ### The appreg.db
  - registration of apps can be done in multiple ways, either pure sql or the register binary
  - deregistration has to be done manually with sql
  - the syntax for registrations is pretty simple heres an example
  ```
  { "def": "interface", "assoc": { "interface": "application" } },
  {
    "def": "handlerId",
    "assoc": {
      "handlerId": "com.github.koreader.helper",
      "props": {
        "extend-start": "Y",
        "unloadPolicy": "unloadOnPause",
        "maxGoTime": "60",
        "maxPauseTime": "60",
        "maxUnloadTime": "60",
        "searchbar-mode": "transient",
        "supportedOrientation": "U",
        "framework": "",
        "lipcId": "com.github.koreader.helper",
        "default-chrome-style": "NH",
        "command": "/mnt/us/EPUBIntegration/koreader_helper"
      }
    }
  }
  ```
  - this registers an app with id com.github.koreader.helper that can be launched with trough lipc `lipc-set-prop com.lab126.appmgrd start app://com.github.koreader.helper
  - if you need your app to run for more than `maxLoadTime` you will need to handle comms with appmgrd an example of this is `src/koreader/main.c` and i don't see a need to explain it more than that
  ```
  { "def": "interface", "assoc": { "interface": "application" } },
  { "def": "interface", "assoc": { "interface": "extractor" } },
  {
    "def": "handlerId",
    "assoc": {
      "handlerId": "com.notmarek.extractor",
      "props": {
        "lib": "/mnt/us/KOI/libKOIExtractor.so",
        "entry": "load_extractors"
      }
    }
  },
  {
    "def": "association",
    "assoc": {
      "interface": "extractor",
      "handlerId": "com.notmarek.extractor",
      "contentIds": ["GL:*.epub"],
      "default": "true"
    }
  },
  {
    "def": "mimetype",
    "assoc": {
      "mimetype": "MT:application/epub+zip",
      "extensions": ["epub"]
    }
  },
  {
    "def": "association",
    "assoc": {
      "interface": "application",
      "handlerId": "com.github.koreader.helper",
      "contentIds": ["MT:application/epub+zip"],
      "default": "true"
    }
  }
  ```
  - Another example, this does 2 things, registers an extractor library for the epub file type as well as bind our previously registered koreader helper to said epubs, clicking an .epub file launches it according to the previous installation
  - Best way to register "books" inside the collection db (cc.db) is communicating with the binary trought the scanner library
  - The needed functions are available in src/stubs/scanner/scanner.h, The json structure can be deduced from my calls to the library in `src/extractor`
  - Have fun!
