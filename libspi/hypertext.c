/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2001 Sun Microsystems Inc, Ximian Inc.
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

/* hypertext.c : implements the HyperText interface */

#include <config.h>
#include <stdio.h>
#include <libspi/hyperlink.h>
#include <libspi/hypertext.h>

SpiHypertext *
spi_hypertext_interface_new (AtkObject *obj)
{
  SpiHypertext *new_hypertext = g_object_new (SPI_HYPERTEXT_TYPE, NULL);

  spi_text_construct (SPI_TEXT (new_hypertext), obj);

  return new_hypertext;
}


static AtkHypertext *
get_hypertext_from_servant (PortableServer_Servant servant)
{
  SpiBase *object = SPI_BASE (bonobo_object_from_servant (servant));

  if (!object)
    {
      return NULL;
    }

  return ATK_HYPERTEXT (object->atko);
}


static CORBA_long
impl_getNLinks (PortableServer_Servant servant,
		CORBA_Environment     *ev)
{
  AtkHypertext *hypertext = get_hypertext_from_servant (servant);

  g_return_val_if_fail (hypertext != NULL, 0);

  return (CORBA_long) atk_hypertext_get_n_links (hypertext);
}


static Accessibility_Hyperlink
impl_getLink (PortableServer_Servant servant,
	      const CORBA_long       linkIndex,
	      CORBA_Environment     *ev)
{
  AtkHyperlink *link;
  Accessibility_Hyperlink rv;
  AtkHypertext *hypertext = get_hypertext_from_servant (servant);

  g_return_val_if_fail (hypertext != NULL, CORBA_OBJECT_NIL);
  
  link = atk_hypertext_get_link (hypertext, linkIndex);
  g_return_val_if_fail (link != NULL, CORBA_OBJECT_NIL);

  rv = BONOBO_OBJREF (spi_hyperlink_new (ATK_OBJECT (link)));

  return CORBA_Object_duplicate (rv, ev);
}


static CORBA_long
impl_getLinkIndex (PortableServer_Servant servant,
		   const CORBA_long       characterIndex,
		   CORBA_Environment     *ev)
{
  AtkHypertext *hypertext = get_hypertext_from_servant (servant);

  g_return_val_if_fail (hypertext != NULL, 0);

  return (CORBA_long)
    atk_hypertext_get_link_index (hypertext,
				  (gint) characterIndex);
}


static void
spi_hypertext_class_init (SpiHypertextClass *klass)
{
  POA_Accessibility_Hypertext__epv *epv = &klass->epv;

  /* Initialize epv table */

  epv->getNLinks = impl_getNLinks;
  epv->getLink = impl_getLink;
  epv->getLinkIndex = impl_getLinkIndex;
}


static void
spi_hypertext_init (SpiHypertext *hypertext)
{
}


BONOBO_TYPE_FUNC_FULL (SpiHypertext,
		       Accessibility_Hypertext,
		       BONOBO_TYPE_OBJECT,
		       spi_hypertext);
