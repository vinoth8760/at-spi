/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2001 Sun Microsystems Inc.
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
 * application.c: implements Application.idl
 *
 */
#include <config.h>
#include <bonobo/Bonobo.h>
#include "atksimpleobject.h"

/*
 * This pulls the CORBA definitions for the "Accessibility::Accessible" server
 */
#include <libspi/Accessibility.h>

/*
 * This pulls the definition for the BonoboObject (GObject Type)
 */
#include "application.h"

/*
 * Our parent Gtk object type
 */
#define PARENT_TYPE ACCESSIBLE_TYPE

static void
application_class_init (ApplicationClass *klass)
{
  ;
}

static void
application_init (Application  *application)
{
  ACCESSIBLE (application)->atko = atk_simple_object_new();
}

GType
application_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static const GTypeInfo tinfo = {
                        sizeof (ApplicationClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) application_class_init,
                        (GClassFinalizeFunc) NULL,
                        NULL, /* class data */
                        sizeof (Application),
                        0, /* n preallocs */
                        (GInstanceInitFunc) application_init,
                        NULL /* value table */
                };
                /*
                 * Bonobo_type_unique auto-generates a load of
                 * CORBA structures for us. All derived types must
                 * use bonobo_type_unique.
                 */
                type = bonobo_type_unique (
                        PARENT_TYPE,
                        POA_Accessibility_Application__init,
                        NULL,
                        G_STRUCT_OFFSET (ApplicationClass, epv),
                        &tinfo,
                        "Application");
        }

        return type;
}

Application *
application_new (char *name, char *desc, char *id)
{
    Application *retval =
               APPLICATION (g_object_new (application_get_type (), NULL));
    atk_object_set_name (ACCESSIBLE (retval)->atko, CORBA_string_dup (name));
    atk_object_set_description (ACCESSIBLE (retval)->atko, CORBA_string_dup (desc));
    retval->id = CORBA_string_dup (id);
    return retval;
}
