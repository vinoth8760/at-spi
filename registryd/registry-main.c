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

#ifdef AT_SPI_DEBUG
#include <stdlib.h>
#endif

#include <config.h>
#include <string.h>
#include <gdk/gdkx.h>
#include <libbonobo.h>
#include <glib.h>
#include "registry.h"
#include <dbus/dbus-glib.h>

#ifdef HAVE_SM
#include <X11/SM/SMlib.h>
#include <X11/ICE/ICElib.h>
#include <fcntl.h>
#endif

#define spi_get_display() GDK_DISPLAY()

static void registry_set_ior (SpiRegistry *registry);
static void registry_session_init (const char *previous_client_id, const char *exe);
static void set_gtk_modules (DBusGProxy *gsm);
#ifdef HAVE_SM
static void die_callback (SmcConn smc_conn, SmPointer client_data);
static void save_yourself_callback      (SmcConn   smc_conn,
                                         SmPointer client_data,
                                         int       save_style,
                                         Bool      shutdown,
                                         int       interact_style,
                                         Bool      fast);

static SmcConn session_connection;
#endif

int
main (int argc, char **argv)
{
  int          ret;
  char        *obj_id;
  const char  *display_name;
  char        *cp, *dp;
  SpiRegistry *registry;

  DBusGConnection *connection;
  DBusGProxy      *gsm;
  GError          *error;

  if (!bonobo_init (&argc, argv))
    {
      g_error ("Could not initialize oaf / Bonobo");
    }

  obj_id = "OAFIID:Accessibility_Registry:1.0";

  registry = spi_registry_new ();

  display_name = g_getenv ("AT_SPI_DISPLAY");
  if (!display_name)
  {
      display_name = g_getenv ("DISPLAY");
      cp = strrchr (display_name, '.');
      dp = strrchr (display_name, ':');
      if (cp && dp && (cp > dp)) *cp = '\0';
  }
  ret = bonobo_activation_register_active_server (
	  obj_id,
	  bonobo_object_corba_objref (bonobo_object (registry)),
	  NULL);

  if (ret != Bonobo_ACTIVATION_REG_SUCCESS)
    {
#ifdef AT_SPI_DEBUG
      fprintf (stderr, "SpiRegistry Message: SpiRegistry daemon was already running.\n");
#endif
    }
  else
    {
#ifdef AT_SPI_DEBUG
      fprintf (stderr, "SpiRegistry Message: SpiRegistry daemon is running.\n");
#endif
      error = NULL;
      connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
      if (connection == NULL)
        {
          g_error ("couldn't get D-Bus connection: %s", error->message);
        }
      gsm = dbus_g_proxy_new_for_name (connection,
                                       "org.gnome.SessionManager",
                                       "/org/gnome/SessionManager",
                                       "org.gnome.SessionManager");
      set_gtk_modules (gsm);

      registry_set_ior (registry);

      /* If DESKTOP_AUTOSTART_ID exists, assume we're started by session
       * manager and connect to it. */
      const char *desktop_autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
      if (desktop_autostart_id != NULL) {
        char *client_id = g_strdup (desktop_autostart_id);
        /* Unset DESKTOP_AUTOSTART_ID in order to avoid child processes to
         * use the same client id. */
        g_unsetenv ("DESKTOP_AUTOSTART_ID");
        registry_session_init (client_id, argv[0]);
        g_free (client_id);
      }

      bonobo_main ();
    }

  return 0;
}

static void
registry_set_ior (SpiRegistry *registry){
  CORBA_Environment ev;
  Atom  AT_SPI_IOR = XInternAtom (spi_get_display (), "AT_SPI_IOR", FALSE);
  char *iorstring = NULL;

  CORBA_exception_init (&ev);

  iorstring = CORBA_ORB_object_to_string (bonobo_activation_orb_get (),
                                     bonobo_object_corba_objref (bonobo_object (registry)),
                                     &ev);

  XChangeProperty (spi_get_display(),
		   XDefaultRootWindow (spi_get_display ()),
		   AT_SPI_IOR, (Atom) 31, 8,
		   PropModeReplace,
		   (unsigned char *) iorstring,
		   iorstring ? strlen (iorstring) : 0);

  if (ev._major != CORBA_NO_EXCEPTION)
	  {
		  g_error ("Error setting IOR %s",
			   CORBA_exception_id (&ev));
                  CORBA_exception_free (&ev);
           }

  CORBA_exception_free (&ev);

}

#ifdef HAVE_SM
/* This is called when data is available on an ICE connection.  */
static gboolean
process_ice_messages (GIOChannel *channel,
                      GIOCondition condition,
                      gpointer client_data)
{
  IceConn connection = (IceConn) client_data;
  IceProcessMessagesStatus status;

  /* This blocks infinitely sometimes. I don't know what
   * to do about it. Checking "condition" just breaks
   * session management.
   */
  status = IceProcessMessages (connection, NULL, NULL);

  if (status == IceProcessMessagesIOError)
    {
#if 0
      IcePointer context = IceGetConnectionContext (connection);
#endif

      /* We were disconnected */
      IceSetShutdownNegotiation (connection, False);
      IceCloseConnection (connection);

      return FALSE;
    }

  return TRUE;
}

/* This is called when a new ICE connection is made.  It arranges for
   the ICE connection to be handled via the event loop.  */
static void
new_ice_connection (IceConn connection, IcePointer client_data, Bool opening,
                    IcePointer *watch_data)
{
  guint input_id;

  if (opening)
    {
      /* Make sure we don't pass on these file descriptors to any
       * exec'ed children
       */
      GIOChannel *channel;

      fcntl (IceConnectionNumber (connection), F_SETFD,
             fcntl (IceConnectionNumber (connection), F_GETFD, 0) | FD_CLOEXEC);

      channel = g_io_channel_unix_new (IceConnectionNumber (connection));

      input_id = g_io_add_watch (channel,
                                 G_IO_IN | G_IO_ERR,
                                 process_ice_messages,
                                 connection);

      g_io_channel_unref (channel);

      *watch_data = (IcePointer) GUINT_TO_POINTER (input_id);
    }
  else
    {
      input_id = GPOINTER_TO_UINT ((gpointer) *watch_data);

      g_source_remove (input_id);
    }
}

static IceIOErrorHandler ice_installed_handler;

/* We call any handler installed before (or after) gnome_ice_init but 
   avoid calling the default libICE handler which does an exit() */
static void
ice_io_error_handler (IceConn connection)
{
    if (ice_installed_handler)
      (*ice_installed_handler) (connection);
}

static void
ice_init (void)
{
  static gboolean ice_initted = FALSE;

  if (! ice_initted)
    {
      IceIOErrorHandler default_handler;

      ice_installed_handler = IceSetIOErrorHandler (NULL);
      default_handler = IceSetIOErrorHandler (ice_io_error_handler);

      if (ice_installed_handler == default_handler)
        ice_installed_handler = NULL;

      IceAddConnectionWatch (new_ice_connection, NULL);

      ice_initted = TRUE;
    }
}
#endif

void
registry_session_init (const char *previous_client_id, const char *exe)
{
#ifdef HAVE_SM
  char buf[256];
  char *client_id;
  unsigned long mask;
  SmcCallbacks callbacks;

  ice_init();

  callbacks.save_yourself.callback = save_yourself_callback;
  callbacks.save_yourself.client_data = NULL;
  callbacks.die.callback = die_callback;
  callbacks.die.client_data = NULL;

  mask = SmcSaveYourselfProcMask | SmcDieProcMask;

  session_connection =
    SmcOpenConnection (NULL, /* use SESSION_MANAGER env */
                       NULL, /* means use existing ICE connection */
                       SmProtoMajor,
                       SmProtoMinor,
                       mask,
                       &callbacks,
                       (char*) previous_client_id,
                       &client_id,
                       255, buf);

  if (session_connection != NULL) {
    SmProp prop1, prop2, prop3, prop4, prop5, prop6, *props[6];
    SmPropValue prop1val, prop2val, prop3val, prop4val, prop5val, prop6val;
    char pid[32];
    char hint = SmRestartImmediately;
    char priority = 1; /* low to run before other apps */

    prop1.name = SmProgram;
    prop1.type = SmARRAY8;
    prop1.num_vals = 1;
    prop1.vals = &prop1val;
    prop1val.value = exe;
    prop1val.length = strlen (exe);

    /* twm sets getuid() for this, but the SM spec plainly
     * says pw_name, twm is on crack
     */
    prop2.name = SmUserID;
    prop2.type = SmARRAY8;
    prop2.num_vals = 1;
    prop2.vals = &prop2val;
    prop2val.value = (char*) g_get_user_name ();
    prop2val.length = strlen (prop2val.value);

    prop3.name = SmRestartStyleHint;
    prop3.type = SmCARD8;
    prop3.num_vals = 1;
    prop3.vals = &prop3val;
    prop3val.value = &hint;
    prop3val.length = 1;

    sprintf (pid, "%d", getpid ());
    prop4.name = SmProcessID;
    prop4.type = SmARRAY8;
    prop4.num_vals = 1;
    prop4.vals = &prop4val;
    prop4val.value = pid;
    prop4val.length = strlen (prop4val.value);

    /* Always start in home directory */
    prop5.name = SmCurrentDirectory;
    prop5.type = SmARRAY8;
    prop5.num_vals = 1;
    prop5.vals = &prop5val;
    prop5val.value = (char*) g_get_home_dir ();
    prop5val.length = strlen (prop5val.value);

    prop6.name = "_GSM_Priority";
    prop6.type = SmCARD8;
    prop6.num_vals = 1;
    prop6.vals = &prop6val;
    prop6val.value = &priority;
    prop6val.length = 1;

    props[0] = &prop1;
    props[1] = &prop2;
    props[2] = &prop3;
    props[3] = &prop4;
    props[4] = &prop5;
    props[5] = &prop6;

    SmcSetProperties (session_connection, 6, props);
  }

#endif
}

#ifdef HAVE_SM
static void
die_callback (SmcConn smc_conn, SmPointer client_data)
{
  SmcCloseConnection (session_connection, 0, NULL);
  bonobo_main_quit ();
}

static void
save_yourself_callback      (SmcConn   smc_conn,
                             SmPointer client_data,
                             int       save_style,
                             Bool      shutdown,
                             int       interact_style,
                             Bool      fast)
{
  SmcSaveYourselfDone (session_connection, TRUE);
}
#endif

static void
set_gtk_modules (DBusGProxy *gsm)
{
        const char *old;
        char       *value;
        gboolean    found_gail;
        gboolean    found_atk_bridge;
        GError     *error;
        int         i;

        found_gail = FALSE;
        found_atk_bridge = FALSE;

        old = g_getenv ("GTK_MODULES");
        if (old != NULL) {
                char **old_modules;
                char **modules;

                old_modules = g_strsplit (old, ":", -1);
                for (i = 0; old_modules[i]; i++) {
                        if (!strcmp (old_modules[i], "gail")) {
                                found_gail = TRUE;
                        } else if (!strcmp (old_modules[i], "atk-bridge")) {
                                found_atk_bridge = TRUE;
                        }
                }

                modules = g_new (char *, i + (found_gail ? 0 : 1) +
                                 (found_atk_bridge ? 0 : 1) + 1);
                for (i = 0; old_modules[i]; i++) {
                        modules[i] = old_modules[i];
                }
                if (!found_gail) {
                                modules[i++] = "gail";
                }
                if (!found_atk_bridge) {
                        modules[i++] = "atk-bridge";
                }
                modules[i] = NULL;

                value = g_strjoinv (":", modules);
                g_free (modules);
                g_strfreev (old_modules);
        } else {
                value = g_strdup ("gail:atk-bridge");
        }

        error = NULL;
        if (!dbus_g_proxy_call (gsm, "Setenv", &error,
                                G_TYPE_STRING, "GTK_MODULES",
                                G_TYPE_STRING, value,
                                G_TYPE_INVALID,
                                G_TYPE_INVALID)) {
                g_warning ("Could not set GTK_MODULES: %s", error->message);
                g_error_free (error);
        }

        g_free (value);
        return;
}
