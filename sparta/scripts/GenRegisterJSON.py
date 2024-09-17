#!/usr/bin/env python3
"""Helper script for regenerating register definition JSON files.
"""

from enum import IntEnum
import json
import math
import sys
import pdb

from RV64_CSR import CSR64_DEFS
from RV32_CSR import CSR32_DEFS

from RV64_CONSTANT import RV64_CONSTS
from RV64_CONSTANT import ATHENA_INTERNAL_REGISTERS

class RegisterGroup(IntEnum):
    INT = 0
    FP = 1
    VEC = 2
    CSR = 3

def GetGroupName(group):
    if group == RegisterGroup.INT:
        return "INT"
    elif group == RegisterGroup.FP:
        return "FP"
    elif group == RegisterGroup.VEC:
        return "VEC"
    elif group == RegisterGroup.CSR:
        return "CSR"
    else:
        return "UNKNOWN"

INT_ALIASES = {
    "x0": ["zero"],
    "x1": ["ra"],
    "x2": ["sp"],
    "x3": ["gp"],
    "x4": ["tp"],
    "x5": ["t0"],
    "x6": ["t1"],
    "x7": ["t2"],
    "x8": ["s0", "fp"],
    "x9": ["s1"],
    "x10": ["a0"],
    "x11": ["a1"],
    "x12": ["a2"],
    "x13": ["a3"],
    "x14": ["a4"],
    "x15": ["a5"],
    "x16": ["a6"],
    "x17": ["a7"],
    "x18": ["s2"],
    "x19": ["s3"],
    "x20": ["s4"],
    "x21": ["s5"],
    "x22": ["s6"],
    "x23": ["s7"],
    "x24": ["s8"],
    "x25": ["s9"],
    "x26": ["s10"],
    "x27": ["s11"],
    "x28": ["t3"],
    "x29": ["t4"],
    "x30": ["t5"],
    "x31": ["t6"],
}
FP_ALIASES = {
    "f0": ["ft0"],
    "f1": ["ft1"],
    "f2": ["ft2"],
    "f3": ["ft3"],
    "f4": ["ft4"],
    "f5": ["ft5"],
    "f6": ["ft6"],
    "f7": ["ft7"],
    "f8": ["fs0"],
    "f9": ["fs1"],
    "f10": ["fa0"],
    "f11": ["fa1"],
    "f12": ["fa2"],
    "f13": ["fa3"],
    "f14": ["fa4"],
    "f15": ["fa5"],
    "f16": ["fa6"],
    "f17": ["fa7"],
    "f18": ["fs2"],
    "f19": ["fs3"],
    "f20": ["fs4"],
    "f21": ["fs5"],
    "f22": ["fs6"],
    "f23": ["fs7"],
    "f24": ["fs8"],
    "f25": ["fs9"],
    "f26": ["fs10"],
    "f27": ["fs11"],
    "f28": ["ft8"],
    "f29": ["ft9"],
    "f30": ["ft10"],
    "f31": ["ft11"],
}
VEC_ALIASES = {
    "v0": [],
    "v1": [],
    "v2": [],
    "v3": [],
    "v4": [],
    "v5": [],
    "v6": [],
    "v7": [],
    "v8": [],
    "v9": [],
    "v10": [],
    "v11": [],
    "v12": [],
    "v13": [],
    "v14": [],
    "v15": [],
    "v16": [],
    "v17": [],
    "v18": [],
    "v19": [],
    "v20": [],
    "v21": [],
    "v22": [],
    "v23": [],
    "v24": [],
    "v25": [],
    "v26": [],
    "v27": [],
    "v28": [],
    "v29": [],
    "v30": [],
    "v31": [],
}

CSR_BF_HEADER_FILE_NAME = "CSRBitMasks{reg_size}.hpp"

CSR_HEADER_FILE_NAME = "CSRNums.hpp"
CSR_NUM_FILE_HEADER = """
#pragma once

#include <cinttypes>

//
// This is generated file from scripts/GenRegisterJSON.py
//
// DO NOT MODIFY THIS FILE BY HAND
//

namespace athena
{
"""

CSR_NUM_FILE_CSRNUM_COMMENT = """
    // CSR Nums
"""

CSR_NUM_FILE_CONSTANT_COMMENT = """
    // Constants defined for RISC-V
"""

CSR_NUM_FILE_INTERNAL_REGS_COMMENT = """
    // Constants defined for RISC-V
"""

CSR_NUM_FILE_FOOTER = """
}
"""

class GenRegisterJSON():
    """Generates register definition JSON files.

    Args:
        group_name (RegisterGroup): Name of register group.
        num_regs (int): Number of registers in the group.
        reg_size (int): Size of the registers in bytes (must be power of 2)
    """

    def __init__(self, group_num, num_regs, reg_size):
        self.group_num = int(group_num)
        self.group_name = GetGroupName(group_num)
        self.num_regs = num_regs
        assert math.log(reg_size, 2).is_integer(), 'reg_size must be a power of 2!'
        self.reg_size = reg_size
        self.reg_defs = []

        self.readonly_fields = {}
        self.readonly_fields["b31_00"] = {"desc":     "read-only",
                                          "low_bit":  0,
                                          "high_bit": 31,
                                          "readonly": True}
        self.readonly_fields["b63_00"] = {"desc":     "read-only",
                                          "low_bit":  0,
                                          "high_bit": 63,
                                          "readonly": True}

        for num in range(0, self.num_regs):
            name = self.get_int_reg_name(num)
        if group_num is RegisterGroup.INT:
            self.gen_int_reg_defs()
        elif group_num is RegisterGroup.FP:
            self.gen_fp_reg_defs()
        elif group_num is RegisterGroup.CSR:
            self.gen_csr_reg_defs()
            self.gen_csr_header_file()
            self.gen_csr_bit_fields()
        elif group_num is RegisterGroup.VEC:
            self.gen_vec_reg_defs()

    def get_int_reg_name(self, num):
        """Returns int register name from register number"""
        return "x"+str(num)

    def get_int_reg_desc(self, num):
        """Returns int register description from register number"""
        return "int register "+str(num)

    def gen_int_reg_defs(self):
        """Generates integer register definitions
        """
        for num in range(0, self.num_regs):
            name = self.get_int_reg_name(num)
            alias = INT_ALIASES[name] if (name in INT_ALIASES.keys()) else []
            fields = {}
            if (num == 0):
                if (self.reg_size == 4):
                    fields["b31_00"] = self.readonly_fields["b31_00"]
                else:
                    fields["b63_00"] = self.readonly_fields["b63_00"]

            self.reg_defs.append(self.__CreateRegDict({
                "name":          name,
                "num":           num,
                "desc":          self.get_int_reg_desc(num),
                "size":          self.reg_size,
                "aliases":       alias,
                "fields":        fields,
                "initial_value": 0,
                "enabled":       True}))

    def get_fp_reg_name(self, num):
        """Returns fp register name from register number"""
        return "f"+str(num)

    def get_fp_reg_desc(self, num):
        """Returns fp register description from register number"""
        return "floating point register "+str(num)

    def gen_fp_reg_defs(self):
        """Generates floating point register definitions
        """

        fields = {}
        fields["sp"] = {"desc":     "single precision",
                        "low_bit":  0,
                        "high_bit": 31,
                        "readonly": False}
        fields["dp"] = {"desc":     "double precision",
                        "low_bit":  0,
                        "high_bit": 63,
                        "readonly": False}

        for num in range(0, self.num_regs):
            name = self.get_fp_reg_name(num)
            alias = FP_ALIASES[name] if (name in FP_ALIASES.keys()) else []
            self.reg_defs.append(self.__CreateRegDict({
                "name":          name,
                "num":           num,
                "desc":          self.get_fp_reg_desc(num),
                "size":          8,
                "aliases":       alias,
                "fields":        fields,
                "initial_value": 0,
                "enabled":       True}))

    def gen_csr_reg_defs(self):
        """Generate control and status register definitions
        """
        CSR_DEFS = CSR32_DEFS if (self.reg_size == 4) else CSR64_DEFS

        for k, v in CSR_DEFS.items():
            self.reg_defs.append(self.__CreateRegDict({
                "name":          v[0],
                "num":           k,
                "desc":          v[1],
                "size":          8,
                "aliases":       ["csr"+str(k)],
                "fields":        v[2],
                "initial_value": v[3],
                "enabled":       True}))

    def gen_csr_header_file(self):
        """Generate the CSR CPP header file
        """

        csr_header_file = open(CSR_HEADER_FILE_NAME, "w")
        csr_header_file.write(CSR_NUM_FILE_HEADER)

        # TODO: Can we get keys in JSON to be printed in hex?
        CSR_DEFS = CSR32_DEFS.copy()
        CSR_DEFS.update(CSR64_DEFS)
        CONST = RV64_CONSTS

        csr_header_file.write(CSR_NUM_FILE_CSRNUM_COMMENT)
        csr_largest_value = -1;
        for k, v in CSR_DEFS.items():
            csr_header_file.write("    static constexpr uint32_t "+
                                  (v[0]).upper()+
                                  " = "+
                                  str(hex(k))+
                                  "; // "+
                                  str(k)+
                                  "\n")
            csr_largest_value = max(csr_largest_value, k)

        csr_header_file.write("    static constexpr uint32_t CSR_LARGEST_VALUE = "+
                              str(hex(csr_largest_value)) +
                              ";\n")

        csr_header_file.write(CSR_NUM_FILE_CONSTANT_COMMENT)
        for k, v in CONST.items():
            csr_header_file.write("    static constexpr uint64_t "+
                                  k.upper()+
                                  " = "+
                                  str(hex(v[0]))+
                                  "; // "+
                                  str(v[1])+
                                  "\n")

        csr_header_file.write(CSR_NUM_FILE_INTERNAL_REGS_COMMENT)
        for k, v in ATHENA_INTERNAL_REGISTERS.items():
            csr_header_file.write("    static constexpr uint64_t "+
                                  k.upper()+
                                  " = "+
                                  str(hex(v[0]))+
                                  "; // "+
                                  str(v[1])+
                                  "\n")

        csr_header_file.write(CSR_NUM_FILE_FOOTER)
        csr_header_file.close()

    def gen_csr_bit_fields(self):
        """Generate the CSR header file with bitfields
        """
        data_width = self.reg_size*8
        csr_bf_header_file = open(CSR_BF_HEADER_FILE_NAME.format(reg_size=data_width), "w")
        csr_bf_header_file.write(CSR_NUM_FILE_HEADER)

        CSR_DEFS = CSR32_DEFS if (self.reg_size == 4) else CSR64_DEFS
        FILL_MASK = 0xffffffffffffffff
        SIZE = 64

        for k, v in CSR_DEFS.items():
            # Print only if there are bit flags
            if v[2]:
                csr_bf_header_file.write("    namespace "+
                                         (v[0]).upper()+
                                         "_" + str(data_width) + "_bitmasks {\n")
                fields = v[2]
                for name in fields:
                    if name.lower() != "resv" and name.lower() != "wpri":
                        high_bit = fields[name]['high_bit']
                        low_bit = fields[name]['low_bit']
                        mask = (FILL_MASK >> (SIZE - (high_bit - low_bit + 1))) << low_bit
                        csr_bf_header_file.write("        static constexpr uint64_t " +
                                                 name + " = " + hex(mask) + ";\n")

                csr_bf_header_file.write("    } // namespace "+
                                         (v[0]).upper()+
                                         "_" + str(data_width) + "_bitfield\n\n")



        csr_bf_header_file.write(CSR_NUM_FILE_FOOTER)
        csr_bf_header_file.close()


    def get_vec_reg_name(self, num):
        """Returns v register name from register number"""
        return "v"+str(num)

    def get_vec_reg_desc(self, num):
        """Returns v register description from register number"""
        return "vector register "+str(num)

    def gen_vec_reg_defs(self):
        """Generates vector register definitions
        """

        fields = {}
        alias = []

        for num in range(0, self.num_regs):
            name = self.get_vec_reg_name(num)
            self.reg_defs.append(self.__CreateRegDict({
                "name":          name,
                "num":           num,
                "desc":          self.get_vec_reg_desc(num),
                "size":          self.reg_size,
                "aliases":       alias,
                "fields":        fields,
                "initial_value": 0,
                "enabled":       True}))

    def write_json(self, filename):
        """Write register definitions to a JSON file"""
        with open(filename,"w") as fh:
            json.dump(self.reg_defs, fh, indent=4)

    def __CreateRegDict(self, reg_dict):
        # Remove the 'fields' key if it is empty
        if not reg_dict["fields"]:
            del reg_dict["fields"]

        # Format the 'initial_value' to hex, or remove it if it is 0
        if reg_dict["initial_value"]:
            reg_dict["initial_value"] = hex(reg_dict["initial_value"])
        else:
            del reg_dict["initial_value"]

        # Remove the 'enabled' key if it is True
        if reg_dict["enabled"]:
            del reg_dict["enabled"]

        # Add group num and group name
        reg_dict["group_num"] = self.group_num
        reg_dict["group_name"] = self.group_name

        return reg_dict

def main():

    if len(sys.argv) != 2:
        print("Usage: GenRegisterJSON.py <rv32|rv64>")
        exit(1)

    isa_string = sys.argv[1]

    if isa_string not in ["rv32", "rv64"]:
        print("Usage: GenRegisterJSON.py <rv32|rv64>")
        exit(1)

    # Default to RV64
    xlen = 4 if "32" in isa_string else 8
    vlen = 32

    # Generate rv64g int, fp and CSR registers
    reg_int = GenRegisterJSON(RegisterGroup.INT, 32, xlen);
    reg_fp  = GenRegisterJSON(RegisterGroup.FP, 32, xlen);
    reg_vec = GenRegisterJSON(RegisterGroup.VEC, 32, vlen)
    reg_csr = GenRegisterJSON(RegisterGroup.CSR, 0, xlen);

    isa_string = ''.join(i for i in isa_string if not i.isdigit())
    reg_int.write_json("reg_int.json");
    reg_fp.write_json("reg_fp.json");
    reg_vec.write_json("reg_vec.json")
    reg_csr.write_json("reg_csr.json");

if __name__ == "__main__":
    main()
