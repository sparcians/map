

import yaml
import hashlib as md5
import os
import wx
import time
import logging
import pickle

import model.element_types as etypes
from .plain_file import Plain


# # The reason this application exists, Layouts allow users to group together
#  the elements they are viewing
#
#  Layouts can only be loaded at Layout.__init__. To load a new layout, a new
#  Layout instance must be created.
class Layout(object):

    # # File name and extension for layout files
    LAYOUT_FILE_EXTENSION = '.alf'

    # # Layout file wildcard for use in file selection dialog boxes
    LAYOUT_FILE_WILDCARD = 'Argos layout files (*{0})|*{0}' \
                           .format(LAYOUT_FILE_EXTENSION)

    # # Code at start of each precompiled alf file (alfc) indicating its version.
    #  @note This Layout class can only load specific versions.
    #  @note Must not contain any whitespace!
    #  @note There is likely no need to support historic versions assuming
    #  people don't delete the source alf's. Worst case scenary is some
    #  additional reload time each time this code is updated.
    PRECOMPILED_VERSION_CODE = 'argos-alfc-v3'.strip()

    # # Initialize two lists to store Elements and registerd Layout_Contexts
    #  @param filename Layout file to parse and open. This filename and a
    #  checksum will be stored in case the file is to be stored again following
    #  changes. If None or '', no file is loaded
    #  @param workspace Argos Workspace object if one is available
    #  @note Layout files can only be loaded during construction
    def __init__(self, filename = None, workspace = None):
        if filename is not None and not isinstance(filename, str):
            raise TypeError('filename must be None or a str, is {0}'.format(type(filename)))

        logging.info("Constructing layout \"{0}\"".format(filename))
        t_start = time.time()

        self.__elements = []
        self.__elements_by_pin = {}
        self.__elements_with_children = []
        self.__graph_nodes = {} # fast access
        self.__workspace = workspace
        self.lay_cons = []

        self.__file_checksum = None # Checksum of file as loaded
        self.__filename = None # Name of file loaded

        if filename:
            self.__LoadLayout(str(filename)) # Load elements (only called during init where no elements are present).
            # __filename & __file_checksum are already set

        # At end of construction, mark as clean
        self.__changed = False

        logging.info("Layout loaded with {0} elements in {1}s".format(len(self.__elements), (time.time() - t_start)))

    # # Returns the workspace associated with this layout
    def GetWorkspace(self):
        return self.__workspace

    # # This should only be called by a Layout_Context
    #  Causes the Layout_Context to be registered with the Layout
    def LinkLayoutContext(self, lay_con):
        self.lay_cons.append(lay_con)

    # # This should only be called by a Layout_Context
    #  Unregisters the Layout_Context from the Layout
    def UnLinkLayoutContext(self, lay_con):
        if lay_con in self.lay_cons:
            self.lay_cons.remove(lay_con)

    # # Tells whether any Layout_Contexts are currently registered with this Layout
    def HasContext(self):
        if len(self.lay_cons) > 0:
            return True
        return False

    # # Determine PINs of elements preceding the given list of elements' pins.
    #  @return List of sequences of all preceeding pins:
    #     e.g. [[1,2], [1,2,3,4], [1,2,3,4,5,6,7]]
    #  Result is in same order as input pins. May contain [-1] to indicate that
    #  an input pin refers to an element at the start of the element list. May
    #  contain None to indicate that a pin was never found.
    #
    #  It is important to return a list of ALL pins leading up to each input PIN
    #  because some element sets (such as a quad_tree) will contain nodes having
    #  only some elements within it. Inserting an element at the right point in
    #  that list requires finding a point after all known preceeding pins
    #  available in that list.
    #
    #  @warning This may use lots of memory for large displays and should be
    #  tested for scalability
    # @profile
    def DeterminePriorPins(self, els):
        pin_map = {} # {pin:pin_index}
        results = [[]] * len(els) # Pins preceeding pin of each element in els. Initialize with empty values

        for idx, e in enumerate(els):
            pin_map[e.GetPIN()] = idx
            parent = e.GetParent()
            if parent is not None:
                results[idx].append(parent.GetPIN()) # Previous pin of this element is paren's pin. Should look for siblings though

        # Fill in prev_pin values
        prev_pin = -1 # Indicates start of list
        for e in self.__elements:
            p = e.GetPIN()
            if p in pin_map:
                results[pin_map[p]].append(prev_pin)
            prev_pin = p

        assert [] not in results, \
               '{} pins not found when determining prior pins for {}. Reslts={}' \
               .format(results.count([]), [e.GetPIN() for e in els], results)
        return results

    # # Creates and returns a new element. Does not add it to the layout.
    # @profile
    def CreateElement(self, duplicate = None, initial_properties = None, element_type = None, parent = None, **kwargs):
        if not element_type:
            # Default for compatibility
            element_type = 'box'
        el_class = etypes.creatables.get(element_type)
        if el_class:
            e = el_class(duplicate, self, initial_properties, parent, **kwargs)
        else:
            raise Exception("CreateElement: Unknown Element Type \"%s\"" % element_type)

        return e

    # # Adds a new element to the list and informs any subscribed Layout
    #  Contexts, supports duplicating an Element that is passed in as a parameter
    #  @param duplicate Optional element from which this element will copy
    #  properties
    #  @param initial_properties Properties dictionary that overrides any
    #  defaults
    #  @return Element created
    #  @see Element.__init__
    # @profile
    def CreateAndAddElement(self, duplicate = None, initial_properties = None, element_type = None, parent = None, **kwargs):
        add_kwargs = {}
        if 'follows_pins' in kwargs:
            add_kwargs['follows_pins'] = kwargs['follows_pins'] # Forward argument
            del kwargs['follows_pins']
        if 'skip_list' in kwargs:
            add_kwargs['skip_list'] = kwargs['skip_list'] # Forward argument
            del kwargs['skip_list']

        e = self.CreateElement(duplicate, initial_properties, element_type, parent, **kwargs)

        self.AddElement(e, **add_kwargs)

        # #logging.debug("Element created in layout")
        return e

    # # Removes the Element from the list and instructs all subscribed Layout
    #  Contexts to do the same
    def RemoveElement(self, e):
        for element in self.__elements:
            # initialize recurisve search and if we've already found something in base level
            # indicate that by setting chop_all
            pruned = self.__PruneOnElement(e, element)
            if pruned:
                pruned = set(pruned) # don't delete things twice
                break # found
        else:
            raise Exception('Zombie objects: unable to remove %s' % e)
        for lay_con in self.lay_cons:
            for el in pruned:
                lay_con.RemoveElement(el)
        self.__changed = True
        if e in self.__elements:
            self.__elements.remove(e)
            del self.__elements_by_pin[e.GetPIN()]
        if e.GetParent() is not None:
            e.GetParent().SetNeedsRedraw()

    # # Adds the given element to the list and instructs all subscribed Layout
    #  Contexts to do the same
    #  @param follows_pin=PIN Pin of element after which to insert this element
    #  @param skip_list=Skips adding element to the local list if true
    # @profile
    def AddElement(self, e, **kwargs):
        follows_pins = kwargs.get('follows_pins', [None])
        skip_list = kwargs.get('skip_list', False)
        if not skip_list:
            self.__InsertElementAfterPIN(e, follows_pins[-1])
            # #self.__elements.append(e)
            self.__elements_by_pin[e.GetPIN()] = e
            if e.HasChildren():
                self.__elements_with_children.append(e)
        if e.GetType() == 'node':
            self.__graph_nodes[e.GetProperty('name')] = e
        for lay_con in self.lay_cons:
            lay_con.AddElement(e, after_pins = follows_pins)
        self.__changed = True

    # # Return graph nodes within this layout.
    #  @warning Do not modify the result
    def GetGraphNodes(self):
        return self.__graph_nodes

    # # If a property (t_offset or LocationString) for an Element is changed
    #  such that it will no longer be correctly sorted in the OrderedDict,
    #  this method will figure out where it goes
    def ReSort(self, e, t_off, loc):
        for lay_con in self.lay_cons:
            lay_con.ReSort(e, t_off, loc)

    # # If a property for an Element indicating position or size is changed,
    #  this method is invoked. Alerts all layout contexts110
    def ElementsMoved(self, e):
        for lay_con in self.lay_cons:
            lay_con.ElementMoved(e)

    def Refresh(self, e):
        for lay_con in self.lay_cons:
            lay_con.ReValue(e)

    # # Find an element by its PIN
    #  @return The element with the given pin if found. Otherwise returns None
    def FindByPIN(self, pin):
        el = self.__elements_by_pin.get(pin, None)
        # If we didn't find the element, it might be because it's a child of another element
        # Search those next
        if el is None:
            for parent_el in self.__elements_with_children:
                el = parent_el.GetChildByPIN(pin)
                if el is not None:
                    break
        return el

    # # Returns the list (un-sorted) of all Elements in the Layout
    #  It is not safe to modify this list
    def GetElements(self):
        return self.__elements

    # @ Returns filename used by latest SaveToFile() either explicitly or
    #  implicitly. Or, if SaveToFile has not yet been called, returns the
    #  construction filename. If no construction filename was specified either,
    #  returns None.
    def GetFilename(self):
        return self.__filename

    # @ Returns file checksum obtained by latest SaveToFile call. Or, if
    #  SaveToFile has not yet been called, returns the construction filename.
    #  If no construction filename was specified either, returns None.
    def GetFileChecksum(self):
        return self.__file_checksum

    # # Import plain file from Neato into Argos
    #  @return List of all elements created by this import
    def ImportPlain(self, filename):
        elements = []
        pf = Plain(filename)
        for name, val in list(pf.nodes.items()):
            # convert name to hex
            if name.isdigit():
                name = '%x' % int(name)

            pos = int(float(val[pf.NODE_POSX]) * 40), int(float(val[pf.NODE_POSY]) * 40)
            dims = (62, 18) # (56, 13)
            # # @todo Tweak dimensions. This might be done dynamically after placing all nodes by
            #  seeing how big they can get without colliding with their neighbors
            props = {'position':pos, 'name':name, 'dimensions': dims, 'data':val[pf.NODE_DATA]}
            el = self.CreateAndAddElement(initial_properties = props, element_type = 'node')
            elements.append(el)

        for edge in pf.edges:
            # convert to hex
            if edge[0].isdigit():
                edge0 = '%x' % int(edge[0])
            else:
                edge0 = edge[0]
            if edge[1].isdigit():
                edge1 = '%x' % int(edge[1])
            else:
                edge1 = edge[1]

            self.__graph_nodes[edge0].AddConnectionOut(self.__graph_nodes[edge1])
            self.__graph_nodes[edge1].AddConnectionIn(self.__graph_nodes[edge0])

        return elements

    # # Store the layout to a file as YAML. Preserves draw order
    #  @param filename. If None, stores to the current filename returned by
    #  GetFilename. If there is no current filename, raises ValueError.
    #  If a str, attempts to store to this file. If None, effectively behaves
    #  like a typical "save" feature. If a str, behaves like a "save-as".
    def SaveToFile(self, filename = None):
        if filename is None:
            filename = self.__filename
        elif not isinstance(filename, str):
            raise TypeError('filename must be None or a str, is {0}'.format(type(filename)))
        if filename == '':
            raise ValueError('filename cannot be an empty string')
        if self.__filename == filename:
            if not self.CanSaveToFile():
                raise IOError('Cannot save layout {0} back to file "{1}" without overwriting other changes in that file' \
                              .format(self, filename))

        try:
            with open(filename, 'w') as f:
                self._StoreYAMLToStream(f)
        except Exception as ex:
            # Failed to save. Allow this to propagate up
            raise

        # Store new file info
        self.__filename = os.path.abspath(filename)
        self.__file_checksum = self.__ComputeChecksum(filename)

        # Write the updated precompiled layout file
        self.__WritePrecompiledLayout(self.__GenPrecompiledLayoutFilename(filename))

        # Just saved, layout is now unchanged
        self.__MarkAsUnchanged()

    # # Determines if this can be stored to the current GetFilename() without
    #  overwriting changes to that file. If GetFilename returns None, this
    #  always returns False.
    #  @note Does not determine write-access for destination directory
    #  @return True if SaveToFile() can safely be called without a filename
    #  and False if SaveToFile must specify a filename different from the
    #  current filename or delete the current filename.
    #
    #  For filenames other than the current GetFilename(), this Layout just
    #  assumes it can write without consequence
    def CanSaveToFile(self):
        if self.__filename is None:
            return False
        if not os.path.exists(self.__filename):
            return True # File was deleted?
        with open(self.__filename, 'r') as f:
            digest = self.__ComputeChecksum(self.__filename)
            if digest != self.__file_checksum:
                return False # File has changed

        return True

    # # Indicates whether this Layout or any of its comprising elements have
    #  been modified in any way that could affect the save state on disk
    #  @return True if this layout or elements have changed
    def HasChanged(self):
        if self.__changed == True:
            return True

        for c in self.__elements:
            if c.HasChanged():
                return True

        return False

    # # Mark the layout as changed. Ideally, this is not needed since HasChanged
    #  inspects elements for their changes.
    def SetChanged(self):
        self.__changed = True

    # # Insert an element following an element with the given pin
    def __InsertElementAfterPIN(self, e, after_pin = -1):
        # #print 'Inserting into layout {} after PIN {}'.format(e, after_pin)
        if after_pin == None:
            self.__elements.append(e)
            # #print '  @back'
        elif after_pin == -1:
            self.__elements.insert(0, e)
            # #print '  @front'
        else:
            for idx, el in enumerate(self.__elements):
                if el.GetPIN() == after_pin:
                    self.__elements.insert(idx + 1, e)
                    # #print '  @{}'.format(idx+1)
                    break
            else:
                self.__elements.append(e)
                # #print '  @back (2) ap={}'.format(after_pin)

    # # Recursive removal of element and its children, returns the pruned elements
    def __PruneOnElement(self, e, element, chop_all = False):
        if e == element or chop_all:
            chop_all = True
            chopped = [element]
        else:
            chopped = []
        if element.HasChildren() and element.GetChildren():
            children = element.GetChildren()
            for child in children:
                if chop_all: # collection mode used on nodes subordinate to target node
                    chopped.extend(self.__PruneOnElement(e, child, chop_all = True))
                else: # keep going
                    if child == e: # found our query
                        element.RemoveChild(child)
                        return self.__PruneOnElement(e, child, chop_all = True)
                    else: # not found yet, delve deeper
                        out = self.__PruneOnElement(e, child)
                        if out: # found something
                            return out
        return chopped

    # # Store the layout to a given stream as a new YAML stream without any
    #  safety checks about the stream itself.
    #  Preserves draw order
    #  @param stream Stream with writeability. Layout representation will be
    #  written to this stream.
    #  @note Does not update filename or file_checksum attributes
    #  @note Emits YAML stream start/end events, so storing multiple layouts in
    #  1 file might not be parseable
    def _StoreYAMLToStream(self, stream):
        events = [yaml.StreamStartEvent(encoding = 'ascii'),
                  yaml.DocumentStartEvent(explicit = True),
                  yaml.SequenceStartEvent(anchor = None, tag = None, implicit = True, flow_style = False),
                  ]

        # Serialize all elements in order
        for e in self.__elements:
            events.extend(e._GetYAMLEvents())

        events.extend([yaml.SequenceEndEvent(),
                       yaml.DocumentEndEvent(explicit = True),
                       yaml.StreamEndEvent(),
                       ])

        yaml.emit(events, stream)

    # # Computes a checksum of the file specified
    def __ComputeChecksum(self, filename):
        f = open(filename, 'r')
        return md5.md5(f.read().encode('utf-8')).hexdigest()

    # #Loads an element from YAML, returns element
    def __LoadElement(self, elinfo, idx, filename, parent = None):
        if not isinstance(elinfo, dict):
            raise ValueError('Element {0} in Argos layout file "{1}" was not a map structure, was a {2}' \
                        .format(idx, filename, type(elinfo)))

        # Peek at type before further reading
        el_type = elinfo.get('type')
        # # TODO: reintroduce unknown property checking
        e = self.CreateElement(initial_properties = elinfo, element_type = el_type, parent = parent)
        # load children if any exist
        children = elinfo.get('children')
        if e.HasChildren() and children:
            for child in children:
                e.AddChild(self.__LoadElement(child, idx, filename, parent = e))
        return e

    # # Loads layout from a YAML file
    #  @param filename File to load
    #  @throw Raises an Exception if file cannot be opened or there are errors
    #  parsing
    #  @pre This layout must have no elements. They will not be cleared.
    #  @note If this layout has layout contexts referencing it, they will be
    #  notified of added elements
    #  @note Sets __filename and __file_checksum
    #  @todo Parse YAML events or tokens with Marks so that Layout-file semantic
    #  errors in the layout file can be pinpointed to a line/col.
    def __LoadLayout(self, filename):
        # Store filename before loading the layout since some elements care
        # about this for determining resource locations
        self.__filename = os.path.abspath(filename) # Name of file loaded

        precompiled_filename = self.__GenPrecompiledLayoutFilename(filename)
        loaded_precompiled = self.__TryLoadPrecompiledLayout(filename, precompiled_filename)

        if not loaded_precompiled:
            # No precompiled file (or out of date)
            with open(filename, 'r') as f:
                file_data = f.read()
                try:
                    d = yaml.safe_load(file_data)
                except Exception as e:
                    raise Exception("Failed to load yaml file \"{0}\":\n{1}".format(filename, str(e)))

            if not hasattr(d, '__iter__'):
                raise ValueError('Data retrieved from Argos layout file "{0}" is not an iterable structure : \n{1}' \
                                 .format(filename, d))
            # Children of elements stored inside elements in Layout.
            # LayoutContext flattens Layout for ease-of-rendering and querying.
            # Get each element
            for idx, elinfo in enumerate(d):
                e = self.__LoadElement(elinfo, idx, filename)
                self.AddElement(e)

        # build graph hashmap
        for e in self.__elements:
            if e.GetType() == 'node':
                e.BuildConnectionsFromProperty(self.__graph_nodes)

        # # @todo Use file date modified instead - probably faster
        self.__file_checksum = self.__ComputeChecksum(filename) # Checksum of file as loaded

        # Store the precompiled alf
        #if not loaded_precompiled:
        #    self.__WritePrecompiledLayout(precompiled_filename)

        # Just loaded - No changes
        self.__MarkAsUnchanged()

    # # Load individual element from pickle
    def __LoadElementFromPickle(self, pdict, parent = None):
        children = pdict.get('children')
        if children:
            # child property only is generated at compile time. Strip out.
            children = pdict.pop('children')

        e = self.CreateElement(initial_properties = pdict, element_type = pdict.get('type'), parent = parent)
        if children:
            for child in children:
                e.AddChild(self.__LoadElementFromPickle(child, parent = e))

        return e

    # # Loads a precompiled layout file
    #  @return True if load was successful and False if not.
    #  @note Logs to warning and debug about loading issues
    # #@profile
    def __TryLoadPrecompiledLayout(self, filename, precompiled_filename):
        if not os.path.exists(precompiled_filename):
            return False

        logging.info('Attempting to precompiled layout {}'.format(precompiled_filename))

        # Open precompiled alf and load header lines
        f = open(precompiled_filename, 'rb')

        # Compare
        pc_ver_code = f.readline().strip()
        if pc_ver_code != self.PRECOMPILED_VERSION_CODE:
            logging.info('Precompiled alf "{}" is in an older format. Version code "{}" differs ' \
                         'from current precompiled layout version code "{}"'
                         .format(precompiled_filename, pc_ver_code, self.PRECOMPILED_VERSION_CODE))
            return False

        # Compare checksum
        pc_checksum = f.readline().strip()
        alf_checksum = self.__ComputeChecksum(filename)
        if pc_checksum != alf_checksum:
            logging.info('Precompiled alf "{}" is out of date. checksum {} differs from checkum ' \
                         '{} of alf file {}:' \
                         .format(precompiled_filename, pc_checksum, alf_checksum, filename))
            return False

        try:
            # Load rest of the precompiled file
            properties = pickle.loads(f.read())
        except Exception as ex:
            logging.warn('Found but failed to load "{}": {}'.format(precompiled_filename, str(ex)))
            return False

        for pdict in properties:
            e = self.__LoadElementFromPickle(pdict)
            self.AddElement(e)
        return True

    def __MakeSerializable(self, el):
        output = {}
        output = el.GetSerializableProperties() # get all fields
        if el.HasChildren():
            children = el.GetChildren()
            output['children'] = []
            for child in children:
                output['children'].append(self.__MakeSerializable(child))

        return output

    # # Writes a precompiled layout file having the given name
    def __WritePrecompiledLayout(self, precompiled_filename):
        try:
            f = open(precompiled_filename, 'wb')
        except IOError as ex:
            if ex.errno != 13: # Permission error
                raise # Only permission errors are expected
            logging.info('Failed to open precompiled alf file for writing {}' \
                         .format(precompiled_filename))
        else:
            try:
                f.write((self.PRECOMPILED_VERSION_CODE + '\n').encode('utf-8'))
                f.write((str(self.__file_checksum) + '\n').encode('utf-8'))
                f.write(pickle.dumps([self.__MakeSerializable(el) for el in self.__elements],
                                      pickle.HIGHEST_PROTOCOL))
            except Exception as ex:
                logging.info('Encountered error writing content to precompiled alf file {}. ' \
                             'File was opened, but the error occurred when writing content: {}' \
                             .format(precompiled_filename, ex))
            else:
                logging.info('Successfully wrote precompiled alf: {}'.format(precompiled_filename))

    # # Generates precompiled filename for a given layout filename
    def __GenPrecompiledLayoutFilename(self, filename):
        return filename + 'c' # e.g. layout.alfc

    # # Flags this layout and all comprising elements as Not Changed.
    #  This is intended to be called when a layout is being loaded or saved
    def __MarkAsUnchanged(self):
        self.__changed = False
        for c in self.__elements:
            c._MarkAsUnchanged()

    def __str__(self):
        return '<Layout "{}">'.format(self.GetFilename())

    def __repr__(self):
        return self.__str__()

    # # Add a region of two offsets from current time to ask for when updated.
    def GetElementDump(self):
        return repr(self.__elements)
