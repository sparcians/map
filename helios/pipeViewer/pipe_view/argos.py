#!/usr/bin/env python3

# @package argos.py
#  @brief Startup file for argos
#
#  This file must be run as a script and not imported

from __future__ import annotations
import argparse
import logging
from logging import info, error, warn
import os
import pstats
import re
import sys
from typing import Dict, List, Optional, Tuple, Union, cast
import wx
import cProfile

info(f"currently with debug mode: {__debug__}")

# Check Interpreter Version
assert sys.version_info[0] == 3 and sys.version_info[1] >= 6, \
    f'Python interpreter {sys.argv[0]} version ({sys.version}) ' \
    'is too old. This version is not supported but might still ' \
    'work if you disable this assertion'

# Path for built argos cython libraries
added_path = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                          'core/lib')
if added_path is not None:
    sys.path.insert(0, added_path)  # Add temporary search path

corePath = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'core')
sys.path.insert(0, corePath)

# Argos imports need to go below this line (some depend on the path to core.so)

import gui.autocoloring  # noqa: E402
from gui.dialogs.select_db_dlg import SelectDatabaseDlg  # noqa: E402
from gui.dialogs.select_layout_dlg import SelectLayoutDlg  # noqa: E402
from model.database import Database  # noqa: E402
from model.settings import ArgosSettings  # noqa: E402
from model.utilities import LogFormatter  # noqa: E402
from model.workspace import Workspace  # noqa: E402

# End Argos imports

if __name__ != '__main__':
    raise ImportError('argos.py must be run as a script and not imported')
else:
    rc = 0

    logging.getLogger().setLevel(logging.INFO)
    streamHandler = logging.StreamHandler(sys.stderr)
    streamHandler.setFormatter(LogFormatter())
    logging.getLogger().addHandler(streamHandler)

    # Regex for finding variable assignments inside a --layout-vars option
    #  @note Designed to handle " core_num=7 , foo = 8 ,bar= 2, cat =3   "
    LAYOUT_VAR_RE = re.compile(r'\s*(?:,?\s*)?(\w+)\s*=\s*(\w+)\s*')

    parser = argparse.ArgumentParser(
        description='Argos Viewer\n'
        'Visualize a transaction database collected from a simulation',
        epilog='Copyright 2019',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        '--layout-file',
        '-l',
        metavar='LAYOUT_FILE',
        type=str,
        nargs='?',
        action='append',
        help='Specifies an Argos layout file to open. Each Layout file opened '
             'corresponds to a Frame. This option can be specified any number '
             'of times to open multiple frames'
    )
    parser.add_argument(
        '--layout-vars',
        '-L',
        metavar='VARS_DICT',
        type=str,
        nargs=1,
        action='append',
        help='Specifies a comma-separated list of "key=value" pairs '
             'associated with one layout instance. The Nth instance of this '
             'option corresponds to the Nth instance of --layout-file/-l. '
             'Example: "argos -l mylayout -L "core_num=2"'
    )
    parser.add_argument(
        '--layout-geometry',
        '-g',
        metavar='GEOMETRY',
        type=str,
        nargs=1,
        action='append',
        help='Specifies a comma-separated list of geometry associated with '
             'one layout instance in the form "w,h[,x,y]". The Nth instance '
             'of this option corresponds to the Nth instance of '
             '--layout-file/-l. Use "0,0,0,0" to minimize the layout. '
             'Example: "argos -l mylayout -g "800,200,0,0"'
    )
    parser.add_argument(
        '--resource-dir',
        '-R',
        metavar='DIR',
        type=str,
        nargs='*',
        help='Specifies an additional directory in which to search for '
             'resources used by argos layouts (e.g. image files). By default, '
             'resources are searched for in the same directory as the '
             'requesting layout file and then in the built-in '
             'argos_view/resources directory'
    )
    parser.add_argument(
        '--database',
        '-d',
        metavar='DATABASE_PREFIX',
        type=str,
        nargs='?',
        help='Selects the database to be opened. If not specified, the user '
             'will be prompted within the application'
    )
    parser.add_argument(
        '--start-cycle',
        '-c',
        metavar='CYCLE_NUM',
        type=str,
        nargs='?',
        help='Selects the initial cycle number in terms of clock set by '
             '(--clock). Overrides --start-tick'
    )
    parser.add_argument(
        '--start-tick',
        '-t',
        metavar='TICK_NUM',
        type=str,
        nargs='?',
        help='Selects an initial tick number at which to display the layout(s)'
    )
    parser.add_argument(
        '--clock',
        metavar='CLOCK_NAME',
        type=str,
        nargs='?',
        help='Selects an initial displayed clock for each layout frame. If no '
             'matching clock is found by this name (case insensitive), a '
             'default is selected which is usually the root clock'
    )
    parser.add_argument(
        '--poll',
        action='store_true',
        help='Update Argos if the database file changes'
    )
    parser.add_argument(
        '--title-prefix',
        metavar='TITLE_PREFIX',
        type=str,
        nargs='?',
        help='Specifies a prefix for the window title'
    )
    parser.add_argument(
        '--title-override',
        metavar='TITLE_OVERRIDE',
        type=str,
        nargs='?',
        help='Overrides the window title with the specified string'
    )
    parser.add_argument(
        '--title-suffix',
        metavar='TITLE_SUFFIX',
        type=str,
        nargs='?',
        help='Specifies a suffix for the window title'
    )
    parser.add_argument(
        '--debug',
        action='store_true',
        help='Debugging and Verbose output'
    )
    parser.add_argument(
        '--profile',
        action='store_true',
        help='Profile and report where time is being spent'
    )
    parser.add_argument(
        '--tracemalloc',
        action='store_true',
        help='trace memory blocks allocated by this program'
    )
    parser.add_argument(
        '--quiet',
        action="store_true",
        help='Be less verbose, set the logging level to warning'
    )

    args = parser.parse_args()

    if args.profile and args.tracemalloc:
        error("cannot enable profiling and tracemalloc at the same time")
        sys.exit(1)
    elif args.profile:
        info("enabling profiling")
        prof = cProfile.Profile(subcalls=True, builtins=True)
        prof.enable()
    elif args.tracemalloc:
        info("enabling tracemalloc profiling")
        import tracemalloc
        tracemalloc.start()

    # logging setup
    # tell the formatter how to identify this one
    logging.getLogger().setLevel(
        logging.WARNING if args.quiet else
        (logging.DEBUG if args.debug else logging.INFO)
    )

    # file output
    fileHandler = logging.FileHandler('argos.log')
    fileHandler.setLevel(logging.DEBUG)
    fileHandler.setFormatter(LogFormatter(isSmoke=False))
    logging.getLogger().addHandler(fileHandler)

    # Argos Application
    class ArgosApp(wx.App):

        def OnInit(self) -> bool:
            return True

    app = ArgosApp()

    settings = ArgosSettings()
    # The user can specify default colorblindness and palette shuffle modes
    # with these environment variables
    gui.autocoloring.BuildBrushes(settings.palette, settings.palette_shuffle)

    # Preconfigure the workspace with options
    # Must be after wx.App is instantiated
    ws = Workspace(settings)

    if args.resource_dir is not None:
        for rd in args.resource_dir:
            ws.AddUserResourceDir(rd)

    # Parse clock
    select_clock: Optional[str]
    if args.clock is not None:
        select_clock = str(args.clock)
    else:
        select_clock = None

    # Determine start tick
    start_tick = None
    if args.start_tick is not None:
        start_tick = int(args.start_tick)

    start_cycle = None
    if args.start_cycle is not None:
        start_cycle = int(args.start_cycle)
        if start_tick is not None:
            logging.warn(
                '--start-cycle is overriding '
                '--start-tick because both were specified'
            )
            start_tick = 0

    dlg: wx.Dialog
    # Select Database
    if args.database is not None:
        database_prefix = args.database
    else:
        # Show database-selection dialog
        dlg = SelectDatabaseDlg()
        dlg.Centre()
        if dlg.ShowModal() == wx.CANCEL:
            sys.exit(0)
        dlg.Destroy()

        database_prefix = dlg.GetPrefix()
        if database_prefix is None:
            error('No database selected, exiting Argos')
            sys.exit(1)

    # Open the Database
    try:
        db = Database(database_prefix, args.poll)
    except IOError as ex:
        error(f'Error opening pipeout database (prefix) "{database_prefix}"')
        error(ex)
        sys.exit(1)

    if args.layout_file is not None:
        layout_files = args.layout_file
    else:
        # Show layout-selection dialog
        dlg = SelectLayoutDlg()
        dlg.Centre()
        if dlg.ShowModal() == wx.CANCEL:
            exit(0)
        dlg.Destroy()

        layout_files = [dlg.GetFilename()]
        # Note: '' or None will cause an empty layout to be created

    layout_vardicts: Optional[List]
    if args.layout_vars is not None:
        layout_vardicts = []
        for lv in args.layout_vars:
            d: Dict[str, str] = {}
            layout_vardicts.append(d)

            last_match_size = [0]

            def replace(match: re.Match[str]) -> str:
                # group(1) is key, group(2) is value
                # group(0) is full string (x=y)
                d[str(match.group(1))] = str(match.group(2))
                last_match_size[0] = len(match.group(0))
                return ''

            pos = 0
            lv = lv[0]
            while pos < len(lv):
                # Disregard result. No replacement is actually done
                _, repls = LAYOUT_VAR_RE.subn(replace, lv[pos:], 1)
                if repls > 0:
                    pos += last_match_size[0]
                else:
                    if pos < len(lv):
                        error(
                            "Failed to evaluate layout variables argument "
                            f"'{lv}' as a comma-separated list of key=value "
                            f"pairs. Successfully parsed '{lv[:pos]}'. "
                            f"Unable to parse '{lv[pos:]}'"
                        )
                        sys.exit(1)
    else:
        layout_vardicts = None

    layout_geos: \
        Optional[List[Union[Tuple[int, int], Tuple[int, int, int, int]]]]
    if args.layout_geometry is not None:
        layout_geos = []
        for lgs in args.layout_geometry:
            toks = [x.strip() for x in lgs[0].split(',')]
            if len(toks) != 2 and len(toks) != 4:
                raise ValueError(
                    f'Unable to convert layout frame geometry "{lgs}" to a '
                    f'sequence of 2 or 4 integers. {len(toks)} tokens found'
                )

            try:
                w = int(toks[0])
                h = int(toks[1])
            except ValueError as ex:
                raise ValueError(
                    f'Unable to convert layout frame geometry "{lgs}" to a '
                    f'sequence of 2 or 4 integers: {ex}'
                )

            if len(toks) == 4:
                try:
                    x = int(toks[2])
                    y = int(toks[3])
                except ValueError as ex:
                    raise ValueError(
                        f'Unable to convert layout frame geometry "{lgs}" to '
                        f'a sequence of 2 or 4 integers: {ex}'
                    )
                layout_geos.append((w, h, x, y))
            elif len(toks) == 2:
                layout_geos.append((w, h))
            else:
                assert False  # Should be covered by if statement above
    else:
        layout_geos = None

    # Launch Layout Frames
    assert layout_files is not None

    for layout_idx, lf in enumerate(layout_files):
        if lf is None:
            lf = ''  # Empty layout

        if layout_vardicts is not None and layout_idx < len(layout_vardicts):
            loc_vars = layout_vardicts[layout_idx]
        else:
            loc_vars = {}

        frame = ws.OpenLayoutFrame(lf,
                                   db,
                                   start_tick,
                                   args.poll,
                                   args.title_prefix,
                                   args.title_override,
                                   args.title_suffix,
                                   loc_vars,
                                   norefresh=True)

        refreshed = False
        if layout_geos is not None and layout_idx < len(layout_geos):
            lg = layout_geos[layout_idx]
            # Difference between actual size and window size (e.g. menu bar).
            # Use this to offset user's dimensions to line things up as
            # expected (no overlap). This may break other systems but they can
            # always just turn off geometry specification as a temoprary
            # workaround.
            # There is complimentary logic in Argos_Menu.OnFrameInfo
            wdiff = frame.GetSize()[0] - frame.GetClientSize()[0]
            hdiff = frame.GetSize()[1] - frame.GetClientSize()[1]
            if len(lg) == 4:
                lg = cast(Tuple[int, int, int, int], lg)
                if lg == (0, 0, 0, 0):
                    frame.Iconize()
                else:
                    w, h, x, y = lg
                    frame.SetRect(wx.Rect(x, y, w - wdiff, h - hdiff))
            elif len(lg) == 2:
                lg = cast(Tuple[int, int], lg)
                w, h = lg
                frame.SetSize((w, h))
            else:
                error("layout_geos is not 2 or 4")
                sys.exit(-1)

        if select_clock:
            if not frame.SetDisplayClock(select_clock, False):
                clock_mgr = frame.GetContext().dbhandle.database.clock_manager
                warn(
                    f"Failed to select display clock {select_clock}. "
                    f'No such clock for frame "{frame.GetTitle()}". '
                    '--start-cycle will not be applied if set. '
                    "Available clocks are: "
                    f"{', '.join(c.name for c in clock_mgr.GetClocks())}"
                )
            elif start_cycle is not None:
                frame.GoToCycle(start_cycle)  # Implies a frame refresh
                refreshed = True

        frame.Show(True)

        if not refreshed:
            # Updates metadata so everything is good on the first rendering
            frame.GetContext().RefreshFrame()

    # Run until close
    app.MainLoop()

    if args.profile:
        prof.disable()
        overallStats = pstats.Stats(prof)
        overallStats = overallStats.sort_stats("cumulative")
        overallStats.reverse_order()
        overallStats.print_stats()
    elif args.tracemalloc:
        snapshot = tracemalloc.take_snapshot()
        stats = snapshot.statistics('lineno', cumulative=True)
        for stat in stats:
            info(stat)

    ws.Cleanup()
    sys.exit(rc)
