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

#ifndef _SPI_PRIVATE_H_
#define _SPI_PRIVATE_H_

/* Private internal implementation details of at-spi. */

#include <libspi/Accessibility.h>
#include <cspi/spi.h>
#include "cspi/cspi-lowlevel.h"
#include "cspi/spi-listener.h"

struct _Accessible {
	CORBA_Object objref;
	/* And some other bits */
	guint        ref_count;
};

#define CSPI_OBJREF(a) (((Accessible *)(a))->objref)

CORBA_Environment     *cspi_ev               (void);
SPIBoolean             cspi_exception        (void);
Accessibility_Registry cspi_registry         (void);
Accessible            *cspi_object_add       (CORBA_Object         corba_object);
void                   cspi_object_ref       (Accessible          *accessible);
void                   cspi_object_unref     (Accessible          *accessible);
SPIBoolean             cspi_accessible_is_a  (Accessible          *obj,
					      const char          *interface_name);

#define cspi_return_if_fail(val)		\
	if (!(val))				\
		return
#define cspi_return_val_if_fail(val, ret)	\
	if (!(val))				\
		return (ret)

#define cspi_return_if_ev(err)			\
	if (!cspi_check_ev (err))		\
		return;
#define cspi_return_val_if_ev(err, ret)	\
	if (!cspi_check_ev (err))		\
		return (ret);

#endif /* _SPI_PRIVATE_H_ */
