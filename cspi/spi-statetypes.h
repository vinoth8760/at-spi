#ifndef _SPI_STATETYPES_H_
#define _SPI_STATETYPES_H_

/*
 *
 * Enumerated type for accessible state
 *
 */

typedef enum
{
  SPI_STATE_INVALID,
  /* Indicates a window is currently the active window */
  SPI_STATE_ACTIVE,
  /* Indicates that the object is armed */
  SPI_STATE_ARMED,
  /* Indicates the current object is busy */
  SPI_STATE_BUSY,
  /* Indicates this object is currently checked */
  SPI_STATE_CHECKED,
  /* Indicates this object is collapsed */
  SPI_STATE_COLLAPSED,
  /* Indicates the user can change the contents of this object */
  SPI_STATE_EDITABLE,
  /* Indicates this object allows progressive disclosure of its children */
  SPI_STATE_EXPANDABLE,
  /* Indicates this object its expanded */
  SPI_STATE_EXPANDED,
  /*
   * Indicates this object can accept keyboard focus, which means all
   * events resulting from typing on the keyboard will normally be passed
   * to it when it has focus
   */
  SPI_STATE_FOCUSABLE,
  /* Indicates this object currently has the keyboard focus */
  SPI_STATE_FOCUSED,
  /* Indicates the orientation of thsi object is horizontal */
  SPI_STATE_HORIZONTAL,
  /* Indicates this object is minimized and is represented only by an icon */
  SPI_STATE_ICONIFIED,
  /*
   * Indicates something must be done with this object before the user can
   * interact with an object in a different window.
   */
  SPI_STATE_MODAL,
  /* Indicates this (text) object can contain multiple lines of text */
  SPI_STATE_MULTI_LINE,
  /*
   * Indicates this object allows more than one of its children to be
   * selected at the same time
   */
  SPI_STATE_MULTISELECSPI_TABLE,
  /* Indicates this object paints every pixel within its rectangular region. */
  SPI_STATE_OPAQUE,
  /* Indicates this object is currently pressed */
  SPI_STATE_PRESSED,
  /* Indicates the size of this object is not fixed */
  SPI_STATE_RESIZABLE,
  /*
   * Indicates this object is the child of an object that allows its
   * children to be selected and that this child is one of those children
   * that can be selected.
   */
  SPI_STATE_SELECSPI_TABLE,
  /*
   * Indicates this object is the child of an object that allows its
   * children to be selected and that this child is one of those children
   * that has been selected.
   */
  SPI_STATE_SELECTED,
  /* Indicates this object is sensitive */
  SPI_STATE_SENSITIVE,
  /*
   * Indicates this object, the object's parent, the object's parent's
   * parent, and so on, are all visible
   */
  SPI_STATE_SHOWING,
  /* Indicates this (text) object can contain only a single line of text */
  SPI_STATE_SINGLE_LINE,
  /* Indicates this object is transient */
  SPI_STATE_TRANSIENT,
  /* Indicates the orientation of this object is vertical */
  SPI_STATE_VERTICAL,
  /* Indicates this object is visible */
  SPI_STATE_VISIBLE,
  SPI_STATE_LAST_DEFINED
} AccessibleState;

#endif