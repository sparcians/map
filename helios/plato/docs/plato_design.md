# plato

This document is a high-level overview of the design of plato. To simply get up and running, look at `getting_started.md`

##Overview 

plato is a generic browser-based data-processing and visualization tool that allows users interactively view moderately large datasets (~100 million rows) of computer architecture simulation events using custom widgets. 

plato itself is not a visualization library, but provides a workspace-like configurable user interface and a client-server framework for communicating data to visualization widgets running in a browser-based client.

Since visualizing new types of data in novel ways will always require coding and many plotting libraries exist for this (many widgets use Plotly, Bokeh, or HTML/CSS), plato assumes that users will always want to add new visualizations and does not try and "solve" visualization of data.

Instead, plato is an environment to help take raw data, process it, and provide it to browser-based visualizations in real-time, solving some of the common problems of navigating and comparing large datasets, and adding some helpful features that make the tool powerful and efficient.

Some of these features include: 

  1. **Client-Server Architecture**: for performing computation and server-class machines but allowing browser-based viewing by other users.
  1. **Widget-based Design**: Individual widgets offer different, customizable visualizations for the same data. Users can instantiate as many widgets as necessary, configure them, and rearrange them to set up different views of their dataset or multiple datasets. 
  1. **Drag-and-Drop Viewing**: Enumeration of all statistics available in each data-set and the ability to Drag statistics (or even entire data-sets) to widgets to visualize them.
  1. **Saveable Layouts**: When a set of widgets is arranged and configured in a useful way looking at a specific region of data for studying a particular problem, it can be saved and reloaded later to look at other data-sets. These saved "layouts" can be shared with other users.
  1. **Time Synchronization**: All widgets are synchronized (if desired) to a global time range so zooming into certain regions of data updates other widgets automatically
  1. **Useful Code**: Existing code exists for downsampling large-scale time-series data and caching computation results
  1. **Extensive Caching**: Caching of data-sets of preprocessed data for lower latency user responses. Reprocessing data for every request is not feasible for a responsive application.
  1. **Comparing Events**: plato helps view events from multiple simulations on the same time-scale at once for easy comparison of configurations.

Adding new types of data and visualizations requires coding but the "Data Pipeline" is designed to minimize duplicate coding.

plato relies on well-behaved clients to interact with the server(s) in a healthy way so is not suitable for public-facing use as-is.   

### Background

plato was created for work on high performance CPU designs with an initial focus on branch prediction analysis. Currently, much of the data-processing and visualization code within the plato project serves this specific purpose but plato itself is generic.

Plato is operational and meets many of its original objectives, but is far from complete, specific to a few types of data files, and generally unpolished. It's development was halted due to a project cancellation.

### Technology

plato is implemented with Javascript, jQuery, HTML, CSS on the frontend and Python3.6 using Django, Django-Channels, Numpy, Numba, Pandas, and Dask on the backend.

Browser-based visualization code uses Plotly and Bokeh, while some data-source-specific backend code uses h5py and Python sqlite. 

## Objectives

plato is designed to give researchers a means of exploring moderately large event data-sets from computer architecture simulations, looking at the data from multiple perspectives, identifying points of interest, drilling down, filtering, and then re-examining the data from various perspectives.

Under normal use, users can view downsampled or summary versions of entire data-sets at once. Different widgets can be created and configured to view this data in different ways. Then, the user can seamlessly drill down to progressively more narrow regions of time. 

Unlike other user-configurable time-series viewers with custom widgets, plato supports multiple arbitrary "time" units for data and int64 granularity for those units which allows users to view events in terms of cycles, instruction counts, event counts, or anything else captured. Multiple data-sources from different simulations or configurations can loaded simultaneously for comparison. Disparate data-sources can be loaded as well and displayed along the same time-scale assuming they have a common time unit. 

The backend data processing is implemented with some polymorphic types that make it possible to connect data-sources to different types of processing (Data Pipeline). It is partly specific to the type of data being processed and implemented in Python with highly optimized numpy and numba code using a lot of caching to keep the application responsive to users. In principal this design allows new data sources to be connected to existing processing. It also allows modelers and researchers to prototype Python-based data-processing code which can later be implemented in the plato backend data-pipeline and connected with a graphical widget (with the help of developers and probably only after some optimization).


## Types of Data Supported (so far)

plato currently supports a type of specialized data that is likely to not be useful for new use-cases, but at provides examples for future types of data: 
 * HDF5 branch predictor files.
 
 plato also supports a few more generic types of data that might be immediately useful for new applications:
 * HDF5 performance-event (pevent) data
 * Sqlite time-series event data from simulation reports.
 
 Data provided for plato typically comes form a simulator using the "Sparta" simulation framework.
 
 Support for new data can be added by implementing new DataSource classes and creating function handlers in the bpEndpoint.py that retrieve that data. This is discussed more in "Writing a new Widget" and "Writing new Data Processing" 

## Architecture

plato is built on a client-server architecture using a browser-based user client to communicate with backend servers which do the actual computation and provide requested data and processing to the client.



### High-Level Terminology

These terms refer to components or concepts that are useful for understanding the plato architecture. They are explained in more detail throughout this document.

* **Client**: The browser-based GUI used to visualize data from a data-source, through one or more processors
  * **Widget**: An element in the client designed to view a data in a certain way (e.g. line plot, heatmap, etc.) from a single source with some processing
  * **Layout**: An instance of all saveable GUI state including widgets, their settings and shapes, some other plato GUI options, and a selected time-range.
* **Server**: A Django-based http and websocket server that reads data from disk or another source, processes it, and provides it to a requesting Client.
* **Data-Source**: A file, files, or data-base containing data of a single type (e.g. branch-predictor events, time-series counters, etc.) from a single experiment. Sometimes the term data-set is used instead of Data-Source.


### Client

The plato client is HTML + Javascript (and jquery based) and communicates plato servers using websockets. The plato client can be directed to load data from certain directories with which it will query the server for a list of available data-sources.

#### GUI Overview

_TODO_

The plato GUI has a means of selecting a set of data-sources to operate on.

The means of selecting data-sources from the GUI still needs to be fleshed out. Currently, a URL query parameter can be used but a dialog box should be implemented to browse server-side data and add data-sources to a working set. 

### Web

The plato backend servers are implemented in Python as a Django application accessible as both a standard web server and through websockets (Django Channels). This Django application provides authentication support, serves dynamic resources, and implements a set of asynchronous web-socket-based functions through which the client can query and request data and certain types of processing.

Static resources are served through nginx.

### Sockets

Asynchronous requests received by the Django application from the plato client are forwarded to a python data-processing backend to be satisfied.

Once a dataset is loaded, the Django application uses extensive caching to keep raw data and pre-processed data in memory to quickly satisfy client requests within interactive latencies.


### Data-Pipeline

The data-processing backend is very open-ended and requires significant coding to make changes or additions at this point.

There are three main types of classes that are (ideally) used to composed a reusable data processing pipeline for getting useful data to plato clients.

* **DataSource**: Abstracts a file on disk or database to provide raw data to plato through an interface. Must have methods to check if a given file is readable as this type of data-source and for enumerating the stats contained in the data-source. Avoid putting all but the most trivial numerical-processing code in the DataSource layer. These are meant to be interchangeable in the long run.   
* **Adapter**: Takes a DataSource instance and adapts it to make it fit for use in a Generator (processor). The concept of this layer is that disparate data-sources should be processable by the same generators (i.e. most data-sources will want line-plot support) so an adapter class should provide an interface that a specific generator class knows how to consume data from. If a DataSource is intended to be consumed by multiple generators to provide visualizations to the client, it should have multiple adapters - one for each generator.    
* **Generator** (aka processor): Takes raw (and adapted) data and produces data useful for client visualization. These classes should focus on the real visualization logic and any necessary optimizations.
* **Websocket Endpoint** Interprets requests coming from clients and directs them to the correct generator. Then packs the output into a json and binary blob which is how plato communicates data responses to the client. This is entirely implemented in bpEndpoint.py at this point.

These categories are guidelines. Existing code follows them but not perfectly. One cannot easily plug different data-sources into different generators today due to the immaturity of the project, but that is the end goal.


#### Data-Sources

_TODO_


# Deployment

This section talks about considerations for the environment in which plato will run.

## Client-Side

Clients should preferably use Chrome. If there are HTML, CSS, or Javascript issues, try upgrading to a newer version of Chrome.

# Server-Side

### Scaling

_TODO_

### Security

plato's Django Server implementation has authentication. There is no DDOS protection, server-side rate limiting, or other security precautions. There is no detection of malicious or misbehaving clients so plato should be used only with trusted users.

plato also contains some client-side code to browse server-side file-systems in a limited way. This could be a security risk and server-side protections should be implemented if untrusted users are given access to a plato client connecting to a server.

### Persistent Data

The only persistent server-side user-modifiable state in a plato deployment is saved layouts. These are stored in a SQL database and can be shared between multiple plato server instances. Besides the actual simulation data from data-sources, this is currently the only data that must be preserved (or shared) to ensure a new deployment continues working for users without perceived loss of data.

### Server requirements

plato is an interactive application and as such the back-end has important latency constraints to keep the user interface responsive. It is true that the ui is able to wait for data asynchronously and can show users which requests are still awaiting responses, but keeping a user waiting implies reduced productivity.

plato operates on large data sets and attempts to process them on-demand and strives to return results to users with real-time latency (Usually <100ms after loading initial data with an exception for some operations which can take seconds). Some operations require iterating over these data-sets of 100 million rows or more and performing computations or searches. A plato server will need to have enough compute power to accomplish this.

#### Memory

Because of the size of data-sets, plato's extensive caching, and the need to do many real-time calculations across entire data-sets, plato servers benefit greatly from large amounts of DRAM. A host with less than 50G is likely going to be slow for multiple concurrent users. If cached plato data is swapped out of DRAM because of memory constraints, performance will likely drop to unusable levels.

Ideal hosts have 500G or, better yet, over 1TB. A load-balancing system could (and should) be developed to support more servers with lesss memory.

Cache sizes and memory topology appear to play a role in plato response times as well. Since plato operates on large data-sets, larger caches (at any level) are always beneficial. Since plato is a multi-threaded application that tends to operates on the same large data sets across multiple threads at the same time in short bursts, a uniform memory access memory model is preferable to non-uniform access. 

#### CPU

The plato backend is both memory bound and cpu bound. Many request require computations across a large data-set unless that data is already cached. Even then, some requests are always required to recompute responses from the raw data by nature. Therefore, faster CPUs will improve plato response time. 

It is unknown at this time if adding many CPUs or hyperthreading improves plato's responsiveness. Concurrency is limited to one thread per request at the moment and clients may only make a few requests at a time depending on use-case. Memory will likely become the main bottleneck once the application caches most the needed data. While adding more CPUs certainly should not hurt performance, if this change requires switching to an architecture with a NUMA memory model, performance could be reduced in typical uses-cases.

#### Network

plato requires large data-sets from NFS. The bandwidth available for reading from NFS is inversely porportional to the load-time of plato data. The client-server link in plato transfers data at a relatively low bandwidth since the plato backend will downsample data bound for the client. Therefore, plato servers should be located somewhere which can ensure high read bandwidth from NFS (500Mb/s+, ideally). The location of plato servers with respect to clients is not important as long as reasonable websocket bandwidth and latency can be ensured (>1Mb/s and 100ms). Higher bandwidth and lower latency links are always preferred, of course.

#### Other

These servers should not be shared with other applications or users as that would impact the response times of the application and slow users.


# Writing New Widget

_TODO_: plato should really have a tutorial/template widget that can be copied for this purpose.

1. Copy a simple widget that currently works in plato to a new .js file. If you can view data in the widget it will be easy to verify your new widget works.
1. Add your new .js file to viewer.html after the other widget .js files (e.g. `heatmap-widget.js`)
1. Rename your new widget class to something unique
1. Ensure that your widget has the following attributes (see existing widgets for example values). If you copy these values over to your new widget, it should continue working like the one from which it was copied.
   * typename - identifies the type of the widget
   * description - describes the widget for users' convenience
   * processor_type - type of processor (generator) that this widget expects to talk to on the server. This dictates what kind of data comes back.
   * data_type - type of datasource that this widget supports. This dictates what kind of data will get shown in this widget. Ideally, this should be a set so different data-sources will work with the same widget... but that is not implemented yet.
1. Add your widget to the viewer class' `widget_factory` object in `viewer.js` along with the other widgets. This registers the widget so that it will be available in the the plato GUI for construction.
1. Refresh the plato GUI and you should see your widget listed in the "Widget Types" control panel on the left side of the screen. You should be able to instantiate your widget and it should behave like the widget you copied.
1. At this point, you can modify your widget to change its functionality. If you have a server-side generator class to provide a certain type of processed data, switch to that by changing your class' processor_type. Be sure to update your class' `on_update_data` and `on_render` functions to match the data you expect to receive though.

There is an API between the viewer and widgets consisting of widget methods that usually begin with "on_". This API is defined (but very poorly documented) in `base-widget.js`. Some of the most important methods that you can override in a `BaseWidget` subclass are:
 
 
 * `can_follow()` Is the widget allowed to follow the global slider (this should be invariant)
 * `get_processor_kwargs()` When constructing a processor on the server, provide custom arguments to pass to it
 * `get_request_kwargs()` Return an object with data specific to the getData request that this widget will make (e.g. what stats to view)
 * `get_config_data()` Return an object describing the configuration of this widget that can be restored later (save & load support)
 * `apply_config_data(d)` Take the output of `get_config_data` loaded from a saved layout or copied from another widget and apply it to 
 * `on_assign_data()` A new data-source has been assigned to this widget
 * `on_assign_processor()` A new processor has been assigned to this widget
 * `on_update_data(data, meta)` Called when new data is received from the server. This is usually followed by an on_render() event.
 * `on_render()` Called when the widget should render itself
 * `on_clear_stats()` Called when stats should be cleared
 * `on_add_stat(stat_name)` Called when a new stat is added

This is not an exhaustive list and more documentation (and examples) can be found in the plato code.


# Writing new Data Processing

_TODO_


# Generating Data

See `capturing_hdf5_with_rome.md`

# Testing

Run:

  cd py;
  tests/run_tests
