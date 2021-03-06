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

#ifndef SPI_PRIVATE_H_
#define SPI_PRIVATE_H_

#include <glib.h>
#include <atk/atk.h>
#include <orbit/orbit.h>
#include <Accessibility.h>

G_BEGIN_DECLS

#define DBG(a,b) if(_dbg>=(a))b

extern int _dbg;

typedef enum {
	SPI_RE_ENTRANT_CONTINUE = 0,
	SPI_RE_ENTRANT_TERMINATE
} SpiReEntrantContinue;

typedef SpiReEntrantContinue (*SpiReEntrantFn) (GList * const *list,
						gpointer       user_data);

Accessibility_Role spi_role_from_atk_role (AtkRole role);
void spi_re_entrant_list_delete_link (GList * const  *element_ptr);
void spi_re_entrant_list_foreach     (GList         **list,
				      SpiReEntrantFn  func,
				      gpointer        user_data);
void spi_init_any_nil                (CORBA_any *any_details, 
				      Accessibility_Application app,
				      Accessibility_Role role,
				      CORBA_string name);
void spi_init_any_string             (CORBA_any *any, 
				      Accessibility_Application app,
				      Accessibility_Role role,
				      CORBA_string name,
				      char **string);
void spi_init_any_object             (CORBA_any *any, 
				      Accessibility_Application app,
				      Accessibility_Role role,
				      CORBA_string name,
				      CORBA_Object *o);
void spi_init_any_rect               (CORBA_any *any, 
				      Accessibility_Application app,
				      Accessibility_Role role,
				      CORBA_string name,
				      AtkRectangle *rect);

G_END_DECLS

#endif /* SPI_PRIVATE_H_ */
