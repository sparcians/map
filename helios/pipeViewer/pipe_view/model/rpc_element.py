from .element import Element
from . import element_propsvalid as valid

import wx
import math
import re
import sys

## An element that acts as a graph node
class RPCElement(Element):
    _RPC_PROPERTIES = {'id'                 : ('', valid.validateString),
                       'Content'            : ('annotation' , valid.validateContent), # See content_options.py
                       'auto_color_basis'   : ('' , valid.validateString),
                       'color_basis_type'   : ('string_key' , valid.validateString), # needs better validator eventually
                       'meta_properties'    : ([], valid.validateList),
                       'annotation_basis'   : ('', valid.validateString),
                       'anno_basis_type'    : ('meta_property' , valid.validateString), # needs better validator eventually
    }

    __CONTENT_OPTIONS = ['annotation', 'auto_color_annotation', 'auto_color_anno_notext', 'auto_color_anno_nomunge']

    _ALL_PROPERTIES = Element._ALL_PROPERTIES.copy()

    # The RPCElement class shouldn't have a caption field, so we remove it if it exists
    if 'caption' in _ALL_PROPERTIES:
        _ALL_PROPERTIES.pop('caption')
    _ALL_PROPERTIES.update(_RPC_PROPERTIES)

    # Name of the property to use as a metadata key
    _METADATA_KEY_PROPERTY = 'id'

    # Additional metadata properties that should be associated with this element type
    _AUX_METADATA_PROPERTIES = list(Element._AUX_METADATA_PROPERTIES)

    ANNO_BASIS_TYPES = ['meta_property', 'python_exp', 'python_func']

    __EXPR_NAMESPACE = {'re':re, 'math':math}

    @staticmethod
    def GetType():
        return 'rpc'

    @staticmethod
    def IsDrawable():
        return True

    @staticmethod
    def UsesMetadata():
        return True

    @staticmethod
    def GetDrawRoutine():
        return RPCElement.DrawRoutine

    def __init__(self, *args, **kwargs):
        Element.__init__(self, *args, **kwargs)

    ## Update the meta_properties property for this instance
    def UpdateMetaProperties(self):
        _AUX_METADATA_PROPERTIES = list(Element._AUX_METADATA_PROPERTIES)
        _AUX_METADATA_PROPERTIES.append(self.GetProperty('meta_properties'))

    def SetProperty(self, key, val):
        if key == 'Content' and val not in self.__CONTENT_OPTIONS:
            raise ValueError('Content type {} not allowed for RPC elements'.format(val))

        Element.SetProperty(self, key, val)

        if key == 'meta_properties':
            self.UpdateMetaProperties()

    ## Return a listing of the valid options for the 'Content' property
    def GetContentOptions(self):
        return self.__CONTENT_OPTIONS

    ## Return the annotation for this element
    def GetAnnotation(self, pair):
        meta_entry = pair.GetMetaEntries()

        # Get the annotation basis and basis type
        anno_basis_type = self.GetProperty('anno_basis_type')
        annotation_basis = self.GetProperty('annotation_basis')

        # Just use the value of a metadata property
        if anno_basis_type == 'meta_property':
            if not meta_entry:
                return None
            return meta_entry.get(annotation_basis)

        # Use the result of a Python expression
        elif anno_basis_type == 'python_exp':
            try:
                return eval(annotation_basis, {'meta_properties': meta_entry}, self.__EXPR_NAMESPACE)

            except:
                print(('Error: expression "{}"" raised exception on input "{}":'.format(annotation_basis, meta_entry)))
                print((sys.exc_info()))
                return None

        # Use the result of a Python function
        elif anno_basis_type == 'python_func':
            func = self.__extensions.GetFunction(annotation_basis)
            if func:
                try:
                    return func(meta_entry)
                except:
                    print(('Error: function "{}"" raised exception on input "{}":'.format(annotation_basis, meta_entry)))
                    print((sys.exc_info()))
                    return None
            else:
                print(('Error: function "{}" can not be loaded.'.format(annotation_basis)))
                return None

        # Invalid basis type
        else:
            return None

    def DrawRoutine(self,
                    pair,
                    dc,
                    canvas,
                    tick):
        (c_x,c_y),(c_w,c_h) = self.GetProperty('position'),self.GetProperty('dimensions')
        xoff, yoff = canvas.GetRenderOffsets()
        (c_x,c_y) = (c_x-xoff, c_y-yoff)

        auto_color = self.GetProperty('color_basis_type'), self.GetProperty('auto_color_basis')

        annotation = self.GetAnnotation(pair)

        # annotation == None => Something went wrong in generating the annotation, so we set it to !
        if not annotation:
            annotation = '!'

        # Set the element value to the generated annotation
        pair.SetVal(annotation)

        content_type = self.GetProperty('Content')

        # Generate the color for the element
        record = canvas.GetTransactionColor(annotation, content_type, auto_color[0], auto_color[1])
        if record:
            string_to_display, brush = record
        else:
            string_to_display, brush = canvas.AddColoredTransaction(annotation, content_type, auto_color[0], auto_color[1], tick, self)

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
                c_num_chars = 1 + (c_w / c_char_width)
                c_content_str_len = len(c_str)
                if c_num_chars < c_content_str_len:
                    c_tmp_char = c_str[c_num_chars]
                    c_str[c_num_chars] = '\0'
                    dc.DrawText(c_str, c_x+c_x_adj, c_y+c_y_adj)
                    c_str[c_num_chars] = c_tmp_char

                elif c_content_str_len > 0:
                    dc.DrawText(c_str, c_x+c_x_adj, c_y+c_y_adj)
            else:
                dc.DrawText(c_str, c_x+c_x_adj, c_y+c_y_adj)

        self.UnsetNeedsRedraw()


RPCElement._ALL_PROPERTIES['type'] = (RPCElement.GetType(), valid.validateString)


