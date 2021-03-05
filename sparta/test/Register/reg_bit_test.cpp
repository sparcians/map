
#include "RegisterBits.hpp"
#include <iostream>
#include <iomanip>

int main()
{
    // std::vector<uint8_t> some_storage_1 (1);
    // std::vector<uint8_t> some_storage_16(2);
    // std::vector<uint8_t> some_storage_32(4);

    // RegisterBits reg_1(some_storage, some_storage.size());
    // RegisterBits reg_16(some_storage, some_storage.size());
    // RegisterBits reg_32(some_storage, some_storage.size());
    std::cout.fill('0');

    std::vector<uint8_t> storage1_64(8);
    std::vector<uint8_t> storage2_64(8);
    *(reinterpret_cast<uint64_t*>(storage1_64.data())) = 0xFFFF;
    *(reinterpret_cast<uint64_t*>(storage2_64.data())) = 0xFFFF0000;
    sparta::RegisterBits reg1_64(storage1_64.data(), storage1_64.size());
    sparta::RegisterBits reg2_64(storage2_64.data(), storage2_64.size());
    sparta::RegisterBits ored64 = reg1_64 | reg2_64;
    std::cout << " 64-bit in 1: " << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage1_64.data()) << std::endl;
    std::cout << " 64-bit in 2: " << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage2_64.data()) << std::endl;
    std::cout << "|64-bit     : " << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored64[0]) << std::endl;
    std::cout << "~64-bit     : " << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>((~ored64).data()) << std::endl;

    std::vector<uint8_t> storage1_128(16);
    std::vector<uint8_t> storage2_128(16);
    *(reinterpret_cast<uint64_t*>(storage1_128.data())) = 0xFFFF;
    *(reinterpret_cast<uint64_t*>(storage2_128.data())) = 0xFFFF0000;
    *(reinterpret_cast<uint64_t*>(storage1_128.data()+8)) = 0b01010101010101010101010101010101;
    *(reinterpret_cast<uint64_t*>(storage2_128.data()+8)) = 0b10101010101010101010101010101010;
    std::cout << " 128-bit in1: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage1_128.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage1_128.data()) << std::endl;
    std::cout << " 128-bit in2: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage2_128.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage2_128.data()) << std::endl;

    sparta::RegisterBits reg1_128(storage1_128.data(), storage1_128.size());
    sparta::RegisterBits reg2_128(storage2_128.data(), storage2_128.size());
    sparta::RegisterBits ored128 = reg1_128 | reg2_128;
    std::cout << "|128-bit    : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored128.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored128.data()) << std::endl;
    sparta::RegisterBits not_ored128 = ~ored128;
    std::cout << "~128-bit    : " << std::hex
              << *reinterpret_cast<const uint64_t*>((not_ored128).data() + 8) // << "_"
              << *reinterpret_cast<const uint64_t*>((not_ored128).data()) << std::endl;

    reg1_128 |= reg2_128;
    std::cout << "|128-bit |= : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((reg1_128).data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((reg1_128).data()) << std::endl;
    std::cout << "ored128 == reg1_128 : " <<  (ored128 == reg1_128) << std::endl;

    *(reinterpret_cast<uint64_t*>(storage1_128.data()))   = 0xFFFFFFFFFFFFFFFF;
    *(reinterpret_cast<uint64_t*>(storage1_128.data()+8)) = 0x0F0F0F0F0F0F0F0F;
    *(reinterpret_cast<uint64_t*>(storage2_128.data()))   = 0x8888888888888888;
    *(reinterpret_cast<uint64_t*>(storage2_128.data()+8)) = 0xdeadbeefdeadbeef;
    sparta::RegisterBits and128 = reg1_128 & reg2_128;
    std::cout << " 128-bit in1: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage1_128.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage1_128.data()) << std::endl;
    std::cout << " 128-bit in2: "
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage2_128.data() + 8) // << "_"
              << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage2_128.data()) << std::endl;
    std::cout << "&128-bit    : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((and128).data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((and128).data()) << std::endl;

    sparta::RegisterBits shift_4_128 = and128 >> 4;
    std::cout << " 128-bit 4RS: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((shift_4_128).data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((shift_4_128).data()) << std::endl;

    sparta::RegisterBits shift_32_128 = and128 >> 32;
    std::cout << " 128-bit 32S: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((shift_32_128).data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((shift_32_128).data()) << std::endl;

    std::vector<uint8_t> storage1_256(32);
    std::vector<uint8_t> storage2_256(32);

    for(uint32_t idx = 0; idx < storage1_256.size(); ++idx) {
        storage1_256[idx] = 0x11 * (idx + 1);
        storage2_256[idx] = 0xFF - (0x11 * (idx + 1));
    }
    sparta::RegisterBits reg1_256(storage1_256.data(), storage1_256.size());
    sparta::RegisterBits reg2_256(storage2_256.data(), storage2_256.size());
    sparta::RegisterBits ored256 = reg1_256 | reg2_256;

    std::cout << " 256-bit in1: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data()) << std::endl;

    std::cout << " 256-bit in2: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg2_256.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg2_256.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg2_256.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg2_256.data()) << std::endl;

    std::cout << "|256-bit    : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored256.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored256.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored256.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored256.data()) << std::endl;
    sparta::RegisterBits and256 = reg1_256 & reg2_256;
    std::cout << "&256-bit    : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(and256.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(and256.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(and256.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(and256.data()) << std::endl;
    sparta::RegisterBits not256 = ~ored256;
    std::cout << "~|256-bit   : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(not256.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(not256.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(not256.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(not256.data()) << std::endl;
    sparta::RegisterBits notnot256 = ~not256;
    std::cout << "~~256-bit   : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(notnot256.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(notnot256.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(notnot256.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(notnot256.data()) << std::endl;

    sparta::RegisterBits shift256_R1 = reg1_256 >> 1;
    std::cout << "S256-bit  R1: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256_R1.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256_R1.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256_R1.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256_R1.data()) << std::endl;

    sparta::RegisterBits shift256R = reg1_256 >> 128;
    std::cout << "S256-bitR128: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256R.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256R.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256R.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256R.data()) << std::endl;

    sparta::RegisterBits shift256R_9 = reg1_256 >> 129;
    std::cout << "S256-bitR129: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256R_9.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256R_9.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256R_9.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256R_9.data()) << std::endl;

    sparta::RegisterBits shift256R_255 = reg1_256 >> 253;
    std::cout << "S256-bitR253: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256R_255.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256R_255.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256R_255.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256R_255.data()) << std::endl;

    std::cout << " 256-bit in1: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data()) << std::endl;

    sparta::RegisterBits shift256L_1 = reg1_256 << 1;
    std::cout << "S256-bit  L1: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L_1.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L_1.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L_1.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L_1.data()) << std::endl;

    sparta::RegisterBits shift256L = reg1_256 << 128;
    std::cout << "S256-bitL128: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L.data()) << std::endl;

    sparta::RegisterBits shift256L_9 = reg1_256 << 129;
    std::cout << "S256-bitL129: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L_9.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L_9.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L_9.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L_9.data()) << std::endl;

    sparta::RegisterBits shift256L_255 = reg1_256 << 255;
    std::cout << "S256-bitL255: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L_255.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L_255.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L_255.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256L_255.data()) << std::endl;

    reg1_256 |= reg2_256;
    std::cout << "|256-bit |= : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data() + 24) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data() + 16) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data()) << std::endl;
    std::cout << "reg1_256 == ored256: " << (reg1_256 == ored256) << std::endl;

    const uint32_t mask_size = 16;
    sparta::RegisterBits write_mask(mask_size);
    sparta::RegisterBits partial_mask(mask_size);
    partial_mask.fill(0xff);

    sparta::RegisterBits mask = ((partial_mask >> ((8*16) - (65-13+1))) << 13);
    std::cout << " write_mask : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(mask.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(mask.data()) << std::endl;

    write_mask |= mask;
    std::cout << " write_mask : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(write_mask.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(write_mask.data()) << std::endl;

    std::cout << " ~write_mask: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((~write_mask).data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((~write_mask).data()) << std::endl;

    sparta::RegisterBits reg_dead_128(16, 0xdead);
    reg_dead_128 <<= 16;
    std::cout << "dead : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg_dead_128.data() + 8) // << "_"
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg_dead_128.data()) << std::endl;

    std::array<uint64_t, 2> reg_large_data = {0xccddeeeeccddffff, 0xcccccccccccccccc};
    const sparta::RegisterBits reg_large((uint8_t*)reg_large_data.data(), 16);
    // std::array<uint8_t, 16> reg_large_data;
    // *reinterpret_cast<uint64_t *>(reg_large_data.data())     = 0xcccccccccccccccc;
    // *reinterpret_cast<uint64_t *>(reg_large_data.data() + 8) = 0xccddeeeeccddffff;
    // sparta::RegisterBits reg_large(reg_large_data.data(), 16);

    sparta::RegisterBits bits_15_00(16, 0xFFFF);
    sparta::RegisterBits bits_31_16(16, 0xFFFF0000);
    sparta::RegisterBits bits_75_60(16, 0xFFFF);
    sparta::RegisterBits bits_79_64(16, 0xFFFF);
    bits_75_60 <<= 60;
    bits_79_64 <<= 64;

    std::cout << (reg_large & bits_15_00).dataAs<uint64_t>() << std::endl;
    std::cout << ((reg_large & bits_31_16) >> 16).dataAs<uint64_t>() << std::endl;
    std::cout << ((reg_large & bits_79_64) >> 64).dataAs<uint64_t>() << std::endl;

    reg_large_data[0] = 0xccddeeeeccddffff;
    reg_large_data[1] = 0x0123456789abcdef;
    std::cout << ((reg_large & bits_75_60) >> 60).dataAs<uint64_t>() << std::endl;

}
