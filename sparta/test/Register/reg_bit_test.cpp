
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
    std::cout << " 128-bit in1: " << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage1_128.data() + 8)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage1_128.data()) << std::endl;
    std::cout << " 128-bit in2: " << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage2_128.data() + 8)
              << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage2_128.data()) << std::endl;

    sparta::RegisterBits reg1_128(storage1_128.data(), storage1_128.size());
    sparta::RegisterBits reg2_128(storage2_128.data(), storage2_128.size());
    sparta::RegisterBits ored128 = reg1_128 | reg2_128;

    std::cout << "|128-bit    : " << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored128.data() + 8)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored128.data()) << std::endl;

    sparta::RegisterBits not_ored128 = ~ored128;
    std::cout << "~128-bit    : " << std::hex << *reinterpret_cast<const uint64_t*>((not_ored128).data() + 8)
              << *reinterpret_cast<const uint64_t*>((not_ored128).data()) << std::endl;

    *(reinterpret_cast<uint64_t*>(storage1_128.data()))   = 0xFFFFFFFFFFFFFFFF;
    *(reinterpret_cast<uint64_t*>(storage1_128.data()+8)) = 0x0F0F0F0F0F0F0F0F;
    *(reinterpret_cast<uint64_t*>(storage2_128.data()))   = 0x8888888888888888;
    *(reinterpret_cast<uint64_t*>(storage2_128.data()+8)) = 0xdeadbeefdeadbeef;
    sparta::RegisterBits and128 = reg1_128 & reg2_128;
    std::cout << " 128-bit in1: " << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage1_128.data() + 8)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage1_128.data()) << std::endl;
    std::cout << " 128-bit in2: " << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage2_128.data() + 8)
              << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>(storage2_128.data()) << std::endl;
    std::cout << "&128-bit    : " << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>((and128).data() + 8)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((and128).data()) << std::endl;

    sparta::RegisterBits shift_4_128 = and128 >> 4;
    std::cout << " 128-bit 4RS: " << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>((shift_4_128).data() + 8)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((shift_4_128).data()) << std::endl;

    sparta::RegisterBits shift_32_128 = and128 >> 32;
    std::cout << " 128-bit 32S: " << std::hex << std::setw(16) << *reinterpret_cast<const uint64_t*>((shift_32_128).data() + 8)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>((shift_32_128).data()) << std::endl;

    std::vector<uint8_t> storage1_256(32);
    std::vector<uint8_t> storage2_256(32);

    for(uint32_t idx = 0; idx < storage1_256.size(); ++idx) {
        storage1_256[idx] = 0xF0;
        storage2_256[idx] = 0x0F;
    }
    sparta::RegisterBits reg1_256(storage1_256.data(), storage1_256.size());
    sparta::RegisterBits reg2_256(storage2_256.data(), storage2_256.size());
    sparta::RegisterBits ored256 = reg1_256 | reg2_256;
    std::cout << "|256-bit    : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored256.data() + 24)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored256.data() + 16)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored256.data() + 8)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(ored256.data()) << std::endl;
    sparta::RegisterBits and256 = reg1_256 & reg2_256;
    std::cout << "&256-bit    : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(and256.data() + 24)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(and256.data() + 16)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(and256.data() + 8)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(and256.data()) << std::endl;
    sparta::RegisterBits not256 = ~ored256;
    std::cout << "~256-bit    : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(not256.data() + 24)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(not256.data() + 16)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(not256.data() + 8)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(not256.data()) << std::endl;
    sparta::RegisterBits notnot256 = ~not256;
    std::cout << "~~256-bit   : " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(notnot256.data() + 24)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(notnot256.data() + 16)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(notnot256.data() + 8)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(notnot256.data()) << std::endl;

    std::cout << " 256-bit in1: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data() + 24)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data() + 16)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data() + 8)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(reg1_256.data()) << std::endl;
    sparta::RegisterBits shift256 = reg1_256 >> 128;
    std::cout << "S256-bit 128: " << std::hex
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256.data() + 24)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256.data() + 16)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256.data() + 8)
              << std::setw(16) << *reinterpret_cast<const uint64_t*>(shift256.data()) << std::endl;

}
