#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Protocol constants
#define PRECIR_MAX_FRAME_SIZE 64

// Structure to hold a Pricer Label ID
typedef struct {
    uint8_t id[4];
} PrecIrLabelId;

/**
 * @brief Parse a barcode string into a Label ID
 * 
 * @param barcode Not null, expected format "xxx..." (17 chars in original, need verification)
 * @param out_plid Output struct
 * @return true if successful
 */
bool precir_parse_plid(const char* barcode, PrecIrLabelId* out_plid);

/**
 * @brief Create a "Ping" frame (Wake up / Identify?)
 * 
 * @param buffer Output buffer (must be at least PRECIR_MAX_FRAME_SIZE)
 * @param length Output length of the frame
 * @param plid Target Label ID
 * @param pp16 Use PP16 protocol (true) or PP4 (false)
 * @param repeats Number of repeats (encoded in frame)
 */
void precir_create_ping_frame(uint8_t* buffer, size_t* length, const PrecIrLabelId* plid, bool pp16, uint16_t repeats);

/**
 * @brief Create a "Refresh" frame (Update content?)
 * 
 * @param buffer Output buffer
 * @param length Output length
 * @param plid Target Label ID
 * @param pp16 Use PP16 protocol
 */
void precir_create_refresh_frame(uint8_t* buffer, size_t* length, const PrecIrLabelId* plid, bool pp16);
void precir_create_page_change_frame(uint8_t* buffer, size_t* length, const PrecIrLabelId* plid, bool pp16);
