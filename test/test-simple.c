/*
 * test-simple.c: A set of simple regression tests
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2001 Ximian, Inc.
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
 * ******** Do not copy this code as an example *********
 */

#include <stdio.h>
#include <gtk/gtk.h>
#include <cspi/spi.h>

#define WINDOW_MAGIC 0x123456a
typedef struct {
  gulong     magic;
  GtkWidget *window;
} TestWindow;

static gboolean
focus_me (GtkWidget *widget)
{
  gtk_widget_grab_focus (widget);
  return FALSE;
}

static TestWindow *
create_test_window (void)
{
  TestWindow *win = g_new0 (TestWindow, 1);

  win->magic  = WINDOW_MAGIC;
  win->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
 
  gtk_widget_show (win->window);

  g_idle_add ((GSourceFunc) focus_me, win->window);

  return win;
}

static void
test_window_destroy (TestWindow *win)
{
  gtk_widget_destroy (win->window);
  g_free (win);
}

static void
global_listener_cb (AccessibleEvent     *event,
		    void                *user_data)
{
  TestWindow *win = user_data;
  static int  events = 0;

  g_assert (win->magic == WINDOW_MAGIC);

  g_warning ("Event %s", event->type);

  if (events++ > 100)
    {
      SPI_exit ();
    }
}

int
main (int argc, char **argv)
{
  TestWindow *win;
  AccessibleEventListener *global_listener;

  gtk_init (&argc, &argv);

  g_assert (!SPI_init ());
  g_assert (SPI_init ());
  g_assert (getDesktopCount () == 1);

  win = create_test_window ();

  global_listener = createAccessibleEventListener (global_listener_cb, win);
  registerGlobalEventListener (global_listener, "focus:");

  SPI_event_main (FALSE /* FIXME: remove this arg. */);

  deregisterGlobalEventListenerAll (global_listener);

  SPI_exit ();
  SPI_exit ();

  test_window_destroy (win);

  fprintf (stderr, "All tests passed\n");

  return 0;
}
