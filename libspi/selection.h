/* ATK -  Accessibility Toolkit
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

#ifndef SPI_SELECTION_H_
#define SPI_SELECTION_H_

#include <bonobo/bonobo-object.h>
#include <atk/atk.h>
#include <libspi/Accessibility.h>

G_BEGIN_DECLS

#define SPI_SELECTION_TYPE        (spi_selection_get_type ())
#define SPI_SELECTION(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), SPI_SELECTION_TYPE, SpiSelection))
#define SPI_SELECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SPI_SELECTION_TYPE, SpiSelectionClass))
#define SPI_IS_SELECTION(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SPI_SELECTION_TYPE))
#define SPI_IS_SELECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SPI_SELECTION_TYPE))

typedef struct _Selection SpiSelection;
typedef struct _SelectionClass SpiSelectionClass;

struct _Selection {
  BonoboObject parent;
  AtkObject *atko;
};

struct _SelectionClass {
  BonoboObjectClass parent_class;
  POA_Accessibility_Selection__epv epv;
};

GType
spi_selection_get_type   (void);

SpiSelection *
spi_selection_interface_new       (AtkObject *obj);

G_END_DECLS

#endif /* SPI_SELECTION_H_ */
