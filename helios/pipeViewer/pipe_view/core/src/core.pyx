# # @package PipeViewer drawing loop and support routines

import sys
import wx
import re
import math
import colorsys
from logging import debug, error, info

# Color expression namespace
EXPR_NAMESPACE = {'re':re, 'colorsys':colorsys, 'math':math, 'extract':extract_value}

from common cimport *
from libc.stdlib cimport strtoul
from libcpp.unordered_map cimport unordered_map
from cpython.ref cimport PyObject

cimport cython

from libcpp cimport bool

cdef extern from "wx/defs.h":

    ctypedef int wxCoord

ctypedef unsigned char ChannelType

cdef extern from "wx/colour.h":

    cdef cppclass wxColour:
        wxColour()

        wxColour(ChannelType red,
                 ChannelType green,
                 ChannelType blue)
        wxColour(unsigned long colRGB)
        int GetRGB()

cdef extern from "wx/bitmap.h":

    cdef cppclass wxBitmap:
        wxBitmap()

cdef extern from "wx/pen.h":

    cdef cppclass wxPen:
        wxPen()
        wxPen(wxColour colour, # const &
              int width) # =1

        void SetColour(wxColour colour) # const &

    ctypedef wxPen const_wxPen "wxPen const"

cdef extern from "wx/brush.h":

    cdef cppclass wxBrush:
        wxBrush()
        wxBrush(wxColour colour) # const &

        void setColour(wxColour col) # const &

    ctypedef wxBrush const_wxBrush "wxBrush const"

cdef extern from "wx/gdicmn.h":

    cdef cppclass wxSize:
        wxSize()
        wxSize(int xx, int yy)

    cdef cppclass wxPoint:
        wxPoint()
        wxPoint(int xx, int yy)

    cdef cppclass wxRect:
        wxRect()
        wxRect(int xx, int yy, int ww, int hh)

    cdef cppclass wxRect:
        wxRect(wxPoint topLeft, # const &
               wxPoint bottomRight) # const &
        wxRect(wxPoint pt, # const &
               wxSize size) # const &

    cdef cppclass wxRegion:
        wxRegion(wxCoord x, wxCoord y, wxCoord w, wxCoord h)
        wxRegion(wxPoint topLeft, # const &
                 wxPoint bottomRight) # const &
        wxRegion(wxRect) # const &

cdef extern from "wx/wxchar.h":

    ctypedef unsigned char wxChar

cdef extern from "wx/string.h":

    cdef cppclass wxString:
        wxString()
        wxString(wxString) # const &
        wxString(const char *) # const
        const char * ToAscii() # const

cdef extern from "wx/font.h":

    cdef cppclass wxFont:
        wxFont()
        wxFont(const wxFont & font)

        bint IsFixedWidth() # const
        wxFont MakeBold()

    ctypedef wxFont const_wxFont "wxFont const"

    cdef enum wxFontWeight:
        wxFONTWEIGHT_NORMAL,
        wxFONTWEIGHT_LIGHT,
        wxFONTWEIGHT_BOLD,
        wxFONTWEIGHT_MAX

cdef extern from "wx/dcgraph.h":

    cdef cppclass wxGCDC:
        void SetFont(wxFont font) # const &
        void SetPen(wxPen pen) # const &
        void SetBrush(wxBrush brush) # const &
        void SetBackground(wxBrush brush) # const &

        const_wxFont GetFont() # const

        const_wxBrush GetBackground() # const method

        const_wxPen GetPen() # const method

        void DrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height)

        void SetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height)

        void SetClippingRegion(wxPoint pt, # const &
                               wxSize sz) # const &
        void SetClippingRegion(wxRect rect) # const &

        void SetClippingRegion(wxRegion region) # const &

        void DestroyClippingRegion()

        void GetTextExtent(wxString text,
                           long * x, long * y)
                           # long *descent = NULL,
                           # long *externalLeading = NULL,
                           # wxFont *theFont = NULL) # const

        void DrawBitmap(wxBitmap bmp, # const &
                        wxCoord x,
                        wxCoord y,
                        bint useMask = false)
        void DrawBitmap(wxBitmap bmp, # const &
                        wxPoint pt, # const &
                        bint useMask = false)

        void DrawText(wxString text, # const &
                      wxCoord x,
                      wxCoord y)
        void DrawText(wxString text, # const &
                      wxPoint pt) # const &

        bint Blit(wxCoord xdest,
                  wxCoord ydest,
                  wxCoord width,
                  wxCoord height,
                  wxGCDC * source,
                  wxCoord xsrc,
                  wxCoord ysrc) # Truncated argument list


cdef extern from "helpers.h":
    bool wxPyConvertWrappedPtr(PyObject* obj, void **ptr, const wxString& className)
    wxGCDC* getDC_wrapped(PyObject* dc) except +
    wxFont* getFont_wrapped(PyObject* font) except +
    wxBrush* getBrush_wrapped(PyObject* brush) except +
    void getTextExtent(wxGCDC* dc, long* char_width, long* char_height)

def get_argos_version():
    return 1;

# # Extracts a value from a string \a s by its \a key. Key is separated from
#  value by 0 or more spaces, then a character in \a separators, followed by 0 or
#  more additional spaces. After extracing the value for the given key, the
#  first \a skip_chars characters are dropped form the result
#  @return Value of first match if there are any matches. Otherwise returns \a not_found
cpdef str extract_value(str s, str key, str separators = '=:', long skip_chars = 0, not_found = ''):
    extractor = re.compile(r'{}\s*[{}]\s*([^ ]*)'.format(key, separators))
    matches = extractor.findall(s)
    if len(matches) == 0:
        return not_found
    return matches[0][skip_chars:]

cdef wxGCDC* getDC(dc):
    return getDC_wrapped(<PyObject*>dc)

cdef wxFont* getFont(font):
    return getFont_wrapped(<PyObject*>font)

cdef wxBrush* getBrush(brush):
    return getBrush_wrapped(<PyObject*>brush)

cdef class Renderer(object):

    cdef unordered_map[int, wxPen] c_pens_map
    cdef dict __reason_brushes
    cdef dict __background_brushes
    cdef object __extensions
    cdef wxFont c_font
    cdef wxFont c_bold_font
    cdef long c_char_width
    cdef long c_char_height

    # Brian Grayson prefers it to be a long time (>500 uops) before we repeat
    # the same combination of color and symbol.  So it's probably best for these to
    # be relatively prime.  Least common multiple should be much larger than 300.
    cdef int NUM_REASON_COLORS
    cdef int NUM_ANNOTATION_COLORS
    cdef int NUM_ANNOTATION_SYMBOLS
    cdef char * ANNOTATION_SYMBOL_STRING
    cdef wxPen HIGHLIGHTED_PEN
    cdef wxPen SEARCH_PEN
    cdef wxPen HIGHLIGHTED_SEARCH_PEN

    def __cinit__(self, *args):
        self.NUM_REASON_COLORS = 16
        self.NUM_ANNOTATION_COLORS = 32
        self.NUM_ANNOTATION_SYMBOLS = 52
        self.ANNOTATION_SYMBOL_STRING = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
        self.HIGHLIGHTED_PEN = wxPen(wxColour(255, 0, 0), 2)
        self.SEARCH_PEN = wxPen(wxColour(0, 0, 0), 2)
        self.HIGHLIGHTED_SEARCH_PEN = wxPen(wxColour(0, 0, 255), 2)

    def __init__(self, *args):
        pass

    def __dealloc__(self):
        pass

    def __str__(self):
        return '<pipeViewer Optimized Renderer>'

    def __repr__(self):
        return self.__str__()

    def setBrushes(self, reason_brushes, background_brushes):
        self.__reason_brushes = reason_brushes
        self.__background_brushes = background_brushes

    def setExtensions(self, extensions):
        self.__extensions = extensions

    def __fieldStringColorization(self, string_to_display, field_string):
        # extract what to base color on
        start_idx = string_to_display.find(field_string)
        if start_idx != -1:
            start_idx += len(field_string)
            if start_idx < len(string_to_display) and string_to_display[start_idx] == '{':
                # use everything inside brackets
                start_idx += 1
                next_open = string_to_display.find('{', start_idx)
                next_close = string_to_display.find('}', start_idx)
                # while there is an open at a lower position than a close,
                # skip the close and open (inside pair)
                while next_open < next_close:
                    next_open = string_to_display.find('{', next_open + 1)
                    next_close = string_to_display.find('}', next_close + 1)
                field_value = string_to_display[start_idx:next_close]
            else:
                next_idxs = filter(lambda x: x >= 0, (string_to_display.find(',', start_idx + 1), \
                                                     string_to_display.find(' ', start_idx + 1), \
                                                     string_to_display.find('\n', start_idx + 1)))
                if not next_idxs:
                    field_value = string_to_display[start_idx:]
                else:
                    next_idx = min(next_idxs)
                    field_value = string_to_display[start_idx:next_idx]
            try:
                field_num = int(field_value, 16)
            except ValueError:
                field_num = hash(field_value)
            string_to_display = chr(self.ANNOTATION_SYMBOL_STRING[field_num % self.NUM_ANNOTATION_SYMBOLS])
            background_idx = field_num % self.NUM_ANNOTATION_COLORS
            return string_to_display, self.__background_brushes[background_idx]
        elif string_to_display: # not empty, just can't find
            return '#', None
        else: # empty string
            return string_to_display, None

    # # @brief Sets color of background brush and parses annotation according to type.
    #
    def parseAnnotationAndGetColor(self, string_to_display, content_type, field_type = None, field_string = ''):
        cdef char * c_seq_id_str
        cdef unsigned long int c_seq_id
        cdef char * c_endptr

        brush = None

        #--------------------------------------------------
        # Choose brush color
        # - uop seq ID is first three hex digits
        #
        string_to_display = str(string_to_display)
        if any([content_type == 'auto_color_annotation',
                content_type == 'auto_color_anno_notext',
                content_type == 'auto_color_anno_nomunge']):

            if field_string:
                if field_type == 'string_key' or field_type is None:
                    if content_type == 'auto_color_anno_nomunge':
                        # Preserve current annotation
                        _, brush = self.__fieldStringColorization(string_to_display, field_string)
                    elif content_type == 'auto_color_anno_notext':
                        string_to_display = '' # No text displayed
                        _, brush = self.__fieldStringColorization(string_to_display, field_string)
                    else: # normal annotation mode (display encoded annotation)
                        string_to_display, brush = self.__fieldStringColorization(string_to_display, field_string)

                elif field_type == 'python_exp':
                    try:
                        # Evaluate the user expression
                        info_tuple = eval(field_string, {'anno': string_to_display}, EXPR_NAMESPACE)
                        brush = wx.TheBrushList.FindOrCreateBrush(info_tuple[:3], wx.SOLID) # no guarantees this is fast
                        if len(info_tuple) > 3:
                            string_to_display = str(info_tuple[3])
                        else:
                            pass # Preserve current display string
                    except:
                        error('Error: expression "{}"" raised exception on input "{}":'.format(field_string, string_to_display))
                        error(sys.exc_info())
                        string_to_display = '!'
                elif field_type == 'python_func':
                    func = self.__extensions.GetFunction(field_string)
                    if func:
                        try:
                            info_tuple = func(string_to_display)
                            brush = wx.TheBrushList.FindOrCreateBrush(info_tuple[:3], wx.SOLID)
                            if len(info_tuple) > 3:
                                string_to_display = str(info_tuple[3])
                            else:
                                pass # Preserve current display string
                        except:
                            error('Error: function "{}"" raised exception on input "{}":'.format(field_string, string_to_display))
                            error(sys.exc_info())
                            string_to_display = '!'
                    else:
                        error('Error: function "{}" can not be loaded.'.format(field_string))
                if brush is None:
                    brush = wx.TheBrushList.FindOrCreateBrush(wx.WHITE, wx.SOLID)
                return string_to_display, brush
            else:
                seq_id_str = string_to_display[:3].strip()
                if (len(seq_id_str) >= 3 and seq_id_str[0] == 'R'):
                    #--------------------------------------------------
                    # This is a "reason" instead of a "uop"
                    #
                    seq_id_str = seq_id_str[1:] # Ignore 'R'
                    byte_str = seq_id_str.encode('UTF-8')
                    c_seq_id_str = byte_str
                    c_seq_id = strtoul(c_seq_id_str, & c_endptr, 16) # hex

                    if c_endptr == c_seq_id_str + len(seq_id_str):
                        #--------------------------------------------------
                        # We found a valid seq id, so use the new colorization method
                        #

                        if (content_type != 'auto_color_anno_nomunge'):
                            string_to_display = string_to_display[2:]
                        return string_to_display, self.__reason_brushes[c_seq_id % self.NUM_REASON_COLORS]
                elif seq_id_str:
                    #--------------------------------------------------
                    # This is not a "reason" and may be a "uop"
                    #
                    byte_str = seq_id_str.encode('UTF-8')
                    c_seq_id_str = byte_str
                    c_seq_id = strtoul(c_seq_id_str, & c_endptr, 16) # hex

                    if c_endptr == c_seq_id_str + len(seq_id_str):
                        #--------------------------------------------------
                        # We found a valid seq id, so use the new colorization method
                        #
                        if (content_type != 'auto_color_anno_nomunge'):
                            c_symbol = self.ANNOTATION_SYMBOL_STRING[c_seq_id % self.NUM_ANNOTATION_SYMBOLS]
                            string_to_display = chr(c_symbol) + string_to_display[3:]
                        return string_to_display, self.__background_brushes[c_seq_id % self.NUM_ANNOTATION_COLORS]
        else:
            if all([content_type == 'caption',
                    len(string_to_display) >= 3,
                    string_to_display[:2] == 'C=']):
                byte_str = string_to_display[2].encode('UTF-8')
                c_seq_id_str = byte_str
                c_seq_id = strtoul(c_seq_id_str, & c_endptr, 16) # hex

                if c_endptr != c_seq_id_str:
                    #--------------------------------------------------
                    # We found a valid seq id, so use the new colorization method
                    #

                    string_to_display = string_to_display[4:]
                    return string_to_display, self.__reason_brushes[c_seq_id % self.NUM_REASON_COLORS]
        return string_to_display, wx.TheBrushList.FindOrCreateBrush(wx.WHITE, wx.SOLID)

    def setFontFromDC(self, dc):
        self.c_font = getFont(dc.GetFont())[0]
        self.c_bold_font = wxFont(self.c_font)
        self.c_bold_font.MakeBold()

        if self.c_font.IsFixedWidth():
            getTextExtent(getDC(dc), &self.c_char_width, &self.c_char_height)

    def drawInfoRectangle(self,
                          tick,
                          element,
                          dc,
                          canvas,
                          rect,
                          annotation,
                          missing_needed_loc,
                          content_type,
                          auto_color, # type, basis
                          clip_x, # (start, width)
                          schedule_settings = None,
                          short_format = ''):
                          # schedule_settings: (period_width, 0/1/2 (none/dots/boxed))
        cdef wxGCDC * c_dc = getDC(dc)

        cdef int c_x
        cdef int c_y
        cdef int c_w
        cdef int c_h
        cdef int x_offs

        cdef char * c_str

        cdef char c_tmp_char

        cdef char c_symbol

        cdef int c_x_adj
        cdef int c_y_adj

        cdef int c_num_chars
        cdef int c_content_str_len
        cdef wxPen old_pen

        x_offs = 0

        c_x, c_y, c_w, c_h = rect

        if missing_needed_loc:
            # Missing location but required one to display. Show with grey hatched background
            string_to_display = ''
            brush = wx.TheBrushList.FindOrCreateBrush((160, 160, 160), wx.CROSSDIAG_HATCH) # no guarantees this is fast
            highlighted = False
            search_result = False
        else:
            record = canvas.GetTransactionColor(annotation, content_type, auto_color[0], auto_color[1])
            if record:
                string_to_display, brush, highlighted, search_result, _, _ = record
            else:
                string_to_display, brush, highlighted, search_result = canvas.AddColoredTransaction(annotation, content_type, auto_color[0], auto_color[1], tick, element)

        if highlighted or search_result:
            old_pen = c_dc.GetPen()
            c_dc.SetFont(self.c_bold_font)
            c_w -= 1
            c_h -= 1
            c_x += 1
            c_y += 1
        if highlighted and search_result:
            c_dc.SetPen(self.HIGHLIGHTED_SEARCH_PEN)
        elif search_result:
            c_dc.SetPen(self.SEARCH_PEN)
        elif highlighted:
            c_dc.SetPen(self.HIGHLIGHTED_PEN)

        # Graph C pointer to brush
        c_dc.SetBrush(getBrush(brush)[0])

        if content_type == 'image':
            # Draw an image
            #dc.DrawBitmap(image, c_x, c_y)
            pass
        else: # auto
            # Draw text clipped to this element

            # Parameters to easily shift the text within a cell.
            c_y_adj = 0
            c_x_adj = 1
            # schedule line drawing code: strict cutting of over-flowing elements
            if schedule_settings:
                # clip long elements
                # don't worry about rendering at -20 or less left
                if c_x + 30 < clip_x[0]:
                    x_offs = c_x - clip_x[0]
                    c_w = c_w + x_offs # can only use upper range
                    c_x = clip_x[0]

                if c_w > clip_x[1]:
                    c_w = clip_x[1] + 30

                # FILL rectangle
                c_dc.DrawRectangle(c_x, c_y, c_w, c_h)

                period_width, div_type = schedule_settings
                if content_type != 'auto_color_anno_notext' and \
                   string_to_display and \
                   period_width >= self.c_char_width:

                    number_of_divs = c_w / period_width

                    # draw text
                    if div_type == 10 or short_format == 'multi_char': # RULER
                        byte_str = string_to_display.encode('UTF-8')
                        c_str = byte_str
                        if 0 <= -x_offs < self.c_char_width * len(string_to_display):
                            c_dc.DrawText(wxString(c_str), < wxCoord > c_x + c_x_adj + x_offs, < wxCoord > c_y + c_y_adj)
                    else:
                        byte_str = string_to_display[0].encode('UTF-8')
                        c_str = byte_str
                        if 0 <= -x_offs < self.c_char_width:
                            c_dc.DrawText(wxString(c_str), < wxCoord > c_x + c_x_adj + x_offs, < wxCoord > c_y + c_y_adj)

                    end_coord = c_x + c_w - period_width / 2.0
                    if div_type == 1:
                        y_coord = c_y + c_y_adj + 5
                        curr_x = c_x + c_x_adj + x_offs - 1 + period_width * 1.5
                        while curr_x < end_coord:
                            c_dc.DrawRectangle(< wxCoord > curr_x, < wxCoord > y_coord, 2, 2)
                            curr_x += period_width
                    elif div_type == 2:
                        y_coord = c_y + c_y_adj
                        curr_x = c_x + c_x_adj + x_offs
                        if x_offs == 0:
                            curr_x += period_width
                        while curr_x < end_coord:
                            c_dc.DrawText(wxString(c_str), < wxCoord > (curr_x), < wxCoord > (y_coord))
                            curr_x += period_width
            else:
                # regular draw
                c_dc.DrawRectangle(c_x, c_y, c_w, c_h)
                if content_type != 'auto_color_anno_notext':
                    string_to_display = self.__truncateString(string_to_display, c_w)
                    byte_str = string_to_display.encode('UTF-8')
                    c_str = byte_str
                    if len(string_to_display):
                        c_dc.DrawText(wxString(c_str), < wxCoord > c_x + c_x_adj, < wxCoord > c_y + c_y_adj)
        if highlighted or search_result:
            c_dc.SetPen(old_pen)
            c_dc.SetFont(self.c_font)

    def __truncateString(self, string_to_display, c_w):
        # Truncate text if possible
        if self.c_char_width:
            num_chars = int(1 + (c_w / self.c_char_width))
            str_len = len(string_to_display)
            if num_chars < str_len:
                return string_to_display[:num_chars]
            return string_to_display
        return string_to_display

    def drawElements(self, dc, canvas, tick):
        """
        Draw elements in the list to the given dc
        """

        elements = canvas.GetDrawPairs() # Uses bounds
        xoff, yoff = canvas.GetRenderOffsets()
        _, reason_brushes = canvas.GetBrushes()

        cdef wxGCDC * c_dc = getDC(dc)

        cdef wxColour c_color
        cdef wxPen c_pen
        cdef int c_x
        cdef int c_y
        cdef int c_w
        cdef int c_h
        cdef int c_yoff = yoff
        cdef int c_xoff = xoff
        cdef int color_rgb
        cdef int64_t c_vis_tick = canvas.GetVisibilityTick()

        needs_brush_purge = False
        for pair in elements:
            if pair.GetVisibilityTick() != c_vis_tick:
                continue # Skip drawing - not visible (probably not on screen)

            string_to_display = pair.GetVal()
            e = pair.GetElement()

            # hack: probably should be handled during update calls
            if e.NeedsDatabase() and e.BrushCacheNeedsPurging():
                e.SetBrushesWerePurged()
                needs_brush_purge = True

            if not e.ShouldDraw():
                continue

            dr = e.GetDrawRoutine()
            if dr:
                # Custom routine.
                dr(e, pair, dc, canvas, tick)
                continue

            (c_x, c_y), (c_w, c_h) = e.GetProperty('position'), e.GetProperty('dimensions')
            (c_x, c_y) = (c_x - c_xoff, c_y - c_yoff)

            color = e.GetProperty('color')
            content_type = e.GetProperty('Content')

            c_color = wxColour(color[0], color[1], color[2])

            color_rgb = c_color.GetRGB()
            if self.c_pens_map.count(color_rgb):
                c_dc.SetPen(self.c_pens_map[color_rgb])
            else:
                c_pen = wxPen(c_color, 1)
                self.c_pens_map[color_rgb] = c_pen
                c_dc.SetPen(c_pen)

            c_dc.SetClippingRegion(c_x, c_y, c_w, c_h)
            self.drawInfoRectangle(tick, e, dc, canvas,
                                   (c_x, c_y, c_w, c_h),
                                   string_to_display,
                                   pair.IsMissingLocation(),
                                   content_type,
                                   (e.GetProperty('color_basis_type'), e.GetProperty('auto_color_basis')),
                                   (c_x, c_w))
            c_dc.DestroyClippingRegion()

        if needs_brush_purge:
            canvas.PurgeBrushCache() # will take care of new render call
