#include "scanner.h"


int epub_extractor(const struct scanner_event* event) {
    if (event->event_type > 2) return 0; // ignore unknown event types
    return 0;
}

[[gnu::visibility("default")]]
void load_epub_extractor(ScannerEventHandler** handler, int* unk1) {
    *handler = epub_extractor;
    *unk1 = 0;
}
