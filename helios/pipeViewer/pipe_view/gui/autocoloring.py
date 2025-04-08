# @package Argos auto-coloring color map

from __future__ import annotations
import wx
import colorsys  # For easy HSL to RGB conversion
import numpy as np
from typing import Dict, Set, Tuple

try:
    from daltonize import daltonize
    HAS_DALTONIZE = True
except ModuleNotFoundError:
    HAS_DALTONIZE = False


class BrushRepository:
    _COLORBLIND_MODES = set(('d', 'p', 't'))
    _PALETTES = set(('default',)) | _COLORBLIND_MODES
    _SHUFFLE_MODES = set(('default', 'shuffled'))

    def __init__(self) -> None:
        self.palette = 'default'
        self.shuffle_mode = 'default'
        self.brushes: Dict[str, Dict[str, Dict[int, wx.Brush]]] = {}
        for shuffle_mode in self._SHUFFLE_MODES:
            self.brushes[shuffle_mode] = {}

    @classmethod
    def get_supported_palettes(cls) -> Set[str]:
        return cls._PALETTES

    @classmethod
    def get_supported_shuffle_modes(cls) -> Set[str]:
        return cls._SHUFFLE_MODES

    def __getitem__(self, idx: int) -> wx.Brush:
        return self.as_dict()[idx]

    def as_dict(self) -> Dict[int, wx.Brush]:
        return self.brushes[self.shuffle_mode][self.palette]

    def __len__(self) -> int:
        return len(self.as_dict())

    def validate_shuffle_mode(self, shuffle_mode: str) -> None:
        if shuffle_mode not in self._SHUFFLE_MODES:
            raise ValueError(
                'Shuffle mode shoud be one of the following values: '
                f'{self._SHUFFLE_MODES}'
            )

    def validate_palette(self, palette: str) -> None:
        if not HAS_DALTONIZE and palette in self._COLORBLIND_MODES:
            raise NotImplementedError(
                'Daltonize is not installed.'
                '`pip install daltonize` to enable colorblindness modes.'
            )
        if palette not in self._PALETTES:
            raise ValueError(
                'Palette mode should be one of the following values: '
                f'{self._PALETTES}'
            )

    def generate_shuffled_palette(
        self,
        shuffle_mode: str,
        color_dict: Dict[int, Tuple[float, float, float]]
    ) -> Dict[int, Tuple[float, float, float]]:
        self.validate_shuffle_mode(shuffle_mode)
        if shuffle_mode == 'default':
            return color_dict
        elif shuffle_mode == 'shuffled':
            # Shuffle the colors around
            new_dict = {
                (3 * key) % len(color_dict): value
                for key, value in color_dict.items()
            }
            # Make sure the shuffle didn't leave out any of the original keys
            assert set(new_dict.keys()) == \
                   set(range(min(color_dict.keys()),
                             max(color_dict.keys()) + 1))
            return new_dict
        else:
            raise NotImplementedError(
                f'Shuffle mode {shuffle_mode} has not been implemented.'
            )

    def create_brush_dict(
        self,
        color_dict: Dict[int, Tuple[float, float, float]]
    ) -> Dict[int, wx.Brush]:
        return {
            key: wx.Brush([int(255 * x) for x in value])
            for key, value in color_dict.items()
        }

    def generate_all_brushes(
        self,
        color_dict: Dict[int, Tuple[float, float, float]]
    ) -> None:
        shuffle_idx = 0
        shuffle_idx_to_mode = {}
        shuffled_color_dicts = {}

        for shuffle_mode in self._SHUFFLE_MODES:
            # Keep track of which y-index maps to which shuffle mode
            shuffle_idx_to_mode[shuffle_idx] = shuffle_mode

            shuffled_color_dicts[shuffle_mode] = \
                self.generate_shuffled_palette(shuffle_mode, color_dict)

            # The default palettes don't get daltonized, so we can go ahead and
            # create brushes for them here
            self.brushes[shuffle_mode]['default'] = \
                self.create_brush_dict(shuffled_color_dicts[shuffle_mode])

            shuffle_idx += 1

        if HAS_DALTONIZE:
            for palette in self._COLORBLIND_MODES:
                # Create an image containing 1 RGB pixel for each color in each
                # shuffled dict
                colors = np.zeros(
                    (len(self._SHUFFLE_MODES), len(color_dict), 3)
                )
                for shf_idx, shf_mode in enumerate(self._SHUFFLE_MODES):
                    for p_idx, color in shuffled_color_dicts[shf_mode].items():
                        colors[shf_idx, p_idx] = color

                # Convert the colors to the requested colorblindness mode
                daltonized_colors = daltonize.daltonize(colors, palette)

                # Go back through the image and get the new pixel colors
                new_dict: Dict[str, Dict[int, Tuple[float, float, float]]] = {}

                for shuffle_idx in range(len(self._SHUFFLE_MODES)):
                    for color_idx in range(len(color_dict)):
                        shuffle_mode = shuffle_idx_to_mode[shuffle_idx]
                        if shuffle_mode not in new_dict:
                            new_dict[shuffle_mode] = {}
                        new_dict[shuffle_mode][color_idx] = \
                            daltonized_colors[shuffle_idx, color_idx]

                for shuffle_mode in self._SHUFFLE_MODES:
                    self.brushes[shuffle_mode][palette] = \
                        self.create_brush_dict(new_dict[shuffle_mode])

    def set_shuffle_mode(self, shuffle_mode: str) -> None:
        self.validate_shuffle_mode(shuffle_mode)
        self.shuffle_mode = shuffle_mode

    def set_palette(self, palette: str) -> None:
        self.validate_palette(palette)
        self.palette = palette


BACKGROUND_BRUSHES = BrushRepository()
REASON_BRUSHES = BrushRepository()


def SetPalettes(palette: str) -> None:
    BACKGROUND_BRUSHES.set_palette(palette)
    REASON_BRUSHES.set_palette(palette)


def SetShuffleModes(mode: str) -> None:
    BACKGROUND_BRUSHES.set_shuffle_mode(mode)
    REASON_BRUSHES.set_shuffle_mode(mode)


# call after wx.App init
def BuildBrushes(colorblindness_mode: str, shuffle_mode: str) -> None:
    # Map of background colors based on annotation content
    # The first pass used pretty saturated colors, which was too dark, and
    # hard to read on some monitors.  I've altered it to use three different
    # sequences of pastels, and hand-adjusted some to try to look better on
    # the screen.  - bgrayson
    # There are 32 colors used right now, and 16 different shades makes the
    # gradations too small, so let's do three passes of unequal sizes:
    #    11, 11, and 10.
    # The lightness of 0.9 is too light, so using 0.6, 0.7, and 0.8
    # I manually adjusted the last color in the first two slices to 10.5 to
    # make it more distinguished.
    BACKGROUND_COLORS = {
        # --------------------------------------------------
        # New colorization method (based on sequence ID)
        # - see transaction_viewer/argos_view/core/src/renderer.pyx
        # - note:  number of colors is set in renderer.pyx, in the
        #   variable NUM_ANNOTATION_COLORS
        # - note2:  if changes are made in renderer.pyx, then you must type
        #   "make" in transaction_viewer
        #
        0: colorsys.hls_to_rgb(0 / 32.0, 0.75, 1),
        1: colorsys.hls_to_rgb(1 / 32.0, 0.75, 1),
        2: colorsys.hls_to_rgb(2 / 32.0, 0.75, 1),
        3: colorsys.hls_to_rgb(3 / 32.0, 0.75, 1),
        4: colorsys.hls_to_rgb(4 / 32.0, 0.75, 1),
        5: colorsys.hls_to_rgb(5 / 32.0, 0.75, 1),
        6: colorsys.hls_to_rgb(6 / 32.0, 0.75, 1),
        7: colorsys.hls_to_rgb(7 / 32.0, 0.75, 1),
        8: colorsys.hls_to_rgb(8 / 32.0, 0.75, 1),
        9: colorsys.hls_to_rgb(9 / 32.0, 0.75, 1),
        10: colorsys.hls_to_rgb(10 / 32.0, 0.75, 1),

        11: colorsys.hls_to_rgb(11 / 32.0, 0.75, 1),
        12: colorsys.hls_to_rgb(12 / 32.0, 0.75, 1),
        13: colorsys.hls_to_rgb(13 / 32.0, 0.75, 1),
        14: colorsys.hls_to_rgb(14 / 32.0, 0.75, 1),
        15: colorsys.hls_to_rgb(15 / 32.0, 0.75, 1),
        16: colorsys.hls_to_rgb(16 / 32.0, 0.75, 1),
        17: colorsys.hls_to_rgb(17 / 32.0, 0.75, 1),
        18: colorsys.hls_to_rgb(18 / 32.0, 0.75, 1),
        19: colorsys.hls_to_rgb(19 / 32.0, 0.75, 1),
        20: colorsys.hls_to_rgb(20 / 32.0, 0.75, 1),
        21: colorsys.hls_to_rgb(21 / 32.0, 0.75, 1),

        22: colorsys.hls_to_rgb(22 / 32.0, 0.75, 1),
        23: colorsys.hls_to_rgb(23 / 32.0, 0.75, 1),
        24: colorsys.hls_to_rgb(24 / 32.0, 0.75, 1),
        25: colorsys.hls_to_rgb(25 / 32.0, 0.75, 1),
        26: colorsys.hls_to_rgb(26 / 32.0, 0.75, 1),
        27: colorsys.hls_to_rgb(27 / 32.0, 0.75, 1),
        28: colorsys.hls_to_rgb(28 / 32.0, 0.75, 1),
        29: colorsys.hls_to_rgb(29 / 32.0, 0.75, 1),
        30: colorsys.hls_to_rgb(30 / 32.0, 0.75, 1),
        31: colorsys.hls_to_rgb(31 / 32.0, 0.75, 1),
    }

    REASON_COLORS = {
        0: colorsys.hls_to_rgb(1.0, 1.0, 1.0),
        1: colorsys.hls_to_rgb(1 / 16.0, 0.75, 1),
        2: colorsys.hls_to_rgb(2 / 16.0, 0.75, 1),
        3: colorsys.hls_to_rgb(3 / 16.0, 0.75, 1),
        4: colorsys.hls_to_rgb(4 / 16.0, 0.75, 1),
        5: colorsys.hls_to_rgb(5 / 16.0, 0.75, 1),
        6: colorsys.hls_to_rgb(6 / 16.0, 0.75, 1),
        7: colorsys.hls_to_rgb(7 / 16.0, 0.75, 1),
        8: colorsys.hls_to_rgb(8 / 16.0, 0.75, 1),
        9: colorsys.hls_to_rgb(9 / 16.0, 0.75, 1),
        10: colorsys.hls_to_rgb(10 / 16.0, 0.75, 1),

        11: colorsys.hls_to_rgb(11 / 16.0, 0.75, 1),
        12: colorsys.hls_to_rgb(12 / 16.0, 0.75, 1),
        13: colorsys.hls_to_rgb(13 / 16.0, 0.75, 1),
        14: colorsys.hls_to_rgb(14 / 16.0, 0.75, 1),
        15: colorsys.hls_to_rgb(15 / 16.0, 0.75, 1),
    }

    BACKGROUND_BRUSHES.generate_all_brushes(BACKGROUND_COLORS)
    REASON_BRUSHES.generate_all_brushes(REASON_COLORS)
    SetPalettes(colorblindness_mode)
    SetShuffleModes(shuffle_mode)
