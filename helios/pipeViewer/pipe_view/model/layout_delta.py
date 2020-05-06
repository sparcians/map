

# Selection Mgrs are in charge of drawing to the canvas a demonstration of
# the current selection, hence importing wx
import wx
import sys

from model.element import Element

## Represents a checkpoint between two layouts.
#  The checkpoint can be applied or removed. The content of the checkpoint can
#  effecively be thought of as a selection. Checkpoints can have a net null
#  effect if they are meant simply to represent a change in selection
class Checkpoint(object):

    ## Initialize the checkpoint. Before the checkpoint is used, after() should
    #  be called to capture the state AFTER the checkpoint
    #  @desc Description of the change
    #@profile
    def __init__(self, layout, before, desc):
        assert layout is not None
        self.__layout = layout
        self.__description = desc
        self.__before_pins = [el.GetPIN() for el in before]
        self.__before = [self.__layout.CreateElement(el, initial_properties = el.GetProperties(), element_type=el.GetProperty('type'), parent=el.GetParent()) for el in before]
        self.__before_follows_pins = self.__layout.DeterminePriorPins(before)
        self.__after_pins = []
        self.__after = []
        self.__after_follows_pins = []

    ## Sets checkpoint state afterward
    def after(self, after):
        self.__after_pins = [el.GetPIN() for el in after]
        self.__after = [self.__layout.CreateElement(el, initial_properties = el.GetProperties(), element_type=el.GetProperty('type'), parent=el.GetParent()) for el in after]
        self.__after_follows_pins = self.__layout.DeterminePriorPins(after)

    ## Decsription of the change captured by this checkpoint
    @property
    def description(self):
        return self.__description

    ## Apply the checkpoint
    #  Returns a 2-tuple: (failures, created_elements). Number of failures is 0 on success, or a
    #  number of elements in the 'before' state that could not be found (likely indicating that the
    #  delta should not be applied here). Also counts PIN collisions when trying to add elements
    #  with PINs that were already in the layout. Checkpoint is still applied as much as possible
    #  since failures here are probably not recoverable. The second item in the result tuple is
    #  al of the elements modified after applying this delta. This should generally be used as the
    #  user's new selection
    def apply(self):
        errors = 0
        # Find all 'before' elements to make sure we're in a sane state
        #print 'delta apply. Removing {}, Adding {}'.format(self.__before_pins, self.__after_pins)
        for pin in self.__before_pins:
            el = self.__layout.FindByPIN(pin)
            if el is not None:
                self.__layout.RemoveElement(el)
            else:
                print('Error while applying layout delta. Element with pin {} could not be found'.format(pin), file=sys.stderr)
                errors += 1

        # Add all the 'after' elements
        created_elements = []
        for el,pin,follows_pins in zip(self.__after, self.__after_pins, self.__after_follows_pins):
            if self.__layout.FindByPIN(pin) is not None:
                print('Error while applying layout delta. Element with pin {} already exists.'.format(pin), file=sys.stderr)
                errors += 1
            else:
                eltype = el.GetProperty('type')
                has_parent = el.GetParent() is not None
                new_el = self.__layout.CreateAndAddElement(el, initial_properties = el.GetProperties(), element_type=eltype, parent=el.GetParent(), force_pin=pin, follows_pins=follows_pins, skip_list = has_parent)
                if has_parent:
                    el.GetParent().AddChild(new_el)
                    el.GetParent().SetNeedsRedraw()
                    new_el.SetNeedsRedraw()
                created_elements.append(new_el)
                assert new_el.GetPIN() == pin

        return (errors, created_elements)

    ## Remove the checkpoint (revert)
    #  Returns a 2-tuple: (failures, created_elements). Number of failures is 0 on success, or a
    #  number of elements in the 'after' state that could not be found (likely indicating that the
    #  delta should not be applied here). Also counts PIN collisions when trying to add elements
    #  with PINs that were already in the layout. Checkpoint is still removed as much as possible
    #  since failures here are probably not recoverable. The second item in the result tuple is
    #  al of the elements modified after applying this delta. This should generally be used as the
    #  user's new selectiont
    def remove(self):
        errors = 0
        # Find all 'after' elements to make sure we're in a sane state
        #print 'delta remove. Removing {}, Adding {}'.format(self.__after_pins, self.__before_pins)
        for pin in self.__after_pins:
            el = self.__layout.FindByPIN(pin)
            if el is not None:
                self.__layout.RemoveElement(el)
            else:
                print('Error while removing layout delta. Element with pin {} could not be found'.format(pin), file=sys.stderr)
                errors += 1

        # Add all the 'after' elements
        created_elements = []
        for el,pin,follows_pins in zip(self.__before, self.__before_pins, self.__before_follows_pins):
            if self.__layout.FindByPIN(pin) is not None:
                print('Error while removing layout delta. Element with pin {} already exists.'.format(pin), file=sys.stderr)
                errors += 1
            else:
                eltype = el.GetProperty('type')
                has_parent = el.GetParent() is not None
                new_el = self.__layout.CreateAndAddElement(el, initial_properties = el.GetProperties(), element_type=eltype, parent=el.GetParent(), force_pin=pin, follows_pins=follows_pins, skip_list=has_parent)
                if has_parent:
                    el.GetParent().AddChild(new_el)
                    el.GetParent().SetNeedsRedraw()
                    new_el.SetNeedsRedraw()
                created_elements.append(new_el)
                assert new_el.GetPIN() == pin

        return (errors, created_elements)
