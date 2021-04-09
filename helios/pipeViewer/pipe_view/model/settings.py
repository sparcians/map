import json
import os
import sys
from gui.autocoloring import BrushRepository
from gui.font_utils import GetDefaultFontSize

## Stores settings in a JSON file in the user's config directory so that they can be persisted across runs
class ArgosSettings(object):
    __DEFAULT_CONFIG = {
        'layout_font_size': GetDefaultFontSize(),
        'hover_font_size': GetDefaultFontSize(),
        'palette': 'default',
        'palette_shuffle': 'default'
    }

    __ALLOWED_VALUES = {
        'palette': BrushRepository.get_supported_palettes(),
        'palette_shuffle': BrushRepository.get_supported_shuffle_modes()
    }

    def __init__(self):
        self.__dirty = False

        argos_config_dir = 'argos'
        # %APPDATA% = Windows user config directory
        # $XDG_CONFIG_HOME = Unix-like (and sometimes OS X) user config directory
        config_dir = os.environ.get('APPDATA') or os.environ.get('XDG_CONFIG_HOME')

        if config_dir is None:
            if sys.platform == 'darwin':
                config_dir = os.path.expanduser('~/Library/Preferences') # Apple-recommended location for program settings
                argos_config_dir = 'sparcians.argos'
            else:
                config_dir = os.path.join(os.environ['HOME'], '.config') # Default for Unix-like systems that don't have XDG_* variables defined

        full_config_dir = os.path.join(config_dir, argos_config_dir)
        self.__config_json_path = os.path.join(full_config_dir, 'config.json')

        if not os.path.exists(full_config_dir):
            os.makedirs(full_config_dir)

        self.__config = ArgosSettings.__DEFAULT_CONFIG.copy()

        if not os.path.exists(self.__config_json_path):
            self.__dirty = True
            self.save()
        else:
            with open(self.__config_json_path, 'r') as f:
                self.__config.update(json.load(f))

        for k, v in self.__config.items():
            self.__validate_setting(k, v)

    def __del__(self):
        self.save()

    def save(self):
        if self.__dirty:
            with open(self.__config_json_path, 'w') as f:
                json.dump(self.__config, f)
                self.__dirty = False

    def update(self, new_settings):
        if not isinstance(new_settings, dict):
            raise TypeError('update() must be called with a dict argument')

        for k, v in new_settings.items():
            self[k] = v

    def __validate_setting(self, key, value):
        if key not in ArgosSettings.__DEFAULT_CONFIG:
            raise KeyError(f'{key} is not a recognized settings field')

        allowed_values = ArgosSettings.__ALLOWED_VALUES.get(key)
        if allowed_values and value not in allowed_values:
            raise ValueError(f'{value} is not an allowed value for setting {key}. Allowed values are: {allowed_values}')

    def __getitem__(self, key):
        return self.__config[key]

    def __getattr__(self, key):
        try:
            return self.__getitem__(key)
        except KeyError:
            raise AttributeError

    def __setitem__(self, key, value):
        self.__validate_setting(key, value)
        self.__config[key] = value
        self.__dirty = True

    def __setattr__(self, key, value):
        try:
            self.__setitem__(key, value)
        except KeyError:
            object.__setattr__(self, key, value)

