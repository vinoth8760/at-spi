#include <cspi/spi-private.h>

/**
 * AccessibleSelection_ref:
 * @obj: a pointer to the #AccessibleSelection implementor on which to operate.
 *
 * Increment the reference count for an #AccessibleSelection object.
 *
 * Returns: (no return code implemented yet).
 *
 **/
int
AccessibleSelection_ref (AccessibleSelection *obj)
{
  Accessibility_Selection_ref (*obj, spi_ev ());
  return 0;
}


/**
 * AccessibleSelection_unref:
 * @obj: a pointer to the #AccessibleSelection implementor on which to operate. 
 *
 * Decrement the reference count for an #Accessible object.
 *
 * Returns: (no return code implemented yet).
 *
 **/
int
AccessibleSelection_unref (AccessibleSelection *obj)
{
  Accessibility_Selection_unref (*obj, spi_ev ());
  return 0;
}



/**
 * AccessibleSelection_getNSelectedChildren:
 * @obj: a pointer to the #AccessibleSelection implementor on which to operate.
 *
 * Get the number of children of an #AccessibleSelection implementor which are
 *        currently selected.
 *
 * Returns: a #long indicating the number of #Accessible children
 *        of the #AccessibleSelection implementor which are currently selected.
 *
 **/
long
AccessibleSelection_getNSelectedChildren (AccessibleSelection *obj)
{
  return (long)
    Accessibility_Selection__get_nSelectedChildren (*obj, spi_ev ());
}


/**
 * AccessibleSelection_getSelectedChild:
 * @obj: a pointer to the #AccessibleSelection on which to operate.
 * @selectedChildIndex: a #long indicating which of the selected
 *      children is specified.
 *
 * Get the i-th selected #Accessible child of an #AccessibleSelection.
 *      Note that @childIndex refers to the index in the list of 'selected'
 *      children and generally differs from that used in
 *      #Accessible_getChildAtIndex() or returned by
 *      #Accessible_getIndexInParent(). @selectedChildIndex must lie between 0
 *      and #AccessibleSelection_getNSelectedChildren()-1, inclusive.
 *
 * Returns: a pointer to a selected #Accessible child object,
 *          specified by @childIndex.
 *
 **/
Accessible *
AccessibleSelection_getSelectedChild (AccessibleSelection *obj,
                                      long int selectedChildIndex)
{
  Accessibility_Accessible child = 
    Accessibility_Selection_getSelectedChild (*obj,
					      (CORBA_long) selectedChildIndex, spi_ev ());
  spi_warn_ev (spi_ev (), "getSelectedChild");

  return (Accessible *) spi_object_add (child);
}

/**
 * AccessibleSelection_selectChild:
 * @obj: a pointer to the #AccessibleSelection on which to operate.
 * @childIndex: a #long indicating which child of the #Accessible
 *              is to be selected.
 *
 * Add a child to the selected children list of an #AccessibleSelection.
 *         For #AccessibleSelection implementors that only allow
 *         single selections, this may replace the (single) current
 *         selection.
 *
 * Returns: #TRUE if the child was successfully selected, #FALSE otherwise.
 *
 **/
boolean
AccessibleSelection_selectChild (AccessibleSelection *obj,
                                 long int childIndex)
{
  return (boolean)
    Accessibility_Selection_selectChild (*obj,
					 (CORBA_long) childIndex, spi_ev ());
}


/**
 * AccessibleSelection_deselectSelectedChild:
 * @obj: a pointer to the #AccessibleSelection on which to operate.
 * @selectedChildIndex: a #long indicating which of the selected children
 *              of the #Accessible is to be selected.
 *
 * Remove a child to the selected children list of an #AccessibleSelection.
 *          Note that @childIndex is the index in the selected-children list,
 *          not the index in the parent container.  @selectedChildIndex in this
 *          method, and @childIndex in #AccessibleSelection_selectChild
 *          are asymmettric.
 *
 * Returns: #TRUE if the child was successfully deselected, #FALSE otherwise.
 *
 **/
boolean
AccessibleSelection_deselectSelectedChild (AccessibleSelection *obj,
                                           long int selectedChildIndex)
{
  return Accessibility_Selection_deselectSelectedChild (
	  *obj, (CORBA_long) selectedChildIndex, spi_ev ());
}



/**
 * AccessibleSelection_isChildSelected:
 * @obj: a pointer to the #AccessibleSelection implementor on which to operate.
 * @childIndex: an index into the #AccessibleSelection's list of children.
 *
 * Determine whether a particular child of an #AccessibleSelection implementor
 *        is currently selected.  Note that @childIndex is the index into the
 *        standard #Accessible container's list of children.
 *
 * Returns: #TRUE if the specified child is currently selected,
 *          #FALSE otherwise.
 *
 **/
boolean
AccessibleSelection_isChildSelected (AccessibleSelection *obj,
                                     long int childIndex)
{
  return (boolean)
    Accessibility_Selection_isChildSelected (*obj,
					     (CORBA_long) childIndex, spi_ev ());
}



/**
 * AccessibleSelection_selectAll:
 * @obj: a pointer to the #AccessibleSelection implementor on which to operate.
 *
 * Attempt to select all of the children of an #AccessibleSelection implementor.
 * Not all #AccessibleSelection implementors support this operation.
 *
 * Returns: #TRUE if successful, #FALSE otherwise.
 *
 **/
boolean
AccessibleSelection_selectAll (AccessibleSelection *obj)
{
  Accessibility_Selection_selectAll (*obj, spi_ev ());
  return TRUE; /* TODO: change the bonobo method to return boolean */
}



/**
 * AccessibleSelection_clearSelection:
 * @obj: a pointer to the #AccessibleSelection implementor on which to operate.
 *
 * Clear the current selection, removing all selected children from the
 *       specified #AccessibleSelection implementor's selection list.
 *
 **/
void
AccessibleSelection_clearSelection (AccessibleSelection *obj)
{
  Accessibility_Selection_clearSelection (*obj, spi_ev ());
}


