#!/usr/bin/env python3
"""Helper script for generating register definition JSON files.
"""

from enum import IntEnum
import json
import math

from RV64_CSR import CSR64_DEFS
from RV32_CSR import CSR32_DEFS

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

class GenRegisterJSON():
    """Generates register definition JSON files.

    Args:
        group (RegisterGroup): Register group.
        num_regs (int): Number of registers in the group.
        reg_size (int): Size of the registers in bytes (must be power of 2)
    """
    def __init__(self, group, num_regs, reg_size):
        self.group = group
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

        if self.group is RegisterGroup.INT:
            self.gen_int_reg_defs()
        elif self.group is RegisterGroup.FP:
            self.gen_fp_reg_defs()
        elif self.group is RegisterGroup.VEC:
            self.gen_vec_reg_defs()
        elif self.group is RegisterGroup.CSR:
            self.gen_csr_reg_defs()

    def gen_int_reg_defs(self):
        """Generates integer register definitions
        """
        for num in range(0, self.num_regs):
            name = f"x{num}"
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
                "desc":          f"int register {num}",
                "size":          self.reg_size,
                "aliases":       alias,
                "fields":        fields,
                "initial_value": 0,
                "enabled":       True}))

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
            name = f"f{num}"
            alias = FP_ALIASES[name] if (name in FP_ALIASES.keys()) else []
            self.reg_defs.append(self.__CreateRegDict({
                "name":          name,
                "num":           num,
                "desc":          f"floating point register {num}",
                "size":          8,
                "aliases":       alias,
                "fields":        fields,
                "initial_value": 0,
                "enabled":       True}))

    def gen_vec_reg_defs(self):
        """Generates vector register definitions
        """
        fields = {}
        alias = []

        for num in range(0, self.num_regs):
            name = f"v{num}"
            self.reg_defs.append(self.__CreateRegDict({
                "name":          name,
                "num":           num,
                "desc":          f"vector register {num}",
                "size":          self.reg_size,
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

    def add_custom_register(self, name, num, desc, size, aliases, fields, initial_value, enabled):
        self.reg_defs.append(self.__CreateRegDict({
            "name":          name,
            "num":           num,
            "desc":          desc,
            "size":          size,
            "aliases":       aliases,
            "fields":        fields,
            "initial_value": initial_value,
            "enabled":       enabled
        }))

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
        reg_dict["group_num"] = int(self.group)
        reg_dict["group_name"] = GetGroupName(self.group)

        return reg_dict
