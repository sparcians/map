from __future__ import annotations
import atexit
import json
import os
import sys
from typing import Any, Dict
from ..gui.autocoloring import BrushRepository
from ..gui.font_utils import GetDefaultFontSize, GetDefaultControlFontSize


# Stores settings in a JSON file in the user's config directory so that they
# can be persisted across runs
class ArgosSettings:
    __DEFAULT_CONFIG = {
        'layout_font_size': GetDefaultFontSize(),
        'hover_font_size': GetDefaultFontSize(),
        'playback_font_size': GetDefaultControlFontSize(),
        'palette': 'default',
        'palette_shuffle': 'default'
    }

    __ALLOWED_VALUES = {
        'palette': BrushRepository.get_supported_palettes(),
        'palette_shuffle': BrushRepository.get_supported_shuffle_modes()
    }

    def __init__(self) -> None:
        self.__dirty = False

        argos_config_dir = 'argos'
        # %APPDATA% = Windows user config directory
        # $XDG_CONFIG_HOME = Unix-like (and sometimes OS X) user config
        # directory
        config_dir = os.environ.get('APPDATA') or \
            os.environ.get('XDG_CONFIG_HOME')

        if config_dir is None:
            if sys.platform == 'darwin':
                # Apple-recommended location for program settings
                config_dir = os.path.expanduser('~/Library/Preferences')
                argos_config_dir = 'sparcians.argos'
            else:
                # Default for Unix-like systems that don't have XDG_* variables
                # defined
                config_dir = os.path.join(os.environ['HOME'], '.config')

        full_config_dir = os.path.join(config_dir, argos_config_dir)
        self.__config_json_path = os.path.join(full_config_dir, 'config.json')

        if not os.path.exists(full_config_dir):
            os.makedirs(full_config_dir)

        self.__config = ArgosSettings.__DEFAULT_CONFIG.copy()
        # Grab a reference to open() so we don't get any errors when at exit
        self.__open = open

        if not os.path.exists(self.__config_json_path):
            self.__dirty = True
            self.save()
        else:
            with self.__open(self.__config_json_path, 'r') as f:
                self.__config.update(json.load(f))

        for k, v in self.__config.items():
            self.__validate_setting(k, v)

        # Register atexit cleanup function so that we don't try to call save()
        # after open() has been deleted
        def cleanup() -> None:
            self.save()

        atexit.register(cleanup)

    def __del__(self) -> None:
        self.save()

    def save(self) -> None:
        if self.__dirty:
            with self.__open(self.__config_json_path, 'w') as f:
                json.dump(self.__config, f)
                self.__dirty = False

    def update(self, new_settings: Dict[str, Any]) -> None:
        if not isinstance(new_settings, dict):
            raise TypeError('update() must be called with a dict argument')

        for k, v in new_settings.items():
            self[k] = v

    def __validate_setting(self, key: str, value: Any) -> None:
        if key not in ArgosSettings.__DEFAULT_CONFIG:
            raise KeyError(f'{key} is not a recognized settings field')

        allowed_values = ArgosSettings.__ALLOWED_VALUES.get(key)
        if allowed_values and value not in allowed_values:
            raise ValueError(
                f'{value} is not an allowed value for setting {key}. '
                f'Allowed values are: {allowed_values}'
            )

    def __getitem__(self, key: str) -> Any:
        return self.__config[key]

    def __getattr__(self, key: str) -> Any:
        try:
            return self.__getitem__(key)
        except KeyError:
            raise AttributeError

    def __setitem__(self, key: str, value: Any) -> None:
        self.__validate_setting(key, value)
        self.__config[key] = value
        self.__dirty = True

    def __setattr__(self, key: str, value: Any) -> None:
        try:
            self.__setitem__(key, value)
        except KeyError:
            object.__setattr__(self, key, value)
