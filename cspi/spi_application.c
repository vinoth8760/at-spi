/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2001, 2002 Sun Microsystems Inc.,
 * Copyright 2001, 2002 Ximian, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 *
 * AccessibleApplication function prototypes
 *
 */

#include <cspi/spi-private.h>
#include <locale.h>

/**
 * AccessibleApplication_ref:
 * @obj: a pointer to the #AccessibleApplication on which to operate.
 *
 * Increment the reference count for an #AccessibleApplication.
 **/
void
AccessibleApplication_ref (AccessibleApplication *obj)
{
  cspi_object_ref (obj);
}

/**
 * AccessibleApplication_unref:
 * @obj: a pointer to the #AccessibleApplication object on which to operate.
 *
 * Decrement the reference count for an #AccessibleApplication.
 **/
void
AccessibleApplication_unref (AccessibleApplication *obj)
{
  cspi_object_unref (obj);
}

/**
 * AccessibleApplication_getToolkitName:
 * @obj: a pointer to the #AccessibleApplication to query.
 *
 * Get the name of the UI toolkit used by an #AccessibleApplication.
 *
 * Returns: a UTF-8 string indicating which UI toolkit is
 *          used by an application.
 **/
char *
AccessibleApplication_getToolkitName (AccessibleApplication *obj)
{
  char *retval;

  cspi_return_val_if_fail (obj != NULL, NULL);

  retval =
    Accessibility_Application__get_toolkitName (CSPI_OBJREF (obj),
						cspi_ev ());

  cspi_return_val_if_ev ("toolkitName", NULL);

  return retval;
}

/**
 * AccessibleApplication_getVersion:
 * @obj: a pointer to the #AccessibleApplication being queried.
 *
 * Get the version of the UI toolkit used by an
 *      #AccessibleApplication instance.
 *
 * Returns: a UTF-8 string indicating the version of the UI toolkit
 *          which is used by an application.
 **/
char *
AccessibleApplication_getVersion (AccessibleApplication *obj)
{
  char *retval;

  cspi_return_val_if_fail (obj != NULL, NULL);

  retval =
    Accessibility_Application__get_version (CSPI_OBJREF (obj),
					    cspi_ev ());

  cspi_return_val_if_ev ("version", NULL);

  return retval;
}

/**
 * AccessibleApplication_getID:
 * @obj: a pointer to the #AccessibleApplication being queried.
 *
 * Get the unique ID assigned by the Registry to an
 *      #AccessibleApplication instance.
 * (Not Yet Implemented by the registry).
 *
 * Returns: a unique #long integer associated with the application
 *          by the Registry, or 0 if the application is not registered.
 **/
long
AccessibleApplication_getID (AccessibleApplication *obj)
{
  long retval;

  cspi_return_val_if_fail (obj != NULL, 0);

  retval = Accessibility_Application__get_id (CSPI_OBJREF (obj),
					      cspi_ev ());

  cspi_return_val_if_ev ("id", 0);

  return retval;
}

/**
 * AccessibleApplication_getLocale:
 * @obj: a pointer to the #AccessibleApplication being queried.
 * @lc_category: one of the POSIX LC_TYPE enumeration, for instance
 * LC_MESSAGES.
 *
 * Get a POSIX-compliant string describing the application's current
 * locale setting for a particular @lctype category.
 *
 * @Since: AT-SPI 1.4
 *
 * Returns: a POSIX-compliant locale string, e.g. "C", "pt_BR", "sr@latn", etc.
 **/
char *
AccessibleApplication_getLocale (AccessibleApplication *obj, int lc_category)
{
  gchar *retval;
  Accessibility_LOCALE_TYPE lctype;

  cspi_return_val_if_fail (obj != NULL, CORBA_string_dup (""));

  switch (lc_category) 
  {
  case LC_COLLATE:
    lctype = Accessibility_LOCALE_TYPE_COLLATE;
    break;
  case LC_CTYPE:
    lctype = Accessibility_LOCALE_TYPE_CTYPE;
    break;
  case LC_NUMERIC:
    lctype = Accessibility_LOCALE_TYPE_NUMERIC;
    break;
  case LC_MONETARY:
    lctype = Accessibility_LOCALE_TYPE_MONETARY;
    break;
  case LC_MESSAGES:
  default:
    lctype = Accessibility_LOCALE_TYPE_MESSAGES;
    break;
  }

  retval = Accessibility_Application_getLocale (CSPI_OBJREF (obj),
						lctype,
						cspi_ev ());

  cspi_return_val_if_ev ("id", CORBA_string_dup (""));

  return CORBA_string_dup (retval);
}

/**
 * AccessibleApplication_pause:
 * @obj: a pointer to the #Accessible object on which to operate.
 *
 * Attempt to pause the application (used when client event queue is
 *  over-full).
 * Not Yet Implemented.
 *
 * Returns: #TRUE if the application was paused successfully, #FALSE otherwise.
 *
 **/
SPIBoolean
AccessibleApplication_pause (AccessibleApplication *obj)
{
  return FALSE;
}

/**
 * AccessibleApplication_resume:
 * @obj: a pointer to the #Accessible object on which to operate.
 *
 * Attempt to resume the application (used after #AccessibleApplication_pause).
 * Not Yet Implemented.
 *
 * Returns: #TRUE if application processing resumed successfully, #FALSE otherwise.
 *
 **/
SPIBoolean
AccessibleApplication_resume (AccessibleApplication *obj)
{
  return FALSE;
}
