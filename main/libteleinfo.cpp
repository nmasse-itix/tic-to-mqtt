#include "libteleinfo.h"
#include "libteleinfo/src/LibTeleinfo.h"

static TInfo tinfo;
static libteleinfo_data_callback data_cb;
static libteleinfo_adps_callback adps_cb;

void _libteleinfo_data_callback(ValueList * valueslist, uint8_t flags) {
    data_cb(valueslist->ts, flags, valueslist->name, valueslist->value);
}

void _libteleinfo_adps_callback(uint8_t phase) {
    adps_cb(phase);
}

EXTERNC void libteleinfo_init(libteleinfo_data_callback dcb, libteleinfo_adps_callback acb) {
    data_cb = dcb;
    adps_cb = acb;

    _Mode_e ltmode = TINFO_MODE_HISTORIQUE;
    if (CONFIG_TIC_MODE) {
        ltmode = TINFO_MODE_STANDARD;
    }

    // Initialize the LibTeleinfo
    tinfo.init(ltmode);
    tinfo.attachData(_libteleinfo_data_callback);
    tinfo.attachADPS(_libteleinfo_adps_callback);
}

EXTERNC void libteleinfo_process(uint8_t* buffer, int len) {
    for (int i = 0; i < len; i++) {
        tinfo.process(buffer[i]);
    }
}
