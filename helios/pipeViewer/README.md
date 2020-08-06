# Argos: The PipeViewer Transaction Viewer

## Prerequisites

1. This project requires Cython and wxPython, which are available in the Conda environment
1. If conda is not installed, install it
   * Get miniconda and install: https://docs.conda.io/en/latest/miniconda.html
   * You can install miniconda anywhere
1. Go to the root of MAP
   * `cd map`
1. Install JSON and Yaml parsers
   * `conda install -c conda-forge jq`
   * `conda install -c conda-forge yq`
1. Create a sparta conda development environment
   * `./scripts/create_conda_env.sh sparta dev`
1. Activate the environment
   * `conda activate sparta`
1. To build this tool and its dependent libraries, use cmake from the root of MAP using the created conda environment
   * `conda activate sparta`
   * `cd $(git rev-parse --show-toplevel); mkdir release; cd release`
   * `cmake -DCMAKE_BUILD_TYPE=Release ..`
   * `make`

## Example of Usage

In this example, you will:
1. Build example core model
1. Generate pipeline database (also known as a `pipeout`)
1. View pipeout using single-cycle layout
1. View pipeout using multi-cycle layout
1. Learn how to create and edit your own multi-cycle layout

### 1. Build example core model

There are several ways to build the example core model.  First build sparta as described above.  Then, pick one of the following:
- In your build directory (ex:  `map/sparta/release`), run `make regress`
- In your build directory (ex:  `map/sparta/release`), run `make sparta_core_example`
- In example core model directory (ex:  `map/sparta/release/example/CoreModel`), run `make`

### 2. Generate pipeline database

In the example core model directory (ex:  `map/sparta/release/example/CoreModel`), run:
```
$ ./sparta_core_example -i 1000 -z pipeout_
```

In this example, we run the simulation until 1000 instructions have retired, and we generate a pipeline database corresponding to the entire run.  If we wanted to skip a certain number instructions or cycles before collecting pipeline information, we could use the `--debug-on` or `--debug-on-icount` options to specify when to start collecting pipeline information.

The pipeline database consists of several files, all of which begin with the same prefix.  In the example above, we specify the prefix `pipeout_`.

The database consists of metadata, plus a number of `transactions`.  A `transaction` is a piece of data (such as an instruction or a count) that occurs in a specific cycle in a specific location.  A `location` can be a pipeline stage, a place in a structure like a queue, a signal, etc.

### 3. View pipeout using single-cycle layout

To view the database, you must run Argos and specify both the database prefix and one or more layout files.  A `layout` file specifies how the transactions will be visually organized on the screen.

In the example core model directory, run:
```
# Linux
$ python3 ${MAP}/helios/pipeViewer/pipe_view/argos.py --database pipeout_ --layout-file cpu_layout.alf

# MacOS
$ pythonw ${MAP}/helios/pipeViewer/pipe_view/argos.py --database pipeout_ --layout-file cpu_layout.alf
```
where `${MAP}` represents the path to your `map` repo.  We are using the database with the prefix `pipeout_` and are using the single-cycle layout file in this directory `cpu_layout.alf`.

This single-cycle view shows only a single cycle at a time.  Each box represents a location.  In the lower left corner of the window, we see the current cycle.  We use controls at the bottom of the window to change the cycle, in any of the following ways:
- Right and left arrow keys will increment/decrement current cycle by 1.
- Buttons at the bottom let you change the current cycle by +1, +3, +10, +30, etc.
- A text box near the buttons lets you specify how much you want to jump; enter the value and click the "jump" button.  If the number begins with a `+` or `-` then the cycle changes by this relative amount.  If the number does not begin with `+` or `-`, then this specifies the absolute value of the cycle to jump to.
- A `play` button in the lower right of the window allows automatically changing the current cycle over time.

More controls:  you should be able to use normal controls to zoom in/zoom out in the display.  For example, `Ctrl +` and `Ctrl -` or `Ctrl` plus mouse scroll should work.

As we change the current cycle, we can see transactions appear in the appropriate locations.  Note that each transaction has a color and begins with a `display ID`.  If the visual element is not large enough to hold all the text in the transaction, you can hover your mouse over the location and hover text will show you the entire text of the transaction.

The `display ID` is used to assign to the transaction both a color and a single letter (case is important).  The letter is important in the multi-cycle layout and will be discussed in that section.

If the transaction represents an instruction, the instruction should have a consistent display ID no matter where the instruction is located.  For example, if the instruction's display ID is mapped to a purple W, it should be a purple W no matter what location it is in.  (Again, the "W" is mainly used in the multi-cycle layout.)

Single-cycle layouts can be created using graphical editing tools built into the pipeline viewer, but we do not discuss this here because multi-cycle layouts are much more powerful and versatile and are the preferred method for viewing the pipeline database.

### 4. View pipeout using multi-cycle layout

Viewing a multi-cycle layout is done the exact same way as a single-cycle layout.  In the example core model directory, run:
```
# Linux
python3 ${MAP}/helios/pipeViewer/pipe_view/argos.py --database pipeout_ --layout-file ${MAP}/helios/pipeViewer/scripts/gen_alf/core.alf

# MacOS
pythonw ${MAP}/helios/pipeViewer/pipe_view/argos.py --database pipeout_ --layout-file ${MAP}/helios/pipeViewer/scripts/gen_alf/core.alf
```

In this example, we replace the single-cycle layout file with a multi-cycle layout.  You can also specify multiple layout files.  Each layout will be displayed in its own window, and the current cycle will be sync'ed up among all the layout specified.  For example:

```
# Linux
python3 ${MAP}/helios/pipeViewer/pipe_view/argos.py --database pipeout_ --layout-file cpu_layout.alf --layout-file ${MAP}/helios/pipeViewer/scripts/gen_alf/core.alf

# MacOS
pythonw ${MAP}/helios/pipeViewer/pipe_view/argos.py --database pipeout_ --layout-file cpu_layout.alf --layout-file ${MAP}/helios/pipeViewer/scripts/gen_alf/core.alf
```

In this example layout, there are three main columns of locations.  The left column contains the multi-cycle display; the middle and right columns show only the current cycle.  The middle column is a more verbose version of the left column, and the right column contains a "directory" containing the current reorder buffer (ROB) entries.

As before, you can change the current cycle with the controls at the bottom of the window.

Let's examine the multi-cycle display (left column).  The horizontal axis is time (cycles).  At the bottom of the left column is an orange bar containing numbers.  These numbers are offsets from the current cycle.  For example, let's say the current cycle is 100.  Then the column labeled "0" represents cycle 100, whereas the column labeled "-5" represents cycle 100-5 = 95, and the column labeled "20" represents cycle 100+20 = 120.

The vertical axis represent different locations in the pipeline.  Many of these show instructions as they flow through the pipeline, one leter per instruction.  You can see instructions propagating diagonally in the display as they go from one pipeline stage to the next.

Not all locations represent instructions.  For example, the "cred" locations represent the number of credits received by that execution unit on that cycle.  And in the load-store unit (LSU), locations represent load/store transactions.  Note you can mouse over any location to see a pop-up corresponding to that location.

Note a further detail.  For example, look at the ROB entries at the bottom of the multi-cycle display.  In the core model example, there are many ROB entries; displaying them all in full would eat up a lot of screen real estate.  So we choose to display only the oldest entries in full (labeled `ROB[0]`, `ROB[1]`, etc).  Beyond the oldest entries, we display the rest of the entries in a vertically-compressed "mini" format.  This still gives a visual representation of the fullness of the ROB, but the letters are no longer visible.  However, the transaction colors are still visible.  You can still hover your mouse over these "mini" entries to see the full text.

The middle column shows only the current cycle (corresponding to the "0" column in the multi-cycle column).  The locations match the ones in the left column.

Finally, the right column provides additional info and shows the entire ROB verbosely.  You can use the middle and right columns to see at a glance more data about each transaction in the display.

### 5. Learn how to create and edit your own multi-cycle layout

Multi-cycle layouts are specified using a simple ad-hoc text format.  By convention, these layout specification files have the `.cmd` extension.  You can see example `.cmd` files in `${MAP}/helios/pipeViewer/scripts/gen_alf`.  This is easily edited; to create your own multi-cycle layout, it's best to copy this example.

This example includes three `.cmd` files.  `core.cmd` is the primary file and includes two others, `vars.cmd` and `core_column.cmd`.  `vars.cmd` contains general settings.  `core_column.cmd` is the heart of the layout and specifies which locations to display.

To generate a layout file (extension:  `.alf`), in this directory run the script `gen_layouts` which runs the `gen_alf` script.  `gen_alf` takes the `.cmd` file and creates the `.alf` layout file.

See `core_column.cmd` to see how to specify locations.  Location paths can be seen in the `*location.data` file when the pipeline database is generated.  In our example, the file is `${MAP}/sparta/release/example/CoreModel/pipeout_location.data` file.
