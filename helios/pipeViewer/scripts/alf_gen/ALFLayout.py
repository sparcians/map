'''ALFLayout Generation Tool

 This python library is used to generate an ALF view used with Argos
 pipeout viewer by using the locations file from a generated pipeout
 of a simulator.

 To use Argos, you do not need to use this library to generate ALF
 views; the Argos tool is its own editor.  However, to generate
 really complicated views, this library/utility comes in handy.

 To view the documentation on this library, run the following
 command:

   pydoc ALFLayout

An example usage can be found in the CoreExample in
map/sparta/example/CoreModel/gen_layouts.py

'''
import sys, re
import pdb
import math

def sort_list_canotical(input_list):
    def atoi(text):
        return int(text) if text.isdigit() else text
    def natural_keys(text):
        return [ atoi(c) for c in re.split('(\d+)',text) ]
    input_list.sort(key=natural_keys)

# Internal utility function
def generate_mini_caption_name(disp_name_map, location_rexp, matched_locs, mini_cnt):
    caption_name = "unknown"
    range_str = f'{len(matched_locs)-1}-{len(matched_locs) - mini_cnt}'
    if len(disp_name_map) == 0:
        caption_name = f"{loc.split('.')[-1]}[{range_str}]"
    else:
        _, n_rep = re.subn(location_rexp, disp_name_map[0], matched_locs[0])
        if n_rep == 0:
            caption_name = f'{disp_name_map[0]}[{range_str}]'
        else:
            caption_name = disp_name_map[0].replace('\\1', f'{range_str}')

    return caption_name

# Internal utility function
def generate_caption_name(disp_name_map, location_rexp, loc, loc_idx):
    caption_name = "unknown"

    if len(disp_name_map) == 0:
        caption_name = loc.split('.')[-1]
        return caption_name

    if len(disp_name_map) == 1 or loc_idx >= len(disp_name_map):
        caption_name, n_rep = re.subn(location_rexp, disp_name_map[0], loc)
        if n_rep == 0:
            print(f"WARNING! Could not replace {disp_name_map[0]} in string {loc}.  Using {disp_name_map[0]} as is")
            caption_name = loc.split('.')[-1]
    else:
        caption_name = re.sub(location_rexp, disp_name_map[loc_idx], loc)

    return caption_name

# Internal utility class
class Content:
    def __init__(self, content, color, dimensions, position, loc='', t_offset=None):
        self._content    = content
        self._loc        = loc
        self._color      = color
        self._dimensions = dimensions
        self._position   = position
        self._t_offset   = t_offset

    def output_to_alf(self, alf_file, spacing=''):
        print(f'''{spacing}- Content: {self._content}
{spacing}  color:      ({','.join([str(x) for x in self._color])})
{spacing}  dimensions: ({','.join([str(x) for x in self._dimensions])})
{spacing}  position:   ({','.join([str(x) for x in self._position])})''', file=alf_file)
        if len(self._loc) > 0:
            print(f'{spacing}  LocationString:  {self._loc}', file=alf_file)
        if self._t_offset is not None:
            print(f'{spacing}  t_offset:   {self._t_offset}', file=alf_file)


class ALFLayout:
    r'''ALFLayout class is the top-level class to create a new ALF file and
    "draw" components to be shown on an Argos view.  Use starts
    with creating an ALFLayout object and populating it with
    components (usually aligned with a location.dat file from a
    pipeout).

    Example usage:

        my_layout = ALFLayout(start_time    = -10,   # Start back in time 10 cycles
                              num_cycles    = num_cycles,
                              clock_scale   = 500,
                              location_file = 'my_pipeout_location.dat',
                              alf_file      = 'my_layout.alf')

        sl_grp = layout.createScheduleLineGroup(default_color=[192,192,192],
                                                include_detail_column = True,
                                                margins=ALFLayout.Margin(top = 2, left = 10))

        # We have these locations in the locations.dat file:
        # top.cpu.alu0.pipe0
        # top.cpu.alu0.pipe1
        # top.cpu.alu0.pipe2
        # top.cpu.alu1.pipe0
        # top.cpu.alu1.pipe1
        # top.cpu.alu1.pipe2
        sl_grp.addScheduleLine('.*alu([0-1]+).*', [r'ALU \1 Stage RR',
                                                   r'ALU \1 Stage EX',
                                                   r'ALU \1 Stage WB'], space=True)

        # Add a graphic that shows the cycles
        sl_grp.addCycleLegend(ALFLayout.CycleLegend(location = 'cycle', interval = 5))
    '''

    # Open and parse the location file grabbing the location strings.
    def __parse_location_file(self, location_file : str):
        try:
            loc_file = open(location_file, 'r')
        except:
            print("ERROR: Issues with location file: ", location_file)
            exit(1)

        # Skip the first line
        loc_file.readline()

        self._loc_strs = []
        for line in loc_file:
            (_, loc_str, _) = line.split(',')
            # Weed out the named variables with [] -- they are dups
            if '[' not in loc_str:
                self._loc_strs.append(loc_str)
        # Remove dups
        self._loc_strs = list(set(self._loc_strs))

    class Spacing:
        def __init__(self,
                     height         : int,
                     height_spacing : int,
                     melem_height   : int,
                     melem_spacing  : int,
                     caption_width  : int,
                     onechar_width  : int = 9):
            self._height = height
            self._height_spacing = height_spacing
            self._melem_height   = int(melem_height)
            self._melem_spacing  = int(melem_spacing)
            self._caption_width  = caption_width
            self._onechar_width  = onechar_width

        @property
        def height_spacing(self):
            return self._height_spacing

        @property
        def melem_height(self):
            return self._melem_height

        @property
        def height(self):
            return self._height

        @property
        def caption_width(self):
            return self._caption_width

        @property
        def onechar_width(self):
            return self._onechar_width

    class Margin:
        '''Margin top and left starting point for a ScheduleLineGroup or ColumnView'''
        def __init__(self,
                     top     = 2,
                     left    = 10):
            self._top     = top
            self._left    = left

        @property
        def left(self):
            return self._left

        @property
        def top(self):
            return self._top

    class CycleLegend:
        def __init__(self,
                     location,
                     interval = 5):
            self._location = location
            self._interval = interval

        @property
        def location(self):
            return self._location

        @property
        def interval(self):
            return self._interval

    class Caption(Content):
        def __init__(self, name, color, dimensions, position):
            Content.__init__(self,
                             content='caption',
                             color=color,
                             dimensions=dimensions,
                             position=position)
            self._name = name

        def output_to_alf(self, alf_file):
            Content.output_to_alf(self, alf_file)
            print(f'  caption:    {self._name}', file=alf_file)

    class Cycle(Content):
        def __init__(self, loc, color, dimensions, position, t_offset=0):
            Content.__init__(self,
                             content='cycle',
                             color=color,
                             dimensions=dimensions,
                             position=position,
                             loc=loc, t_offset=t_offset)

        def output_to_alf(self, alf_file):
            Content.output_to_alf(self, alf_file)

    ################################################################################
    # Schedule line
    class ScheduleLineGroup:
        r'''A ScheduleLineGroup allows a user to add ScheduleLine components
        that represent a location in a pipeout.  Intended usage:

            sl_group = my_layout.createScheduleLineGroup(default_color=[192,192,192],
                                                         include_detail_column = True,
                                                         margins=ALFLayout.Margin(top = 2, left = 10))
            sl_group.addScheduleLine('.*alu([0-9]+).*', [r'alu[\1]'])
        '''
        class ScheduleLine(Content):
            def __init__(self,
                         loc_str : str,
                         content : str = 'auto_color_annotation',
                         color   : list = [192,192,192],
                         dimen   : list = [0,0],
                         pos     : list = [0,0],
                         t_offset: int = 0):
                Content.__init__(self,
                                 content=content,
                                 color=color,
                                 dimensions=dimen,
                                 position=pos,
                                 loc=loc_str,
                                 t_offset=t_offset)

            def output_to_alf(self, alf_file):
                Content.output_to_alf(self, alf_file, spacing='  ')
                print(f'''    type:           schedule_line''', file=alf_file)

        def __init__(self,
                     clock_scale  : int,
                     num_cycles   : int,
                     locations    : list,
                     def_color    : list,
                     spacing,
                     margins,
                     include_detail_column : bool,
                     content_width : int,
                     time_offset  : int):
            self._num_cycles   = num_cycles
            self._locations    = locations
            self._def_color    = def_color
            self._spacing      = spacing
            self._margins      = margins
            self._pos          = [self._margins.left + self._spacing.caption_width - 1,
                                  self._margins.top]
            self._pixel_offset = -(time_offset) * (self._spacing.onechar_width - 1)
            self._time_scale   = clock_scale/(self._spacing.onechar_width - 1)
            self._schedule_lines = []
            self._detailed_schedule_lines = []
            self._t_offset = time_offset
            self._dimen = [self._spacing.onechar_width*self._num_cycles, 0]
            self._captions = []
            self._horz_lines = []
            self._line_pos    = self._pos.copy()
            self._caption_pos = [self._margins.left, self._pos[1]]
            self._detail_column = include_detail_column
            self._content_width = content_width

        def addScheduleLine(self,
                            location_name : str,
                            disp_name_map : list,
                            **kwargs):
            '''Add schedule lines to the layout (within a group).  The location
            name is allowed to hit multiple locations.  If so, a line
            will be added for each location found.

            Parameters:

               location_name - Regexp of the location to add
               disp_name_map - List/map of names to location

               Other keywords:
                   color      - List of RGB: [0,0,0]
                   reverse    - Reverse the locations found (default is N -> 0)
                   nomunge    - The value found the location raw -- do not colorize
                   space      - Add a dark line after all the locations have been added

            '''
            location_rexp = re.compile(location_name)
            matched_locs  = list(filter(location_rexp.findall, self._locations))

            if len(matched_locs) == 0:
                print("ERROR: Could not find locations matching pattern: ", location_name)
                exit(1)
            sort_list_canotical(matched_locs)

            mini_perc = 0.0
            reg_perc  = 100.0
            if 'mini_split' in kwargs:
                mini_perc, reg_perc = kwargs['mini_split']

            mini_cnt = round(len(matched_locs) * mini_perc/100)
            reg_cnt  = round(len(matched_locs) * reg_perc/100)

            assert (mini_cnt + reg_cnt) == len(matched_locs)

            color = self._def_color
            if 'color' in kwargs:
                color = kwargs['color']
                assert isinstance(color, list), "Color must be a list of 3 colors"

            reverse = True
            if 'reverse' in kwargs:
                reverse = kwargs['reverse']

            content_width = self._content_width

            # Create the mini layout
            if mini_cnt != 0:
                melement_height_total = 0
                mini_anno = matched_locs[reg_cnt:]
                if reverse:
                    mini_anno.reverse()
                for loc in mini_anno:
                    self._schedule_lines.append(
                        self.ScheduleLine(loc,
                                          'auto_color_anno_notext',
                                          color,
                                          [self._dimen[0], self._spacing.melem_height],
                                          self._line_pos.copy(),
                                          self._t_offset))
                    if self._detail_column:
                        # Add the mini layout to the right
                        self._detailed_schedule_lines.append (Content(content='auto_color_anno_notext',
                                                                                loc=loc, color=color,
                                                                                dimensions=[content_width,
                                                                                            self._spacing.melem_height],
                                                                                position = [
                                                                                    self._line_pos[0] +
                                                                                    self._dimen[0] +
                                                                                    self._spacing.caption_width + 20,
                                                                                    self._line_pos[1]],
                                                                                t_offset = 0))

                    melement_height_total += self._spacing.melem_height
                    self._line_pos[1] += self._spacing.melem_height

                caption_name = generate_mini_caption_name(disp_name_map,
                                                          location_rexp,
                                                          matched_locs,
                                                          mini_cnt)

                self._captions.append(ALFLayout.Caption(caption_name, color,
                                                        [self._spacing.caption_width,
                                                         self._spacing.height_spacing],
                                                        self._caption_pos.copy()))

                if self._detail_column:
                    self._captions.append(ALFLayout.Caption(caption_name, color,
                                                            [self._spacing.caption_width,
                                                             self._spacing.height_spacing],
                                                            position=[
                                                                self._line_pos[0] +
                                                                self._dimen[0] + 20, self._caption_pos[1]+1]))

                self._caption_pos[1] += int(round(melement_height_total))

            content = "auto_color_annotation"
            if 'nomunge' in kwargs:
                content = "auto_color_anno_nomunge"

            loc_idx = 0
            # Create the normal sized items
            anno = matched_locs[0:reg_cnt]
            if reverse:
                anno.reverse()
            for loc in anno:
                caption_name = generate_caption_name(disp_name_map,
                                                     location_rexp,
                                                     loc,
                                                     loc_idx)
                loc_idx += 1

                self._schedule_lines.append(
                    self.ScheduleLine(loc,
                                      content,
                                      color,
                                      [self._dimen[0], self._spacing.height],
                                      self._line_pos.copy(),
                                      self._t_offset))

                self._captions.append(ALFLayout.Caption(caption_name,
                                                        color,
                                                        [self._spacing.caption_width,
                                                         self._spacing.height],
                                                        self._caption_pos.copy()))

                if self._detail_column:
                    self._detailed_schedule_lines.append (Content(content=content,
                                                                  loc=loc,
                                                                  color=color,
                                                                  dimensions=[content_width,
                                                                              self._spacing.height],
                                                                  position = [
                                                                      self._line_pos[0] +
                                                                      self._dimen[0] +
                                                                      self._spacing.caption_width + 20,
                                                                      self._line_pos[1]],
                                                                  t_offset = 0))
                    self._captions.append(ALFLayout.Caption(caption_name, color,
                                                            [self._spacing.caption_width,
                                                             self._spacing.height_spacing],
                                                            position=[
                                                                self._line_pos[0] +
                                                                self._dimen[0] + 20, self._caption_pos[1]+1]))

                self._caption_pos[1] += self._spacing.height
                self._line_pos[1]    += self._spacing.height

            # Create a horizontal separator
            if 'space' in kwargs and kwargs['space']:
                self._horz_lines.append(ALFLayout.Caption("",
                                                          [0,0,0], # black
                                                          [self._dimen[0]+self._spacing.caption_width, 1],
                                                          self._caption_pos.copy()))
            # Increment the schedule y dimension
            self._dimen[1] += self._spacing.height_spacing * len(matched_locs)

        def addCycleLegend(self, cycle_legend):
            cycle_line_pos = self._caption_pos.copy()
            # Add the cycle keyword
            self._captions.append(ALFLayout.Caption('C=1 Cycle', self._def_color,
                                                    [self._spacing.caption_width,
                                                     self._spacing.height],
                                                    cycle_line_pos.copy()))
            cycle_word_size = 50

            # Move X
            cycle_line_pos[0] += cycle_word_size

            # Add the current cycle count
            self._captions.append(ALFLayout.Cycle(loc=cycle_legend.location,
                                                  color=self._def_color,
                                                  dimensions=[self._spacing.caption_width - cycle_word_size,
                                                              self._spacing.height],
                                                  position=cycle_line_pos.copy()))

            # Add cycle markers
            cycle_line_pos[0] += self._spacing.caption_width - cycle_word_size

            time_marker_width = 8 * cycle_legend.interval

            for time_marker in range(self._t_offset, self._num_cycles, 5):
                self._captions.append(ALFLayout.Caption(f'C=1 {time_marker}',
                                                        self._def_color,
                                                        [time_marker_width,
                                                         self._spacing.height],
                                                        cycle_line_pos.copy()))
                cycle_line_pos[0] += time_marker_width


        def output_to_alf(self, alf_file):
            assert self._dimen[1] != 0, "Looks like a ScheduleLine was created, but nothing added"
            print(f'''- type: schedule
  color:        ({','.join([str(x) for x in self._def_color])})
  dimensions:   ({','.join([str(x) for x in self._dimen])})
  position:     ({','.join([str(x) for x in self._pos])})
  pixel_offset: {self._pixel_offset}
  time_scale:   {self._time_scale}
  children: ''', file=alf_file)
            for line in self._schedule_lines:
                line.output_to_alf(alf_file)
            for line in self._detailed_schedule_lines:
                line.output_to_alf(alf_file)
            for cap in self._captions:
                cap.output_to_alf(alf_file)
            for horz in self._horz_lines:
                horz.output_to_alf(alf_file)

    class ColumnView:
        '''A ColumnView has captions to the left and single-cycle view to the right
        '''
        def __init__(self,
                     locations: list,
                     spacing,
                     margins,
                     content_width,
                     def_color,
                     time_offset : int):

            self._locations = locations
            self._spacing   = spacing
            self._margins   = margins
            self._content_width = content_width
            self._def_color = def_color
            self._t_offset  = time_offset
            self._pos       = [self._margins.left, self._margins.top]
            self._captions  = []
            self._content   = []

        def addColumn(self,
                      location_name : str,
                      disp_name_map : list,
                      ** kwargs):
            location_rexp = re.compile(location_name)
            matched_locs  = list(filter(location_rexp.findall, self._locations))
            if len(matched_locs) == 0:
                print("ERROR: Could not find locations matching pattern: ", location_name)
                exit(1)
            sort_list_canotical(matched_locs)
            matched_locs.reverse()
            content = "auto_color_annotation"
            if 'nomunge' in kwargs:
                content = "auto_color_anno_nomunge"
            loc_idx = 0
            for loc in matched_locs:
                caption_name = generate_caption_name(disp_name_map, location_rexp, loc, loc_idx)
                loc_idx += 1

                self._captions.append(ALFLayout.Caption(caption_name,
                                                        color=self._def_color,
                                                        dimensions=[self._spacing.caption_width,
                                                                    self._spacing.height],
                                                        position=self._pos.copy()))
                self._content.append(Content(content=content,
                                             loc=loc,
                                             color=self._def_color,
                                             dimensions=[self._content_width,
                                                         self._spacing.height],
                                             position = [
                                                 self._pos[0] +
                                                 self._spacing.caption_width,
                                                 self._pos[1]],
                                             t_offset = 0))

                self._pos[1] += self._spacing.height

        def output_to_alf(self, alf_file):
            for content in self._content:
                content.output_to_alf(alf_file)
            for caption in self._captions:
                caption.output_to_alf(alf_file)

    ################################################################################
    # ALFLayout construction
    def __init__(self,
                 start_time    : int,
                 num_cycles    : int,
                 clock_scale   : int,
                 location_file : str,
                 alf_file      : str):
        '''
        Create an ALF Layout file (.alf) used by argos (-l <alf> option).

        There are two parts to this generator:
        1. Creating schedule lines (multiple cycle views)
        2. Creating a column view (single cycle view in a column format)

        Construction arguments:
        start_time    - When to start showing time (can be negative)
        num_cycles    - Number of cycles to view when adding schedule lines
        clock_scale   - Used to setup the Argos timescale
        location_file - The location file generated by a simulator when using the -z option
        alf_file      - The final ALF file
        '''
        self._all_good = False
        self.__parse_location_file(location_file)
        self._all_good = True
        self._start_time    = start_time
        self._num_cycles    = num_cycles
        self._clock_scale   = clock_scale
        self._spacing       = None
        self._schedule_line = None
        self._column_view   = None

        # Variable that contains the state of object being placed in
        # the layout (to prevent overlaps)
        self._positional_state = [0, 0]

        # Print the first line expected in all ALFs
        self._alf_file = open(alf_file, 'w')
        print('---', file=self._alf_file)

    def __del__(self):
        '''Destroy the Layout, writing all results to the given ALF file
        '''
        if self._all_good:
            if self._schedule_line:
                self._schedule_line.output_to_alf(self._alf_file)
            if self._column_view:
                self._column_view.output_to_alf(self._alf_file)
            print('...', file=self._alf_file)

    ################################################################################
    # ALFLayout API
    def setSpacing(self, spacing):
        '''For this layout, set the basic spacing that should be used.  See
        Spacing class for more documentation.
        '''
        self._spacing = spacing

    def count(self, location):
        '''Count the number of matches for the given location (regexpression).
        This is handy when unsure how many units are in the given
        location and the need to iterate over them is required.  A
        capture group is required to make the query very unique.

           num_alu = my_layout.count(r'.*alu([0-9]).*`)

        '''
        regexp = re.compile(location)
        loc_matches = list(filter(regexp.findall, self._loc_strs))
        uniq_names = set()
        for match in loc_matches:
            uniq_names.add(re.sub(regexp, r'\1', match))
        return len(uniq_names)

    def createScheduleLineGroup(self, default_color, include_detail_column, content_width, margins):
        '''Create a ScheduleLineGroup that ScheduleLines can be added.  See
        ScheduleLineGroup for more documentation.
        '''
        self._schedule_line = self.ScheduleLineGroup(clock_scale = self._clock_scale,
                                                     num_cycles  = self._num_cycles,
                                                     locations   = self._loc_strs,
                                                     def_color   = default_color,
                                                     spacing     = self._spacing,
                                                     margins     = margins,
                                                     include_detail_column = include_detail_column,
                                                     content_width = content_width,
                                                     time_offset = self._start_time)
        return self._schedule_line

    def createColumnView(self, margins, content_width, default_color=[192,192,192]):
        '''Create a ColumnView -- a single cycle view with added components.
        See ColumnView for more documentation.
        '''
        self._column_view = self.ColumnView(locations = self._loc_strs,
                                            spacing   = self._spacing,
                                            margins   = margins,
                                            content_width = content_width,
                                            def_color     = default_color,
                                            time_offset=0)
        return self._column_view
