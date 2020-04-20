import traceback

import os
import sys
import copy
import re
import sre_constants
import weakref

sys.path.append('../')
import yaml

from . import element_propsvalid as valid
import logging

import wx # Though this is part of the model-side, it is easier to place some
          # drawing routines here - there is a precedent for that already anyway

# Another view-side import since elements here have rendering code embedded.
import gui.autocoloring

import logsearch # Argos module for searching logfiles


# # The building blocks of a Layout
class Element(object):
#    __FRAME_COLOR = (230, 230, 230)
    __FRAME_COLOR = (128, 128, 128)

    # Set the default values for each property of an Element
    _ALL_PROPERTIES = {
        'type'                  : (''              , valid.validateString),
        'position'              : ((10, 7)          , valid.validatePos),
        'dimensions'            : ((120, 14)        , valid.validateDim),
        'color'                 : (__FRAME_COLOR   , valid.validateColor),
        't_offset'              : (0               , valid.validateOffset), # Clock cycles, not HC's
        'caption'               : (''              , valid.validateString),
        'on_update'             : (''              , valid.validateString),
        'on_init'               : (''              , valid.validateString),
        'on_cycle_changed'      : (''              , valid.validateString)
    }

    # Name of the property to use as a metadata key
    _METADATA_KEY_PROPERTY = ''

    # Additional metadata properties that should be associated with this element type
    _AUX_METADATA_PROPERTIES = []

    __CONTENT_OPTIONS = valid.GetContentOptions()

    __cur_pin = -1

    # Store PIN in root class
    @staticmethod
    def Gen_PIN():
        Element.__cur_pin += 1
        return Element.__cur_pin

    # # called to see if we element should be considered for rendering
    # Currently there is no 'ghost' type
    @staticmethod
    def IsDrawable():
        return False

    # # called to see if we element will be making queries
    @staticmethod
    def NeedsDatabase():
        return False

    # # called to see if we element is selectable in playback mode
    @staticmethod
    def IsSelectable():
        return True

    # # called to see if we element will gather data from additional'meta'
    # table of data from database
    @staticmethod
    def UsesMetadata():
        return False

    # # called when drawn is not null
    @staticmethod
    def GetDrawRoutine():
        return None

    # # indicates if this is a possible container
    # if it is then the following methods must be added:
    # GetChildren(self) -> [Element, ...]
    # AddChild(self, Element)
    # RemoveChild(self, Element)
    @staticmethod
    def HasChildren():
        return False

    @staticmethod
    def GetElementProperties():
        return Element._ALL_PROPERTIES

    @staticmethod
    def GetType():
        return ''

    # # returns a list of property names (keys) to exclude from
    # element property dialog
    @staticmethod
    def GetHiddenProperties():
        return []

    # # returns a list of property names (keys) to mark read-only in
    # element property dialog
    @staticmethod
    def GetReadOnlyProperties():
        return []

    # # The constructor
    #  if an Element is passed in, then a duplicate is created, otherwise the
    #  default values are loaded
    #  @param initial_properties Properties dictionary that overrides any
    #  defaults
    #  @param force_pin Force the PIN to be a specific value. Caller is
    #  responsible for prevenging layout duplicates
    #  @profile
    #  Must be run by subclass implementing _ALL_PROPERTIES static varible
    def __init__(self, duplicate = None, container = None, initial_properties = None, parent = None, force_pin = None):
        self._parent = parent
        self.__draw = True
        self.__needs_redraw = True
        self._properties = {}
        self.__pin = force_pin if force_pin is not None else self.Gen_PIN()
        if force_pin is not None and container is not None:
            assert container.FindByPIN(force_pin) is None, 'forced PIN {} already exists in layout'.format(force_pin)

        # layout needs to be None to begin with, to allow for proper sorting
        # in Layout & Layout Context the first time after the other
        # properties are set
        self._layout = None

        if duplicate is None:
            for key in self._ALL_PROPERTIES:
                self._properties[key] = self._ALL_PROPERTIES[key][0]

            if initial_properties:
                # Overwrite defaults with any initial settings we have.
                for key in initial_properties:
                    prop = self._ALL_PROPERTIES.get(key)
                    if prop:
                        self._properties[key] = prop[1](key, initial_properties[key])
                    elif key == 'annotation':
                        # Suppress error, but also further generation of this element
                        # unused--this property is dynamically generated.
                        # this code can be removed when annotation fields are completely
                        # phased out.
                        # -N.S. 06/21/13
                        pass
                    else:
                        raise Exception('Trying to add unknown property type: %s' % key)
        else:
            self._properties = duplicate._properties.copy()

        # Unescape input. This must be symmetric with _GetYAMLEvents
        for k, v in self._properties.items():
            if isinstance(v, str):
                self._properties[k] = v.replace('\\"', '"').replace('\\n', '\n').replace('\\r', '\r')
            # for key in duplicate.__properties:
            #    self._properties[key]=duplicate.__properties[key]
            # #logging.debug("Successfully duplicated an element")

        # These need to be set after the properties are set
        self._layout = container

        # At end of construction, mark as clean
        self.__changed = False

        self._pen = wx.Pen(self._properties['color'], 1)

    # # Return the unique identifier of this element
    def GetPIN(self):
        return self.__pin

    # # Return parent for this object
    def GetParent(self):
        return self._parent

    # # Return the layout which this element belongs to
    def GetLayout(self):
        return self._layout

    def GetBounds(self):
        pos = self.GetProperty('position')
        size = self.GetProperty('dimensions')
        return pos[0], pos[1], pos[0] + size[0], pos[1] + size[1]

    # # Set a flag that the element needs to be redrawn
    def SetNeedsRedraw(self):
        self.__needs_redraw = True

    # # Unset the redraw flag
    def UnsetNeedsRedraw(self):
        self.__needs_redraw = False

    # # Query the redraw flag
    def NeedsRedraw(self):
        return self.__needs_redraw

    # # Set the Element's property key to the value val, if it passes the
    #  validation effort
    #  @param key Property to change
    #  @param val New value for the property. Note that val can always be a
    #  string regardless of property type
    def SetProperty(self, key, val):
        if not key in self._properties:
            raise ValueError('Attempting to set a non-existent property: ' + str(key))

        orig_val = self._properties[key]

        props = self._ALL_PROPERTIES.get(key)
        val = props[1](key, val)
        if (key == 'caption'):
            self._properties[key] = val
            self._layout.Refresh(self)
        elif key == 'position' or key == 'dimensions':
            self._properties[key] = val
            self._layout.ElementsMoved(self)
            self._layout.Refresh(self)
        elif key == 'color':
            self._properties[key] = val
            self._pen = wx.Pen(val, 1)
        else:
            self._properties[key] = val

        # Setting a property is a change if the new value is actually different
        if val != orig_val:
            self.__changed = True
            self._layout.SetChanged()

    def SetX(self, x):
        current = self._properties['position']
        orig_val = current[0]
        val = self._ALL_PROPERTIES['position'][1]('position', (x, current[1]))
        self._properties['position'] = val
        self._layout.ElementsMoved(self)

        # Setting a property is a change if the new value is actually different
        if val[0] != orig_val:
            self.__changed = True
            self._layout.SetChanged()

    def SetY(self, y):
        current = self._properties['position']
        orig_val = current[1]
        val = self._ALL_PROPERTIES['position'][1]('position', (current[0], y))
        self._properties['position'] = val
        self._layout.ElementsMoved(self)

        # Setting a property is a change if the new value is actually different
        if val[0] != orig_val:
            self.__changed = True
            self._layout.SetChanged()

    # # Fetch the value for the given key
    def GetProperty(self, key, period = None):
        return self._properties[key]

    # # These shortcut functions were added in order to improve performance for some derived types
    # # Shortcut to get X dimension
    def GetXDim(self):
        return self.GetProperty('dimensions')[0]

    # # Shortcut to get Y dimension
    def GetYDim(self):
        return self.GetProperty('dimensions')[1]

    # # Shortcut to get X position
    def GetXPos(self):
        return self.GetProperty('position')[0]

    # # Shortcut to get Y position
    def GetYPos(self):
        return self.GetProperty('position')[1]

    # # Return the entire dict of properties
    def GetProperties(self):
        return self._properties

    # # Does this element have a particular property
    def HasProperty(self, key):
        return key in self._properties

    # # Return a listing of the valid options for the 'Content' property
    def GetContentOptions(self):
        return self.__CONTENT_OPTIONS

    # # Override and return offset times to query range of data at update.
    def GetQueryFrame(self, period):
        return None

    # # Indicates whether this element has been modified in any way that could
    #  affect the save state on disk
    #  @return True if the element has changed
    #  @see _MarkAsUnchanged
    def HasChanged(self):
        return self.__changed

    # # Mark the element for update.
    def SetChanged(self):
        self.__changed = True

    # # Sets if element should be drawn.
    # Useful after a parent has handled the draw for the children.
    def EnableDraw(self, state):
        self.__draw = state

    # # Returns if draw is enabled on this element.
    def ShouldDraw(self):
        return self.__draw

    # # Append YAML content of this element to a YAML event list
    #  @pre Assumes this element can just append a self-contained YAML
    #  block-style map right here (no leading map key/scalar or
    #  sequence start events will be emitted).
    #  @param events Llist of YAML events to which new events should be appended
    #  describing this Element.
    #  @return None
    #
    #  Intended to be called by a Layout during saving
    def _GetYAMLEvents(self):
        events = []
        events.append(yaml.MappingStartEvent(anchor = None, tag = None, implicit = True, flow_style = False))

        # Serialize all properties to yaml pairs
        sorted_keys = sorted(self._ALL_PROPERTIES.keys())
        for k in sorted_keys:
            if k == 'children' and self.HasChildren():
                events.append(yaml.ScalarEvent(anchor = None, tag = None, implicit = (True, True), value = 'children'))
                events.append(yaml.SequenceStartEvent(anchor = None, tag = None, implicit = (True, True)))
                for child in self.GetChildren():
                    events.extend(child._GetYAMLEvents())

                events.append(yaml.SequenceEndEvent())
            else:
                val = str(self._properties[k])
                # Escape input. This must be symmetric with __init__ when loading initial properties
                val = val.replace('"', '\\"').replace('\n', '\\n').replace('\r', '\\r')
                # val = val.replace("'","\\'")

                events.extend([yaml.ScalarEvent(anchor = None, tag = None, implicit = (True, True), value = str(k)),
                               yaml.ScalarEvent(anchor = None, tag = None, implicit = (True, True), value = val),
                               ])

        events.append(yaml.MappingEndEvent())
        return events

    # # Flags this layout and all comprising elements as Not Changed.
    #  This is intended to be called by the owning Layout
    def _MarkAsUnchanged(self):
        self.__changed = False

    # # Will return a hash of the Elements PIN, which is a unique identifier
    #  for each Element (and therefore each Element Value)
    def __hash__(self):
        return hash(self.GetPIN())

    # # Determine if two Element instances are equivalent, as determined by
    #  sharing the same PIN or the other element being None.
    #  Comparison with other types is not allowed
    def __eq__(self, other):
        if other is None:
            return False
        return self.GetPIN() == other.GetPIN()


Element._ALL_PROPERTIES['type'] = (Element.GetType(), valid.validateString)


# # eneric element capable of holding other elements.
class MultiElement(Element):
    _MULTI_PROPERTIES = {'children' : ([], valid.validateList)}
    _ALL_PROPERTIES = Element._ALL_PROPERTIES.copy()
    _ALL_PROPERTIES.update(_MULTI_PROPERTIES)

    @staticmethod
    def HasChildren():
        return True

    def __init__(self, *args, **kwargs):
        Element.__init__(self, *args, **kwargs)
        # store child elements
        self.__children = []
        # map PINs to child elements
        self.__children_by_pin = {}

    @staticmethod
    def GetHiddenProperties():
        # we use pixel_offset instead
        return ['children']

    def GetChildren(self):
        return self.__children

    def GetChildByPIN(self, pin):
        return self.__children_by_pin.get(pin, None)

    def RemoveChild(self, e):
        self.__children.remove(e)
        del self.__children_by_pin[e.GetPIN()]

    def AddChild(self, e):
        self.__children.append(e)
        self.__children_by_pin[e.GetPIN()] = e


# # Element that gets queries from database.
class LocationallyKeyedElement(Element):
    _LOC_KEYED_PROPERTIES = {'LocationString' : ('top' , valid.validateLocation),
                            #  'annotation'    : ('', valid.validateAnnotation), #unused, phase out
                              'Content'       : ('loc' , valid.validateContent), # See content_options.py
                              'auto_color_basis'       : ('' , valid.validateString),
                              'color_basis_type'       : ('string_key' , valid.validateString), # needs better validator eventually
                              'tooltip'       : ('', valid.validateString),
                             }

    _ALL_PROPERTIES = Element._ALL_PROPERTIES.copy()
    _ALL_PROPERTIES.update(_LOC_KEYED_PROPERTIES)
    COLOR_BASIS_TYPES = ['string_key', 'python_exp', 'python_func']

    @staticmethod
    def NeedsDatabase():
        return True

    __brush_cache_needs_purging = False

    def __init__(self, *args, **kwargs):
        Element.__init__(self, *args, **kwargs)
        self.__has_vars = False
        # keep track of state among all LocationallyKeyed Elements

    def SetProperty(self, key, val):
        Element.SetProperty(self, key, val)
        val = self.GetProperty(key)
        if (key == 'LocationString' or key == 't_offset') \
           and (self._layout is not None and self._layout.HasContext()):
            if key == 'LocationString':
                loc = val
                # This allows us to avoid using an expensive regex later on if there aren't any variables in the location string
                if '{' in val:
                    self.__has_vars = True
                t_off = self._properties['t_offset']
            else:
                t_off = val
                loc = self._properties['LocationString']
            self._layout.ReSort(self, t_off, loc)
            self._properties[key] = val
            self._layout.Refresh(self)
        elif (key == 'Content'):
            self._properties[key] = val
            self._layout.Refresh(self)
        elif key == 'auto_color_basis' or key == 'color_basis_type':
            self.__brush_cache_needs_purging = True

    def SetBrushesWerePurged(self):
        self.__brush_cache_needs_purging = False

    def BrushCacheNeedsPurging(self):
        return self.__brush_cache_needs_purging

    # Return if the location string has any variables
    def LocationHasVars(self):
        return self.__has_vars

    # # Debug purposes only
    def __str__(self):
        return '<{} element: loc={} toff={}>' \
               .format(self._properties['type'], self._properties['LocationString'], str(self._properties['t_offset']))

    # # Debug purposes only
    def __repr__(self):
        return self._properties['LocationString'] + ' ' + self._properties['Content']


# # For purposes of consistency, we don't use the base type Element directly.
class BoxElement(LocationallyKeyedElement):

    @staticmethod
    def GetType():
        return 'box'

    @staticmethod
    def IsDrawable():
        return True


BoxElement._ALL_PROPERTIES['type'] = (BoxElement.GetType(), valid.validateString)


# # For purposes of consistency, we don't use the base type Element directly.
class ImageElement(LocationallyKeyedElement):

    # # Manages loaded images
    #  @note This is required as an optimization for undo/redo/scale operations
    #  so that images needn't be reloaded.
    class ImageManager:

        class BitmapOriginalPair:

            def __init__(self, bmp_wref, original_img):
                self.bmp_wref = bmp_wref
                self.original_img = original_img

        def __init__(self):
            self.__originals = weakref.WeakValueDictionary() # {(filename): wx.Image}
            self.__bmps = {} # {(filename,(w,h): (wx.Bitmap, original wx.Image)}

        # A bit map being tracked expired. Free it
        def __OnBmpExpire(self, wref):
            print(wref)
            for k, v in self.__bmps.items():
                if k.bmp_wref == wref:
                    del self.__bmps[k]
                    break

        # # Get an item from the manager, loading if necessary
        #  @param info Info about the item to load including filename and
        #  dimensions as nested tuples: (filename, (w,h))
        def __getitem__(self, info):
            # See if a scaled image matching the info exists already
            bmp_and_img = self.__bmps.get(info)
            if bmp_and_img is not None:
                # #print 'Re-using scaled image {}'.format(info)
                return bmp_and_img.bmp_wref() # Use the existing scaled bitmap. Will expedite copy/paste undo/redo
            else:
                pass # #print 'Could not find scaled image in {}'.format(self.__bmps.keys())

            # Unpack args for new image
            filename, dims = info
            w, h = dims

            # See if a full-size image exists
            img = self.__originals.get(filename)

            if img is None:
                # #print 'Loading original image:'.format(info)
                bmp = wx.Bitmap(filename, wx.BITMAP_TYPE_ANY)
                if bmp.Ok() == False:
                    raise IOError('Bitmap {} could not be loaded'.format(filename))
                img = wx.ImageFromBitmap(bmp)
                self.__originals[filename] = img

            # Scale the new image
            # #print 'Scaling image {}'.format(info)
            img2 = img.Scale(w, h, wx.IMAGE_QUALITY_HIGH)
            bmp = wx.BitmapFromImage(img2)

            # Track in list
            self.__bmps[info] = self.BitmapOriginalPair(weakref.ref(bmp), img)

            # #print 'BMPs:', self.__bmps.keys()
            # #print 'ORIGs: ', self.__originals.keys()

            return bmp

    _IMAGE_PROPERTIES = {'filename': ('logo.png', valid.validateString)}

    _ALL_PROPERTIES = LocationallyKeyedElement._ALL_PROPERTIES.copy()
    _ALL_PROPERTIES.update(_IMAGE_PROPERTIES)
    _ALL_PROPERTIES['dimensions'] = ((135, 135), _ALL_PROPERTIES['dimensions'][1])

    _IMAGE_MANAGER = ImageManager() # Static image manager/loader for ImageElements per process

    @staticmethod
    def GetType():
        return 'image'

    @staticmethod
    def IsDrawable():
        return True

    # # called when drawn is not null
    @classmethod
    def GetDrawRoutine(cls):
        return cls.DrawRoutine

    def __init__(self, *args, **kwargs):
        LocationallyKeyedElement.__init__(self, *args, **kwargs)

        self.__image = None
        self.__loaded_filename = None

        self.ScaleImage(self.GetProperty('filename'), self.GetProperty('dimensions'))

    def DrawRoutine(self,
                    pair,
                    dc,
                    canvas,
                    tick,
                    time_range = None,
                    render_box = None,
                    fixed_offset = None):

        border_color = self.GetProperty('color')
        pen = wx.Pen(border_color, 1) # TODO: Use a pen cache
        dc.SetPen(pen)

        (x, y), (w, h) = self.GetProperty('position'), self.GetProperty('dimensions')
        xoff, yoff = canvas.GetRenderOffsets()
        if not fixed_offset:
            (x, y) = (x - xoff, y - yoff)
        else:
            x = x - fixed_offset[0]
            y = y - fixed_offset[1]

        brush = wx.Brush((0, 0, 0), style = wx.SOLID) # TODO: use a brush cache
        dc.SetBrush(brush)

        # #dc.SetClippingRegion(x, y, w, h)

        dc.SetBackground(wx.Brush('WHITE'))

        if self.__image is not None:
            dc.DrawBitmap(self.__image, x, y)
        else:
            # Draw border and X through box
            dc.DrawRectangle(x, y, w, h)
            dc.DrawLine(x, y, x + w, y + h)
            dc.DrawLine(x + w, y, x, y + h)

        self.UnsetNeedsRedraw()
        # #dc.DestroyClippingRegion()

    def SetProperty(self, key, val):
        LocationallyKeyedElement.SetProperty(self, key, val)

        val = self.GetProperty(key)
        if (key == 'filename'):
            self.ScaleImage(val, self.GetProperty('dimensions'))
        if (key == 'dimensions'):
            self.ScaleImage(self.GetProperty('filename'), val)

    # # Scale image, reloading if necessary
    def ScaleImage(self, filename, dims):
        imgpath = self.__ChooseImage(filename)

        if imgpath is None or not os.path.exists(imgpath):
            print('Image does not exist in known resource dirs: "{}"->"{}". Use -R to specify others' \
                  .format(filename, imgpath))
            ws = self.GetLayout().GetWorkspace()
            print('Resource dirs:')
            if ws is not None:
                for rd in self.__GetResourceDirs():
                    print('  {}'.format(rd))
            else:
                print('  None')

            self.__image = None
            self.__loaded_filename = imgpath
            return

        try:
            self.__image = self._IMAGE_MANAGER[(imgpath, dims)]
        except Exception as ex:
            print('Error loading bitmap: "{}": {}'.format(imgpath, ex))
            self.__images = None

        self.__loaded_filename = imgpath
        return

    def __GetResourceDirs(self):
        dirs = []

        lfn = self.GetLayout().GetFilename()
        # import pdb; pdb.set_trace()
        import traceback
        traceback.print_stack()
        # print self.GetLayout()
        print(lfn)
        if lfn is not None:
            dirs.append(os.path.dirname(lfn)) # Use layout file path

        ws = self.GetLayout().GetWorkspace()
        if ws is not None:
            dirs.extend(ws.GetResourceResolutionList()) # Get resource paths

        return dirs

    # # Determines a path to an image given a relative path to this session's resource directories
    def __ChooseImage(self, filename):
        lfn = self.GetLayout().GetFilename()
        ws = self.GetLayout().GetWorkspace()
        if ws is not None:
            imgpath = ws.LocateResource(filename, try_first = (os.path.dirname(lfn) if lfn is not None else None))
        else:
            imgpath = filename # Maybe we'll get lucky. Shouldn't really be caring about resources
                               # without a workspace though.

        return imgpath


ImageElement._ALL_PROPERTIES['type'] = (ImageElement.GetType(), valid.validateString)


# # For purposes of consistency, we don't use the base type Element directly.
class LogElement(LocationallyKeyedElement):

    _LOG_PROPERTIES = {'filename': ('logo.png', valid.validateString),
                       'regex': ('', valid.validateString),
                       'sub_pattern': ('', valid.validateString),
                       'color_group_pattern': ('', valid.validateString), }

    _ALL_PROPERTIES = {key:LocationallyKeyedElement._ALL_PROPERTIES[key] \
                       for key in LocationallyKeyedElement._ALL_PROPERTIES \
                       if key not in ('auto_color_annotation', 'auto_color_basis', 'on_update', 'on_init', 'on_cycle_changed', 'caption')}
    _ALL_PROPERTIES.update(_LOG_PROPERTIES)
    _ALL_PROPERTIES['dimensions'] = ((200, 200), _ALL_PROPERTIES['dimensions'][1])

    @staticmethod
    def GetType():
        return 'log'

    @staticmethod
    def IsDrawable():
        return True

    # # called when drawn is not null
    @classmethod
    def GetDrawRoutine(cls):
        return cls.DrawRoutine

    def __init__(self, *args, **kwargs):
        LocationallyKeyedElement.__init__(self, *args, **kwargs)

        self.__def_background_color = wx.Colour(255, 255, 255)

        self.__searcher = None # Searcher (logsearcher) for scanning log for ticks
        self.__file = None # File handle for reading log
        self.__line_cache = [] # Log lines for current cycle
        self.__tick = None
        self.__prev_loc = None

        self.LoadLog(self.GetProperty('filename'))

    def DrawRoutine(self,
                    pair,
                    dc,
                    canvas,
                    render_tick,
                    time_range = None,
                    render_box = None,
                    fixed_offset = None):

        border_color = self.GetProperty('color')
        normal_pen = wx.Pen(border_color, 1) # TODO: Use a pen cache
        dc.SetPen(normal_pen)

        (x, y), (w, h) = self.GetProperty('position'), self.GetProperty('dimensions')
        xoff, yoff = canvas.GetRenderOffsets()
        if not fixed_offset:
            (x, y) = (x - xoff, y - yoff)
        else:
            x = x - fixed_offset[0]
            y = y - fixed_offset[1]

        period = pair.GetClockPeriod()
        t_offset = self.GetProperty('t_offset')

        dc.SetClippingRegion(x, y, w, h)

        filename = self.GetProperty('filename')
        if not self.__searcher:
            brush = wx.Brush((200, 200, 200), style = wx.SOLID) # TODO: use a brush cache
            dc.SetBrush(brush)

            dc.SetBackground(wx.Brush(self.__def_background_color))

            # Draw border and X through box
            dc.DrawRectangle(x, y, w, h)

            # Draw x through box
            dc.DrawLine(x, y, x + w, y + h)
            dc.DrawLine(x + w, y, x, y + h)
            dc.DrawText('No such file:\n' + filename, x, y)
        elif t_offset != 0 and period == -1:
            brush = wx.Brush((200, 200, 200), style = wx.SOLID) # TODO: use a brush cache
            dc.SetBrush(brush)

            dc.SetBackground(wx.Brush(self.__def_background_color))

            # Draw border and X through box
            dc.DrawRectangle(x, y, w, h)

            # Draw x through box
            dc.DrawLine(x, y, x + w, y + h)
            dc.DrawLine(x + w, y, x, y + h)
            dc.DrawText('t_offset is nonzero ({}) but this element\n' \
                        'element is not associated with a database\n' \
                        'location which has a clock. Change location\n' \
                        '"{}"\n' \
                        'to something else so that this element can\n' \
                        'compute the offset in that location\'s clock\n' \
                        'cycles. Or change t_offset to 0\n' \
                        .format(t_offset, self.GetProperty('LocationString')),
                        x, y)
        else:
            regex_str = self.GetProperty('regex')
            sub_pat_str = self.GetProperty('sub_pattern')
            color_pat_str = self.GetProperty('color_group_pattern')
            if regex_str == '':
                expr = None
            else:
                expr = re.compile(regex_str)
            brush = wx.Brush(border_color, style = wx.TRANSPARENT) # TODO: use a brush cache
            dc.SetBrush(brush)
            dc.DrawRectangle(x, y, w, h)

            tick = render_tick + (t_offset * period)

            if self.__tick != tick or self.__line_cache is None:
                if self.__tick is None or tick > self.__tick:
                    after_last = True
                else:
                    after_last = False
                self.__tick = tick
                self.__line_cache = []

                if self.__prev_loc is None or self.__prev_loc == self.__searcher.BAD_LOCATION:
                    self.__prev_loc = 0 # Reset for a new search
                self.__prev_loc = self.__searcher.getLocationByTick(self.__tick, self.__prev_loc if after_last else 0)
                if self.__prev_loc == self.__searcher.BAD_LOCATION:
                    pass
                    # Nothing in the log for this tick
                else:
                    self.__file.seek(self.__prev_loc)
                    while 1:
                        t = self.__file.readline()
                        if t.startswith('{') == False:
                            break # Not in the middle of the line
                        try:
                            tick_str = t[1:t.find(' ')]
                            line_tick = int(tick_str)
                        except:
                            break # Unable to parse SPARTA line prefix
                        if line_tick > self.__tick:
                            break # Reached the next tick

                        if expr is None:
                            self.__line_cache.append((t, self.__def_background_color))
                        else:
                            m = expr.search(t)
                            if m is not None:
                                # Determine Line Color
                                if color_pat_str == '':
                                    color = self.__def_background_color
                                else:
                                    try:
                                        col_str = re.sub(expr, color_pat_str, t)
                                    except sre_constants.error as ex:
                                        print('Unable to generate color value from regex groups using "{}". Error={}'.format(color_pat_str, ex))
                                        color = self.__def_background_color
                                    else:
                                        try:
                                            idx = int(col_str) % len(gui.autocoloring.BACKGROUND_BRUSHES)
                                        except ValueError:
                                            try:
                                                idx = int(col_str, 16) % len(gui.autocoloring.BACKGROUND_BRUSHES)
                                            except ValueError:
                                                idx = hash(col_str) % len(gui.autocoloring.BACKGROUND_BRUSHES)

                                        color = gui.autocoloring.BACKGROUND_BRUSHES[idx].GetColour()

                                # Determine Content
                                if sub_pat_str == '': # Do no replacement
                                    self.__line_cache.append((t, color))
                                else:
                                    try:
                                        t = re.sub(expr, sub_pat_str, t)
                                    except sre_constants.error as ex:
                                        print('Unable to generate displayed string from regex groups using "{}". Error={}'.format(color_pat_str, ex))

                                    self.__line_cache.append((t, color))

            lx = x + 1
            ly = y + 1
            dc.SetBackgroundMode(wx.SOLID)
            for t, color in self.__line_cache:
                dc.DrawTextList([t.rstrip()], [(lx, ly)], backgrounds = color)
                ly += 12 # Figure out line height

        dc.DestroyClippingRegion()
        self.UnsetNeedsRedraw()

    def SetProperty(self, key, val):
        LocationallyKeyedElement.SetProperty(self, key, val)

        val = self.GetProperty(key)
        if (key == 'filename'):
            self.LoadLog(val)
        elif (key == 'regex'):
            self.__line_cache = None # Force reloading
        elif key == 'sub_pattern':
            self.__line_cache = None # Force reloading
        elif key == 'color_group_pattern':
            self.__line_cache = None # Force reloading

    def LoadLog(self, filename):
        if os.path.exists(filename):
            self.__searcher = logsearch.LogSearch(filename)
            self.__file = open(filename, 'r')
        else:
            self.__searcher = None
            self.__file = None

        # Clear line cache and force reload next time
        self.__tick = None
        self.__line_cache = None
        self.__prev_loc = 0


LogElement._ALL_PROPERTIES['type'] = (LogElement.GetType(), valid.validateString)


# # Class which only is used to identify transactions.
# Used for collision events for mouse-over.
class FakeElement:

    def __init__(self):
        self._properties = {}

    @staticmethod
    def IsSelectable():
        return True

    def GetProperty(self, key, period = None):
        # try...except is faster than dict.get()
        try:
            return self._properties[key]
        except:
            return None

    def SetProperty(self, key, value):
        self._properties[key] = value

    # # Does this element have a particular property
    def HasProperty(self, key):
        return key in self._properties


# # For same purpose as fake element, just plays a different role.
class FakeElementValue:

    def __init__(self, element, value):
        self.__element = element
        self.__value = value
        self.__clock_period = 1

    def GetVal(self):
        return self.__value

    def GetElement(self):
        return self.__element

    def GetClockPeriod(self):
        return self.__clock_period

    def SetClockPeriod(self, period):
        self.__clock_period = period

