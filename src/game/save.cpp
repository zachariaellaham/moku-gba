// SRAM persistence: CRC32-checked SaveFile at offset 0. See save.h for the layout.
//
// Layout rules that must not change without bumping save::VERSION:
//   [0..3]  magic  [4..5] version  [6..7] size  [8..11] crc  [12..] payload
// The CRC covers everything after the crc field, so magic/version/size are validated by hand
// and a half-written payload is rejected.
#include "game/save.h"

#include <cstddef>

#include "bn_sram.h"
#include "bn_span.h"

namespace save {

namespace {

constexpr int FILE_SIZE = int(sizeof(SaveFile));
constexpr int CRC_SKIP = 12;   // magic + version + size + crc

static_assert(FILE_SIZE <= bn::sram::size(), "SaveFile does not fit in SRAM");
static_assert(CRC_SKIP == int(offsetof(SaveFile, settings)), "SaveFile header is not 12 bytes");

// Zero page used to erase SRAM in chunks (SRAM is byte-wide, so this is a plain byte loop).
constexpr uint8_t ZEROS[64] = {};

// A whole SaveFile is not a constant expression (SandboxGame::moves has no default member
// initialiser), so the defaults are rebuilt from the sub-objects that are, and only the four
// non-zero SandboxGame defaults from save.h are repeated here.
void set_defaults(SaveFile& file) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&file);

    for (int index = 0; index < FILE_SIZE; ++index) {
        bytes[index] = 0;
    }

    file.magic = MAGIC;
    file.version = VERSION;
    file.size = uint16_t(FILE_SIZE);
    file.crc = 0;
    file.settings = Settings();
    file.last_slot = 0;

    for (int index = 0; index < SLOT_COUNT; ++index) {
        file.slots[index] = Slot();
    }

    file.sandbox.size = 9;
    file.sandbox.player_black = 1;
    file.sandbox.komi_x2 = 15;
    file.sandbox.first_mover = 1;
}

}  // namespace

uint32_t crc32(const void* data, int size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFFu;

    for (int index = 0; index < size; ++index) {
        crc ^= bytes[index];

        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }

    return ~crc;
}

bool load(SaveFile& out) {
    bn::sram::read(out);

    if (out.magic == MAGIC && out.version == VERSION && out.size == FILE_SIZE) {
        const uint8_t* payload = reinterpret_cast<const uint8_t*>(&out) + CRC_SKIP;

        if (crc32(payload, FILE_SIZE - CRC_SKIP) == out.crc) {
            return true;
        }
    }

    set_defaults(out);
    return false;
}

void store(SaveFile& file) {
    file.magic = MAGIC;
    file.version = VERSION;
    file.size = uint16_t(FILE_SIZE);

    const uint8_t* payload = reinterpret_cast<const uint8_t*>(&file) + CRC_SKIP;
    file.crc = crc32(payload, FILE_SIZE - CRC_SKIP);

    bn::sram::write(file);
}

void wipe() {
    // Only the SaveFile region is erased: nothing else is ever written to SRAM, and zeroing the
    // whole 32 KB byte by byte would cost a visible frame drop.
    for (int offset = 0; offset < FILE_SIZE; offset += int(sizeof(ZEROS))) {
        int chunk = FILE_SIZE - offset;

        if (chunk > int(sizeof(ZEROS))) {
            chunk = int(sizeof(ZEROS));
        }

        bn::span<const uint8_t> span(ZEROS, chunk);
        bn::sram::write_span_offset(span, offset);
    }
}

}  // namespace save
