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

#ifndef SPI_KEYSTROKE_LISTENER_H_
#define SPI_KEYSTROKE_LISTENER_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <bonobo/bonobo-object.h>
#include <atk/atkobject.h>
#include <libspi/Accessibility.h>
#include "keymasks.h"

#define SPI_KEYSTROKE_LISTENER_TYPE        (spi_keystroke_listener_get_type ())
#define SPI_KEYSTROKE_LISTENER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), SPI_KEYSTROKE_LISTENER_TYPE, SpiKeystrokeListener))
#define SPI_KEYSTROKE_LISTENER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), SPI_KEYSTROKE_LISTENER_TYPE, SpiKeystrokeListenerClass))
#define IS_SPI_KEYSTROKE_LISTENER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), SPI_KEYSTROKE_LISTENER_TYPE))
#define IS_SPI_KEYSTROKE_LISTENER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), SPI_KEYSTROKE_LISTENER_TYPE))

typedef gboolean (*BooleanKeystrokeListenerCB) (const void *keystroke_ptr);

typedef struct {
        BonoboObject parent;
	GList *callbacks;
} SpiKeystrokeListener;

typedef struct {
        BonoboObjectClass parent_class;
        POA_Accessibility_KeystrokeListener__epv epv;
} SpiKeystrokeListenerClass;

GType                  spi_keystroke_listener_get_type        (void);
SpiKeystrokeListener  *spi_keystroke_listener_new             (void);
void                   spi_keystroke_listener_add_callback    (SpiKeystrokeListener *listener,
							       BooleanKeystrokeListenerCB callback);
void                   spi_keystroke_listener_remove_callback (SpiKeystrokeListener *listener,
							       BooleanKeystrokeListenerCB callback);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* KEYSTROKE_SPI_LISTENER_H_ */