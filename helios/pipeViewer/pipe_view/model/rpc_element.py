from __future__ import annotations
from .element import Element
from . import element_propsvalid as valid

import wx
import math
import re
import sys
from typing import Any, Callable, List, Optional, Tuple, cast, TYPE_CHECKING

if TYPE_CHECKING:
    from .element_value import Element_Value
    from .extension_manager import ExtensionManager
    from ..gui.layout_canvas import Layout_Canvas
    from .element import PropertyValue, ValidatedPropertyDict


# An element that acts as a graph node
class RPCElement(Element):
    _RPC_PROPERTIES: ValidatedPropertyDict = {
        'id':               ('', valid.validateString),
        # See content_options.py
        'Content':          ('annotation', valid.validateContent),
        'auto_color_basis': ('', valid.validateString),
        # needs better validator eventually
        'color_basis_type': ('string_key', valid.validateString),
        'meta_properties':  ([], valid.validateList),
        'annotation_basis': ('', valid.validateString),
        # needs better validator eventually
        'anno_basis_type':  ('meta_property', valid.validateString),
    }

    __CONTENT_OPTIONS = [
        'annotation',
        'auto_color_annotation',
        'auto_color_anno_notext',
        'auto_color_anno_nomunge'
    ]

    _ALL_PROPERTIES = Element._ALL_PROPERTIES.copy()

    # The RPCElement class shouldn't have a caption field, so we remove it if
    # it exists
    if 'caption' in _ALL_PROPERTIES:
        _ALL_PROPERTIES.pop('caption')
    _ALL_PROPERTIES.update(_RPC_PROPERTIES)

    # Name of the property to use as a metadata key
    _METADATA_KEY_PROPERTY = 'id'

    # Additional metadata properties that should be associated with this
    # element type
    _AUX_METADATA_PROPERTIES = list(Element._AUX_METADATA_PROPERTIES)

    ANNO_BASIS_TYPES = ['meta_property', 'python_exp', 'python_func']

    __EXPR_NAMESPACE = {'re': re, 'math': math}

    @staticmethod
    def GetType() -> str:
        return 'rpc'

    @staticmethod
    def IsDrawable() -> bool:
        return True

    @staticmethod
    def UsesMetadata() -> bool:
        return True

    @staticmethod
    def GetDrawRoutine() -> Callable:
        return RPCElement.DrawRoutine

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        Element.__init__(self, *args, **kwargs)
        self.__extensions = ExtensionManager()

    # Update the meta_properties property for this instance
    def UpdateMetaProperties(self) -> None:
        _AUX_METADATA_PROPERTIES = list(Element._AUX_METADATA_PROPERTIES)
        _AUX_METADATA_PROPERTIES.extend(
            cast(List[str], self.GetProperty('meta_properties'))
        )

    def SetProperty(self, key: str, val: PropertyValue) -> None:
        if key == 'Content' and val not in self.__CONTENT_OPTIONS:
            raise ValueError(
                f'Content type {val} not allowed for RPC elements'
            )

        Element.SetProperty(self, key, val)

        if key == 'meta_properties':
            self.UpdateMetaProperties()

    # Return a listing of the valid options for the 'Content' property
    def GetContentOptions(self) -> List[str]:
        return self.__CONTENT_OPTIONS

    # Return the annotation for this element
    def GetAnnotation(self, pair: Element_Value) -> Optional[str]:
        meta_entry = pair.GetMetaEntries()

        # Get the annotation basis and basis type
        anno_basis_type = self.GetProperty('anno_basis_type')
        annotation_basis = cast(str, self.GetProperty('annotation_basis'))

        # Just use the value of a metadata property
        if anno_basis_type == 'meta_property':
            if not meta_entry:
                return None
            return meta_entry.get(annotation_basis)

        # Use the result of a Python expression
        elif anno_basis_type == 'python_exp':
            try:
                return eval(annotation_basis,
                            {'meta_properties': meta_entry},
                            self.__EXPR_NAMESPACE)

            except Exception:
                print(f'Error: expression "{annotation_basis}" raised '
                      f'exception on input "{meta_entry}":')
                print(sys.exc_info())
                return None

        # Use the result of a Python function
        elif anno_basis_type == 'python_func':
            func = self.__extensions.GetFunction(annotation_basis)
            if func:
                try:
                    return func(meta_entry)
                except Exception:
                    print(f'Error: function "{annotation_basis}" raised '
                          f'exception on input "{meta_entry}":')
                    print(sys.exc_info())
                    return None
            else:
                print(
                    f'Error: function "{annotation_basis}" can not be loaded.'
                )
                return None

        # Invalid basis type
        else:
            return None

    def DrawRoutine(self,
                    pair: Element_Value,
                    dc: wx.DC,
                    canvas: Layout_Canvas,
                    tick: int) -> None:
        (c_x, c_y) = cast(Tuple[int, int], self.GetProperty('position'))
        (c_w, c_h) = cast(Tuple[int, int], self.GetProperty('dimensions'))
        xoff, yoff = canvas.GetRenderOffsets()
        (c_x, c_y) = (c_x - xoff, c_y - yoff)

        auto_color = (cast(str, self.GetProperty('color_basis_type')),
                      cast(str, self.GetProperty('auto_color_basis')))

        annotation = self.GetAnnotation(pair)

        # annotation == None => Something went wrong in generating the
        # annotation, so we set it to !
        if not annotation:
            annotation = '!'

        # Set the element value to the generated annotation
        pair.SetVal(annotation)

        content_type = cast(str, self.GetProperty('Content'))

        # Generate the color for the element
        record = canvas.GetTransactionColor(annotation,
                                            content_type,
                                            auto_color[0],
                                            auto_color[1])
        if record is not None:
            string_to_display, brush, _, _, _, _ = record
        else:
            string_to_display, brush, _, _ = \
                canvas.AddColoredTransaction(annotation,
                                             content_type,
                                             auto_color[0],
                                             auto_color[1],
                                             tick,
                                             self)

        dc.SetBrush(brush)

        # Parameters to easily shift the text within a cell.
        c_y_adj = 0
        c_x_adj = 1

        # Draw the element
        dc.DrawRectangle(c_x, c_y, c_w, c_h)
        if content_type != 'auto_color_anno_notext':
            c_str = string_to_display
            (c_char_width, c_char_height) = dc.GetTextExtent(c_str)
            # Truncate text if possible
            if c_char_width != 0:
                c_num_chars = int(1 + (c_w / c_char_width))
                c_content_str_len = len(c_str)
                if c_num_chars < c_content_str_len:
                    dc.DrawText(c_str[:c_num_chars], c_x+c_x_adj, c_y+c_y_adj)

                elif c_content_str_len > 0:
                    dc.DrawText(c_str, c_x+c_x_adj, c_y+c_y_adj)
            else:
                dc.DrawText(c_str, c_x+c_x_adj, c_y+c_y_adj)

        self.UnsetNeedsRedraw()


RPCElement._ALL_PROPERTIES['type'] = (RPCElement.GetType(),
                                      valid.validateString)
