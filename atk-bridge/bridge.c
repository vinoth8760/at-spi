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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libbonobo.h>
#include <orbit/orbit.h>
#include <atk/atk.h>
#include <atk/atkobject.h>
#include <atk/atknoopobject.h>
#include <libspi/Accessibility.h>
#include "accessible.h"
#include "application.h"

#undef SPI_BRIDGE_DEBUG

static CORBA_Environment ev;
static Accessibility_Registry registry;
static SpiApplication *this_app = NULL;

static gboolean bridge_idle_init (gpointer user_data);
static void bridge_focus_tracker (AtkObject *object);
static void bridge_exit_func (void);
static void register_atk_event_listeners (void);
static gboolean bridge_property_event_listener (GSignalInvocationHint *signal_hint,
						guint n_param_values,
						const GValue *param_values,
						gpointer data);
static gboolean bridge_state_event_listener (GSignalInvocationHint *signal_hint,
					     guint n_param_values,
					     const GValue *param_values,
					     gpointer data);
static gboolean bridge_signal_listener (GSignalInvocationHint *signal_hint,
					guint n_param_values,
					const GValue *param_values,
					gpointer data);

static gint bridge_key_listener (AtkKeyEventStruct *event,
				 gpointer data);

int
gtk_module_init (gint *argc, gchar **argv[])
{
  CORBA_Environment ev;

  if (!bonobo_init (argc, *argv))
    {
      g_error ("Could not initialize Bonobo");
    }

  CORBA_exception_init(&ev);

  registry = bonobo_activation_activate_from_id (
	  "OAFIID:Accessibility_Registry:proto0.1", 0, NULL, &ev);
  
  if (ev._major != CORBA_NO_EXCEPTION)
    {
      g_error ("Accessibility app error: exception during "
	       "registry activation from id: %s\n",
	       CORBA_exception_id (&ev));
      CORBA_exception_free (&ev);
    }

  if (CORBA_Object_is_nil (registry, &ev))
    {
      g_error ("Could not locate registry");
    }

  fprintf (stderr, "About to register application\n");

  bonobo_activate ();

  /* Create the accessible application server object */

  this_app = spi_application_new (atk_get_root ());

  fprintf (stderr, "About to register application\n");

  Accessibility_Registry_registerApplication (registry,
                                              BONOBO_OBJREF (this_app),
                                              &ev);

  g_atexit (bridge_exit_func);

  g_idle_add (bridge_idle_init, NULL);

  return 0;
}

static gboolean
bridge_idle_init (gpointer user_data)
{
  register_atk_event_listeners ();

  fprintf (stderr, "Application registered & listening\n");

  return FALSE;
}

static void
register_atk_event_listeners (void)
{
  /*
   * kludge to make sure the Atk interface types are registered, otherwise
   * the AtkText signal handlers below won't get registered
   */

  AtkObject *o = atk_no_op_object_new (g_object_new (ATK_TYPE_OBJECT, NULL));
  
  /* Register for focus event notifications, and register app with central registry  */

  atk_add_focus_tracker (bridge_focus_tracker);
  atk_add_global_event_listener (bridge_property_event_listener, "Gtk:AtkObject:property-change");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkObject:children-changed");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkObject:visible-data-changed");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkSelection:selection-changed");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkText:text-selection-changed");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkText:text-changed");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkText:text-caret-moved");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkTable:row-inserted");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkTable:row-reordered");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkTable:row-deleted");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkTable:column-inserted");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkTable:column-reordered");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkTable:column-deleted");
  atk_add_global_event_listener (bridge_signal_listener, "Gtk:AtkTable:model-changed");
  atk_add_key_event_listener    (bridge_key_listener, NULL);

  g_object_unref (o);
}

static void
bridge_exit_func (void)
{
  fprintf (stderr, "exiting bridge\n");

/*
  FIXME: this may be incorrect for apps that do their own bonobo shutdown,
  until we can explicitly shutdown to get the ordering right. */

  if (!bonobo_is_initialized ())
    {
      g_warning ("Re-initializing bonobo\n");
      g_assert (bonobo_init (0, NULL));
      g_assert (bonobo_activate ());
      registry = bonobo_activation_activate_from_id (
	  "OAFIID:Accessibility_Registry:proto0.1", 0, NULL, &ev);
      g_assert (registry);
      g_assert (this_app);
    }
  
  Accessibility_Registry_deregisterApplication (registry,
						BONOBO_OBJREF (this_app),
						&ev);
  
  bonobo_object_release_unref (BONOBO_OBJREF (this_app), &ev);
  
  fprintf (stderr, "bridge exit func complete.\n");

  bonobo_debug_shutdown ();
}

static void
bridge_focus_tracker (AtkObject *object)
{
  SpiAccessible *source;
  Accessibility_Event e;

  source = spi_accessible_new (object);

  e.type = "focus:";
  e.source = CORBA_Object_duplicate (BONOBO_OBJREF (source), &ev);
  e.detail1 = 0;
  e.detail2 = 0;

  Accessibility_Registry_notifyEvent (registry, &e, &ev);
}

static void
emit_eventv (GObject      *gobject,
	     unsigned long detail1,
	     unsigned long detail2,
	     const char   *format, ...)
{
  va_list             args;
  Accessibility_Event e;
  SpiAccessible      *source;
  AtkObject          *aobject;

  va_start (args, format);
  
  if (ATK_IS_IMPLEMENTOR (gobject))
    {
      aobject = atk_implementor_ref_accessible (ATK_IMPLEMENTOR (gobject));
      source  = spi_accessible_new (aobject);
      g_object_unref (G_OBJECT (aobject));
    }
  else if (ATK_IS_OBJECT (gobject))
    {
      aobject = ATK_OBJECT (gobject);
      source  = spi_accessible_new (aobject);
    }
  else
    {
      aobject = NULL;
      source  = NULL;
      g_error ("received property-change event from non-AtkImplementor");
    }

  if (source != NULL)
    {
      e.type = g_strdup_vprintf (format, args);
      e.source = CORBA_Object_duplicate (BONOBO_OBJREF (source), &ev);
      e.detail1 = detail1;
      e.detail2 = detail2;

#ifdef SPI_BRIDGE_DEBUG
      g_warning ("Emitting event '%s' (%d, %d) on %p",
		 e.type, e.detail1, e.detail2, source);
#endif

      Accessibility_Registry_notifyEvent (registry, &e, &ev);

      g_free (e.type);
    }

  va_end (args);
}

static gboolean
bridge_property_event_listener (GSignalInvocationHint *signal_hint,
				guint n_param_values,
				const GValue *param_values,
				gpointer data)
{
  AtkPropertyValues *values;
  GObject *gobject;

#ifdef SPI_BRIDGE_DEBUG
  GSignalQuery signal_query;
  const gchar *name;
  
  g_signal_query (signal_hint->signal_id, &signal_query);
  name = signal_query.signal_name;

  fprintf (stderr, "Received (property) signal %s:%s\n",
	   g_type_name (signal_query.itype), name);
#endif

  gobject = g_value_get_object (param_values + 0);
  values = (AtkPropertyValues*) g_value_get_pointer (param_values + 1);

  emit_eventv (gobject, 0, 0, "object:property-change:%s", values->property_name);

  return TRUE;
}

static gboolean
bridge_state_event_listener (GSignalInvocationHint *signal_hint,
			     guint n_param_values,
			     const GValue *param_values,
			     gpointer data)
{
  GObject *gobject;
  AtkPropertyValues *values;
#ifdef SPI_BRIDGE_DEBUG
  GSignalQuery signal_query;
  const gchar *name;
  
  g_signal_query (signal_hint->signal_id, &signal_query);
  name = signal_query.signal_name;
  fprintf (stderr, "Received (state) signal %s:%s\n",
	   g_type_name (signal_query.itype), name);
#endif

  gobject = g_value_get_object (param_values + 0);
  values = (AtkPropertyValues*) g_value_get_pointer (param_values + 1);

  emit_eventv (gobject, 
	       (unsigned long) values->old_value.data[0].v_ulong,
	       (unsigned long) values->new_value.data[0].v_ulong,
	       "object:%s:?", values->property_name);

  return TRUE;
}

static void
accessibility_init_keystroke_from_atk_key_event (Accessibility_KeyStroke *keystroke,
						 AtkKeyEventStruct       *event)
{
#ifdef SPI_DEBUG
  if (event)
    {
      g_print ("event %c (%d)\n", (int) event->keyval, (int) event->keycode);
    }
  else
#endif
  if (!event)
    {
      g_print ("WARNING: NULL key event!");
    }
  
  keystroke->keyID     = (CORBA_long) event->keyval;
  keystroke->keycode   = (CORBA_short) event->keycode;
  keystroke->timestamp = (CORBA_unsigned_long) event->timestamp;
  keystroke->modifiers = (CORBA_unsigned_short) (event->state & 0xFFFF);

  switch (event->type)
    {
    case (ATK_KEY_EVENT_PRESS):
      keystroke->type = Accessibility_KEY_PRESSED;
      break;
    case (ATK_KEY_EVENT_RELEASE):
      keystroke->type = Accessibility_KEY_RELEASED;
      break;
    default:
      keystroke->type = 0;
      break;
    }
}

static gint
bridge_key_listener (AtkKeyEventStruct *event, gpointer data)
{
  Accessibility_KeyStroke key_event;
  CORBA_boolean result;
  Accessibility_DeviceEventController controller =
	  Accessibility_Registry_getDeviceEventController (registry, &ev);

  accessibility_init_keystroke_from_atk_key_event (&key_event, event);

  /* FIXME: this casting is just totaly bogus */
  result = Accessibility_DeviceEventController_notifyListenersSync (controller,
								    (Accessibility_DeviceEvent *) &key_event,
								    &ev);
  return result; /* FIXME: is this correct ? */
}

static gboolean
bridge_signal_listener (GSignalInvocationHint *signal_hint,
			guint n_param_values,
			const GValue *param_values,
			gpointer data)
{
  GObject *gobject;
  GSignalQuery signal_query;
  const gchar *name;
  
  g_signal_query (signal_hint->signal_id, &signal_query);

  name = signal_query.signal_name;

#ifdef SPI_BRIDGE_DEBUG
  fprintf (stderr, "Received signal %s:%s\n",
	   g_type_name (signal_query.itype), name);
#endif

  gobject = g_value_get_object (param_values + 0);

  emit_eventv (gobject, 0, 0, "%s:%s", name, g_type_name (signal_query.itype));

  return TRUE;
}
