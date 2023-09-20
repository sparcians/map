import sys,os
import pathlib


gen_dir = pathlib.PurePath(os.path.abspath(__file__))
scripts_dir  =''
alf_gen_path = '/helios/pipeViewer/scripts'
for part in gen_dir.parts[1:]:
    scripts_dir += '/' + part
    if os.path.exists(scripts_dir + alf_gen_path):
        scripts_dir += alf_gen_path
        break
assert scripts_dir != '', f"Can't find {alf_gen_path}"

sys.path.append(scripts_dir)
from alf_gen.ALFLayout import ALFLayout

if os.path.isfile("pipeout/location.dat") == False:
    print('''ERROR: Cannot find pipeout/location.dat.  This tool cannot be run without a location file.
    To generate one:
        mkdir pipeout
        ./sparta_core_example -i1 -z pipeout/
    ''')
    exit(255)
try:
    os.mkdir('layouts')
except:
    pass

NUM_CYCLES = 55
layout = ALFLayout(start_time  = -10,
                   num_cycles  = NUM_CYCLES,
                   clock_scale = 1,
                   location_file = "pipeout/location.dat",
                   alf_file    = "layouts/cpu_layout.alf")

general_height = 15
layout.setSpacing(ALFLayout.Spacing(height         = general_height,
                                    height_spacing = general_height - 1,
                                    melem_height   = general_height/3,
                                    melem_spacing  = general_height/3 - 1,
                                    caption_width  = 150))

sl_grp = layout.createScheduleLineGroup(default_color=[192,192,192],
                                        include_detail_column = True,
                                        margins=ALFLayout.Margin(top = 2, left = 10))

# Fetch
sl_grp.addScheduleLine('.*fetch.next_pc', ['Next PC'], space=True)

# Decode
sl_grp.addScheduleLine('.*decode.FetchQueue.FetchQueue([0-9]+)', [r'FQ[\1]'], space=True)

# Rename
sl_grp.addScheduleLine('.*rename.rename_uop_queue.rename_uop_queue([0-9]+)', [r'Rename[\1]'], space=True)

# Dispatch
sl_grp.addScheduleLine('.*dispatch.dispatch_queue.dispatch_queue([0-9]+)', [r'Dispatch[\1]'], mini_split=[80,20], space=True)

# Credits
sl_grp.addScheduleLine('.*dispatch.in_(.*)_credits', [r'\1 Credits'], space=True, nomunge=True)

# ALUs
def add_exe_block(block_name):
    num_block_names = layout.count(f'(.*{block_name}[0-9]+)\..*')
    if num_block_names == 0:
        sl_grp.addScheduleLine(f'.*{block_name}.scheduler_queue.scheduler_queue([0-9]+)',
                               [rf'{block_name} IQ\1 '], space=True)
        sl_grp.addScheduleLine(f'.*{block_name}$', [rf'{block_name} pipe'], space=True)
    else:
        cnt = 0
        while cnt < num_block_names:
            sl_grp.addScheduleLine(f'.*{block_name}{cnt}.scheduler_queue.scheduler_queue([0-9]+)',
                                   [rf'{block_name}{cnt} IQ\1 '], space=True)
            sl_grp.addScheduleLine(f'.*{block_name}{cnt}$', [rf'{block_name}{cnt} pipe'], space=True)
            cnt += 1

for block in ['alu', 'fpu', 'br']:
    add_exe_block(block)

# LSU
sl_grp.addScheduleLine('.*lsu.lsu_inst_queue.lsu_inst_queue([0-9]+)',       [r'LSU IQ[\1]'])
sl_grp.addScheduleLine('.*lsu.LoadStorePipeline.LoadStorePipeline([0-9]+)',
                       ['LSU Pipe MMU',
                        'LSU Pipe D$',
                        'LSU Pipe Commit'], reverse=False)
sl_grp.addScheduleLine('.*lsu.dcache_busy$', ['D$ Busy'], nomunge=True, space=True)

# ROB
sl_grp.addScheduleLine('.*rob.ReorderBuffer.ReorderBuffer([0-9])', [r'ROB[\1]'], mini_split=[80,20], space=True)

# Insert the cycle line
sl_grp.addCycleLegend(ALFLayout.CycleLegend(location = 'top.cpu.core0.rob.ReorderBuffer.ReorderBuffer0',
                                            interval = 5))

col_view = layout.createColumnView(margins=ALFLayout.Margin(top=2, left=1000),
                                   content_width=500)
col_view.addColumn('.*rob.ReorderBuffer.ReorderBuffer([0-9]+)', [r'ROB[\1]'])
