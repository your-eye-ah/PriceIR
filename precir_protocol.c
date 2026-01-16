#include "precir_protocol.h"
#include <string.h>
#include <stdlib.h>

// CRC16-CCITT (0x8408 poly reversed? Original used 0x8408)
// From pr.py:
// result = 0x8408
// poly = 0x8408
// ...
static uint16_t precir_crc16(const uint8_t* data, size_t len) {
    uint16_t result = 0x8408;
    uint16_t poly = 0x8408;

    for(size_t i = 0; i < len; i++) {
        result ^= data[i];
        for(int bi = 0; bi < 8; bi++) {
            if(result & 1) {
                result >>= 1;
                result ^= poly;
            } else {
                result >>= 1;
            }
        }
    }
    return result;
}

static void precir_terminate_frame(uint8_t* buffer, size_t* len, bool pp16, uint16_t repeats) {
    // Original: terminate_frame(frame, pp16, repeats)
    // 1. Calculate CRC on current content
    uint16_t crc = precir_crc16(buffer, *len);
    
    // 2. If PP16, prepend header [0x00, 0x00, 0x00, 0x40]
    // In C, prepending is expensive (memmove). 
    // Optimization: The caller should reserve space or we handle it by constructing in order.
    // For now, let's assume the callers of this private function know what they are doing.
    // BUT, the original python code prepended it *inside* terminate_frame. 
    // Let's handle it by shifting if needed, but it's better if `make_ping` handles the offset.
    
    if(pp16) {
        // Shift data by 4 bytes
        memmove(buffer + 4, buffer, *len);
        buffer[0] = 0x00;
        buffer[1] = 0x00;
        buffer[2] = 0x00;
        buffer[3] = 0x40;
        *len += 4;
        
        // Re-calculate CRC? 
        // Original python: `crc = crc16(frame)` (before prepend) -> params passed by ref?
        // Python: `crc = crc16(frame)` then `if pp16: frame[0:0] = ...`
        // So CRC is calculated on the raw payload WITHOUT the PP16 header.
    }

    // Append CRC
    buffer[(*len)++] = crc & 0xFF;
    buffer[(*len)++] = (crc >> 8) & 0xFF;

    // Append Repeats (used by transmitter)
    // Python: frame.append(repeats & 255); frame.append((repeats // 256) & 255)
    buffer[(*len)++] = repeats & 0xFF;
    buffer[(*len)++] = (repeats >> 8) & 0xFF;
}

bool precir_parse_plid(const char* barcode, PrecIrLabelId* out_plid) {
    // Barcode format logic from pr.py:
    // id_value = int(barcode[2:7]) + (int(barcode[7:12]) << 16)
    // Expected len 17? "PLID[0] = ..."
    
    if (!barcode || strlen(barcode) < 12) return false; 
    // TODO: Verify exact barcode format. Using simple parsing for now based on python slice.
    
    char part1[6] = {0};
    char part2[6] = {0};
    
    // Safety check needed for string operations
    if(strlen(barcode) >= 12) {
        strncpy(part1, barcode + 2, 5); // chars 2-6
        strncpy(part2, barcode + 7, 5); // chars 7-11
    } else {
        return false;
    }

    uint32_t val1 = atoi(part1);
    uint32_t val2 = atoi(part2);
    // User requested D9 2E 49 2A (Val2 then Val1).
    // This implies Val2 (11993) is the Low Word (LSB side).
    uint32_t id_value = val2 + (val1 << 16);

    out_plid->id[3] = (id_value >> 24) & 0xFF;
    out_plid->id[2] = (id_value >> 16) & 0xFF;
    out_plid->id[1] = (id_value >> 8) & 0xFF;
    out_plid->id[0] = id_value & 0xFF;
    
    return true;
}

void precir_create_ping_frame(uint8_t* buffer, size_t* length, const PrecIrLabelId* plid, bool pp16, uint16_t repeats) {
    *length = 0;
    
    // make_raw_frame(0x85, PLID, 0x17)
    // frame = [protocol, PLID[0], PLID[1], PLID[2], PLID[3], cmd]
    buffer[(*length)++] = 0x85;
    buffer[(*length)++] = plid->id[0];
    buffer[(*length)++] = plid->id[1];
    buffer[(*length)++] = plid->id[2];
    buffer[(*length)++] = plid->id[3];
    buffer[(*length)++] = 0x17; // 0x97? python says 0x17

    // Append 0x01, 0x00, 0x00, 0x00
    buffer[(*length)++] = 0x01;
    buffer[(*length)++] = 0x00;
    buffer[(*length)++] = 0x00;
    buffer[(*length)++] = 0x00;

    // Append 22 bytes of 0x00
    for(int i=0; i<22; i++) {
        buffer[(*length)++] = 0x00;
    }

    precir_terminate_frame(buffer, length, pp16, repeats);
}

void precir_create_refresh_frame(uint8_t* buffer, size_t* length, const PrecIrLabelId* plid, bool pp16) {
    *length = 0;

    // make_mcu_frame(PLID, 0x01)
    // frame = [0x85, PLID[0], PLID[1], PLID[2], PLID[3], 0x34, 0x00, 0x00, 0x00, cmd]
    buffer[(*length)++] = 0x85;
    buffer[(*length)++] = plid->id[0];
    buffer[(*length)++] = plid->id[1];
    buffer[(*length)++] = plid->id[2];
    buffer[(*length)++] = plid->id[3];
    buffer[(*length)++] = 0x34;
    buffer[(*length)++] = 0x00;
    buffer[(*length)++] = 0x00;
    buffer[(*length)++] = 0x00;
    buffer[(*length)++] = 0x01; // cmd

    // Append 22 bytes of 0x00
    for(int i=0; i<22; i++) {
        buffer[(*length)++] = 0x00;
    }

    precir_terminate_frame(buffer, length, pp16, 1);
}
