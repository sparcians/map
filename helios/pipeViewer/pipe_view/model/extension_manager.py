from __future__ import annotations
import sys
from types import ModuleType
from typing import Callable, Dict, List, Optional
from importlib.machinery import SourceFileLoader
import os

## Dynamically imports requested scripts and returns requested functions
class ExtensionManager:
    def __init__(self) -> None:
        self.__modules: Dict[str, ModuleType] = {} # keyed by name
        self.__paths: List[str] = []

    def __FindOrImportModule(self, name: str) -> Optional[ModuleType]:
        if name in list(self.__modules.keys()):
            return self.__modules[name]
        else:
            for path in self.__paths:
                path_to_py = os.path.join(path, name+'.py')
                try:
                    loader = SourceFileLoader(name, path_to_py)
                    mod = ModuleType(loader.name)
                    loader.exec_module(mod)
                    self.__modules[name] = mod
                    return mod
                except:
                    pass
        return None

    ## Adds a path to manager's list of paths to import from
    def AddPath(self, path: str) -> None:
        self.__paths.append(path)

    ## Parses a string module:function, loads the module if needed, and returns the function
    def GetFunction(self, string: str) -> Optional[Callable]:
        try:
            module_name, func_name = string.split(':')
            module = self.__FindOrImportModule(module_name)
            if module:
                return getattr(module, func_name)
        except:
            pass
        return None
