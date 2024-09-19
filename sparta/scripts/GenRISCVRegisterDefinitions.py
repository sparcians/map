#!/usr/bin/env python3
"""Script for generating RISC-V register definition JSON files.
"""

from enum import Enum
import os

from GenRegisterJSON import GenRegisterJSON
from GenRegisterJSON import RegisterGroup

def main():
    # Make rv64 directory if it doesn't exist
    if not os.path.exists("rv64"):
        os.makedirs("rv64")
    os.chdir("rv64")

    # Generate rv64g int, fp and CSR registers
    RV64_XLEN = 8
    VLEN = 32
    reg_int = GenRegisterJSON(RegisterGroup.INT, 32, RV64_XLEN)
    reg_fp  = GenRegisterJSON(RegisterGroup.FP,  32, RV64_XLEN)
    reg_vec = GenRegisterJSON(RegisterGroup.VEC, 32, VLEN)
    reg_csr = GenRegisterJSON(RegisterGroup.CSR, 0,  RV64_XLEN)

    # Add register for the PC
    num = 32
    reg_int.add_custom_register("pc", num, "Program counter", 8, [], {}, 0, True)

    # Add registers for atomic load-reservation and store-conditional instructions
    num += 1
    reg_int.add_custom_register("resv_addr", num, "Load reservation address", 8, [], {}, 0, True)
    num += 1
    reg_int.add_custom_register("resv_valid", num, "Load reservation valid", 8, [], {}, 0, True)

    reg_int.write_json("reg_int.json")
    reg_fp.write_json("reg_fp.json")
    reg_vec.write_json("reg_vec.json")
    reg_csr.write_json("reg_csr.json")

    # Make rv32 directory if it doesn't exist
    os.chdir("..")
    if not os.path.exists("rv32"):
        os.makedirs("rv32")
    os.chdir("rv32")

    # Generate rv32g int, fp and CSR registers
    RV32_XLEN = 4
    reg_int = GenRegisterJSON(RegisterGroup.INT, 32, RV32_XLEN)
    reg_fp  = GenRegisterJSON(RegisterGroup.FP,  32, RV32_XLEN)
    reg_vec = GenRegisterJSON(RegisterGroup.VEC, 32, VLEN)
    reg_csr = GenRegisterJSON(RegisterGroup.CSR, 0,  RV32_XLEN)

    reg_int.write_json("reg_int.json");
    reg_fp.write_json("reg_fp.json");
    reg_vec.write_json("reg_vec.json")
    reg_csr.write_json("reg_csr.json");


if __name__ == "__main__":
    main()
