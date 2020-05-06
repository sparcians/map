

import sys
import imp
import os

## Dynamically imports requested scripts and returns requested functions
class ExtensionManager:
    def __init__(self):
        self.__modules = {} # keyed by name
        self.__paths = []

    def __FindOrImportModule(self, name):
        if name in list(self.__modules.keys()):
            return self.__modules[name]
        else:
            for path in self.__paths:
                path_to_py = os.path.join(path, name+'.py')
                try:
                    mod = imp.load_source(name, path_to_py)
                    self.__modules[name] = mod
                    return mod
                except:
                    pass
    
    ## Adds a path to manager's list of paths to import from
    def AddPath(self, path):
        self.__paths.append(path)

    ## Parses a string module:function, loads the module if needed, and returns the function
    def GetFunction(self, string):
        try:
            module_name, func_name = string.split(':')
            module = self.__FindOrImportModule(module_name)
            if module:
                return getattr(module, func_name)
        except:
            return None
