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

#ifndef SPI_REGISTRY_H_
#define SPI_REGISTRY_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <glib/gmain.h>
#include <libspi/Accessibility.h>
#include "listener.h"
#include "desktop.h"
#include "deviceeventcontroller.h"

#define SPI_REGISTRY_TYPE        (spi_registry_get_type ())
#define SPI_REGISTRY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), SPI_REGISTRY_TYPE, SpiRegistry))
#define SPI_REGISTRY_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), SPI_REGISTRY_TYPE, SpiRegistryClass))
#define IS_SPI_REGISTRY(o)       (G_TYPE_CHECK__INSTANCE_TYPE ((o), SPI_REGISTRY_TYPE))
#define IS_SPI_REGISTRY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), SPI_REGISTRY_TYPE))

typedef struct {
  SpiListener parent;
  GList *object_listeners;
  GList *window_listeners;
  GList *toolkit_listeners;
  GList *applications;
  SpiDeviceEventController *device_event_controller;
  SpiDesktop *desktop;
  gboolean (*kbd_event_hook) (gpointer source);
} SpiRegistry;

typedef struct {
        SpiListenerClass parent_class;
        POA_Accessibility_SpiRegistry__epv epv;
} SpiRegistryClass;

GType               spi_registry_get_type   (void);
SpiRegistry            *spi_registry_new       (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SPI_REGISTRY_H_ */
