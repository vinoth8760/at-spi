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

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "spi.h"

static void report_focus_event (void *fp);
static void report_button_press (void *fp);
static void get_environment_vars (void);

static int _festival_init ();
static void _festival_say (const char *text, const char *voice, boolean shutup);
static void _festival_write (const char *buff, int fd);

static boolean use_magnifier = FALSE;
static boolean use_festival = FALSE;
static boolean festival_chatty = FALSE;

int
main(int argc, char **argv)
{
  int i, j;
  int n_desktops;
  int n_apps;
  Accessible *desktop;
  Accessible *application;
  AccessibleEventListener *focus_listener;
  AccessibleEventListener *button_listener;

  if ((argc > 1) && (!strncmp(argv[1],"-h",2)))
  {
    printf ("Usage: simple-at\n");
    printf ("\tEnvironment variables used:\n\t\tFESTIVAL\n\t\tMAGNIFIER\n\t\tFESTIVAL_CHATTY\n");
    exit(0);
  }

  SPI_init();

  focus_listener = createEventListener (report_focus_event);
  button_listener = createEventListener (report_button_press);

  registerGlobalEventListener (focus_listener, "focus:");
  registerGlobalEventListener (button_listener, "Gtk:GtkWidget:button-press-event");

  n_desktops = getDesktopCount ();

  for (i=0; i<n_desktops; ++i)
    {
      desktop = getDesktop (i);
      fprintf (stderr, "desktop %d name: %s\n", i, Accessible_getName (desktop));
      n_apps = Accessible_getChildCount (desktop);
      for (j=0; j<n_apps; ++j)
        {
          application = Accessible_getChildAtIndex (desktop, j);
          fprintf (stderr, "app %d name: %s\n", j, Accessible_getName (application));
        }
    }

  get_environment_vars();

  SPI_event_main(FALSE);
}

static void
get_environment_vars()
{
  if (getenv ("FESTIVAL"))
  {
    use_festival = TRUE;
    if (getenv ("FESTIVAL_CHATTY"))
    {
      festival_chatty = TRUE;
    }
  }
  if (getenv("MAGNIFIER"))
  {
    use_magnifier = TRUE;
  }  
}

void
report_focus_event (void *p)
{
  AccessibleEvent *ev = (AccessibleEvent *) p;
  fprintf (stderr, "%s event from %s\n", ev->type,
           Accessible_getName (&ev->source));
  
  if (use_festival)
    {
    if (festival_chatty) 	    
      {
        _festival_say (Accessible_getRole (&ev->source), "voice_don_diphone", TRUE);
      }
      _festival_say (Accessible_getName (&ev->source), "voice_kal_diphone", festival_chatty==FALSE);
    }
  
  if (Accessible_isComponent (&ev->source))
    {
      long x, y, width, height;
      AccessibleComponent *component = Accessible_getComponent (&ev->source);
      AccessibleComponent_getExtents (component, &x, &y, &width, &height,
                                      COORD_TYPE_SCREEN);
      fprintf (stderr, "Bounding box: (%ld, %ld) ; (%ld, %ld)\n",
               x, y, x+width, y+height);
      if (use_magnifier) {
	      magnifier_set_roi (x, y, width, height);	      
      }
    }
}

void
report_button_press (void *p)
{
  AccessibleEvent *ev = (AccessibleEvent *) p;
  fprintf (stderr, "%s event from %s\n", ev->type,
           Accessible_getName (&ev->source));
}

static int _festival_init ()
{
  int fd;
  struct sockaddr_in name;
  int tries = 2;

  name.sin_family = AF_INET;
  name.sin_port = htons (1314);
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  fd = socket (PF_INET, SOCK_STREAM, 0);

  while (connect(fd, (struct sockaddr *) &name, sizeof (name)) < 0) {
    if (!tries--) {
      perror ("connect");
      return -1;
    }
  }

  _festival_write ("(audio_mode'async)\n", fd);
  _festival_write ("(Parameter.set 'Duration_Model 'Tree_ZScore)\n", fd);
  _festival_write ("(Parameter.set 'Duration_Stretch 0.5)\n", fd);
  return fd;
}

static void _festival_say (const char *text, const char *voice, boolean shutup)
{
  static int fd = 0;
  gchar *quoted;
  gchar *p;
  gchar prefix[50];
  static gchar voice_spec[32];
  
  if (!fd)
    {
      fd = _festival_init ();
    }

  fprintf (stderr, "saying text: %s\n", text);
  
  quoted = g_malloc(64+strlen(text)*2);

  sprintf (prefix, "(SayText \"");

  strncpy(quoted, prefix, 10);
  p = quoted+strlen(prefix);
  while (*text) {
    if ( *text == '\\' || *text == '"' )
      *p = '\\';
    *p++ = *text++;
  }
  *p++ = '"';
  *p++ = ')';
  *p++ = '\n';
  *p = 0;

  if (shutup) _festival_write ("(audio_mode'shutup)\n", fd);
  if (voice && (strncmp (voice, (char *) (voice_spec+1), strlen(voice))))
    {
      snprintf (voice_spec, 32, "(%s)\n", voice); 
      _festival_write (voice_spec, fd);
      _festival_write ("(Parameter.set 'Duration_Model 'Tree_ZScore)\n", fd);
      _festival_write ("(Parameter.set 'Duration_Stretch 0.5)\n", fd);
    }

  _festival_write (quoted, fd);

  g_free(quoted);
}

static void _festival_write (const gchar *command_string, int fd)
{
  fprintf(stderr, command_string);
  if (fd < 0) {
    perror("socket");
    return;
  }
  write(fd, command_string, strlen(command_string));
}

