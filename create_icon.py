import zlib
import struct

def make_png(width, height):
    # minimal png header
    # Signature
    data = b'\x89PNG\r\n\x1a\n'
    # IHDR
    ihdr = struct.pack('>IIBBBBB', width, height, 1, 0, 0, 0, 0)
    crc = zlib.crc32(b'IHDR' + ihdr)
    data += struct.pack('>I', len(ihdr)) + b'IHDR' + ihdr + struct.pack('>I', crc)
    
    # IDAT (1 bit depth, raw data)
    # 10 pixels wide = 2 bytes per row (padded? No, packed)
    # scanline 0 + ceil(w/8) bytes
    # 10 pixels -> 2 bytes. 00000000 00xxxxxx
    raw = b''
    for i in range(height):
        raw += b'\x00\xFF\xC0' # filter 0, 11111111 11000000 (all white)
        
    compressed = zlib.compress(raw)
    crc = zlib.crc32(b'IDAT' + compressed)
    data += struct.pack('>I', len(compressed)) + b'IDAT' + compressed + struct.pack('>I', crc)
    
    # IEND
    data += struct.pack('>I', 0) + b'IEND' + struct.pack('>I', zlib.crc32(b'IEND'))
    return data

with open('icon.png', 'wb') as f:
    f.write(make_png(10, 10))
    
print("icon.png generated")
