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

/* UGLY HACK for (unutterable_horror) */
#include <libspi/libspi.h>

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <cspi/spi.h>
#include <libbonobo.h>

static void validate_accessible (Accessible *accessible,
				 gboolean    has_parent,
				 gboolean    recurse_down);

#define WINDOW_MAGIC 0x123456a
#define TEST_STRING "A test string"

static gboolean print_tree = FALSE;

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
	GtkWidget  *widget, *vbox;

	win->magic  = WINDOW_MAGIC;
	win->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
 
	gtk_widget_show (win->window);

	vbox = gtk_vbox_new (0, 0);
	gtk_container_add (GTK_CONTAINER (win->window), vbox);
	gtk_widget_show (vbox);

	widget = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (widget), TEST_STRING);
	gtk_container_add (GTK_CONTAINER (vbox), widget);
	gtk_widget_show (widget);

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
test_roles (void)
{
	int i;

	fprintf (stderr, "Testing roles...\n");
	for (i = -1; i < 1000; i++)
		g_assert (AccessibleRole_getName (i) != NULL);

	g_assert (!strcmp (AccessibleRole_getName (SPI_ROLE_FILE_CHOOSER), "file chooser"));
	g_assert (!strcmp (AccessibleRole_getName (SPI_ROLE_RADIO_BUTTON), "radiobutton"));
	g_assert (!strcmp (AccessibleRole_getName (SPI_ROLE_TABLE), "table"));
	g_assert (!strcmp (AccessibleRole_getName (SPI_ROLE_WINDOW), "window"));
}

static void
test_desktop (void)
{
	Accessible *desktop;
	Accessible *application;

	fprintf (stderr, "Testing desktop...\n");

	g_assert (getDesktop (-1) == NULL);
	desktop = getDesktop (0);
	g_assert (desktop != NULL);

	validate_accessible (desktop, FALSE, FALSE);

	application = Accessible_getChildAtIndex (desktop, 0);
	g_assert (application == NULL); /* app registered idly */

	Accessible_unref (desktop);
}

static void
test_application (Accessible *application)
{
	char *str;

	g_assert (Accessible_isApplication (application));
	g_assert (Accessible_getApplication (application) ==
		  application);
	AccessibleApplication_unref (application);

	str = AccessibleApplication_getToolkitName (application);
	g_assert (str != NULL);
	g_assert (!strcmp (str, "GAIL"));
	SPI_freeString (str);

	str = AccessibleApplication_getVersion (application);
	g_assert (str != NULL);
	SPI_freeString (str);

	AccessibleApplication_getID (application);
}


static void
validate_tree (Accessible *accessible,
	       gboolean    has_parent,
	       gboolean    recurse_down)
{
	Accessible  *parent;
	long         len, i;

	parent = Accessible_getParent (accessible);
	if (has_parent) {
		long        index;
		Accessible *child_at_index;

		g_assert (parent != NULL);

		index = Accessible_getIndexInParent (accessible);
		g_assert (index >= 0);

		child_at_index = Accessible_getChildAtIndex (parent, index);

		g_assert (child_at_index == accessible);

		Accessible_unref (parent);
	}

	len = Accessible_getChildCount (accessible);
	for (i = 0; i < len; i++) {
		Accessible *child;

		child = Accessible_getChildAtIndex (accessible, i);
		g_assert (child != NULL);

		g_assert (Accessible_getIndexInParent (child) == i);
		g_assert (Accessible_getParent (child) == accessible);

		if (recurse_down)
			validate_accessible (child, has_parent, recurse_down);
	}
}

static void
validate_accessible (Accessible *accessible,
		     gboolean    has_parent,
		     gboolean    recurse_down)
{
	Accessible *tmp;
	char       *name, *descr;
	const char *role;

	name = Accessible_getName (accessible);
	g_assert (name != NULL);
	SPI_freeString (name);
  
	descr = Accessible_getDescription (accessible);
	g_assert (descr != NULL);
	SPI_freeString (descr);

	role = Accessible_getRole (accessible);
	g_assert (role != NULL);

	if (print_tree)
		fprintf (stderr, "accessible: %p '%s' (%s) - %s: [",
			 accessible, name, descr, role);

	if (Accessible_isAction (accessible)) {
		fprintf (stderr, "At");
		tmp = Accessible_getAction (accessible);
		g_assert (tmp != NULL);
		AccessibleAction_unref (accessible);
	}

	if (Accessible_isApplication (accessible)) {
		fprintf (stderr, "Ap");
		test_application (accessible);
	}

	if (Accessible_isComponent (accessible)) {
		fprintf (stderr, "Co");
		tmp = Accessible_getComponent (accessible);
		g_assert (tmp != NULL);
		AccessibleComponent_unref (accessible);
	}

	if (Accessible_isEditableText (accessible)) {
		fprintf (stderr, "Et");
		tmp = Accessible_getEditableText (accessible);
		g_assert (tmp != NULL);
		AccessibleEditableText_unref (accessible);
	}

	if (Accessible_isHypertext (accessible)) {
		fprintf (stderr, "Ht");
		tmp = Accessible_getHypertext (accessible);
		g_assert (tmp != NULL);
		AccessibleHypertext_unref (accessible);
	}

	if (Accessible_isImage (accessible)) {
		fprintf (stderr, "Im");
		tmp = Accessible_getImage (accessible);
		g_assert (tmp != NULL);
		AccessibleImage_unref (accessible);
	}

	if (Accessible_isSelection (accessible)) {
		fprintf (stderr, "Se");
		tmp = Accessible_getSelection (accessible);
		g_assert (tmp != NULL);
		AccessibleSelection_unref (accessible);
	}

	if (Accessible_isTable (accessible)) {
		fprintf (stderr, "Ta");
		tmp = Accessible_getTable (accessible);
		g_assert (tmp != NULL);
		AccessibleTable_unref (accessible);
	}

	if (Accessible_isText (accessible)) {
		fprintf (stderr, "Te");
		tmp = Accessible_getText (accessible);
		g_assert (tmp != NULL);
		AccessibleText_unref (accessible);
	}


	fprintf (stderr, "]\n");

	validate_tree (accessible, has_parent, recurse_down);
}

static void
unutterable_horror (void)
{
	/* Brutal ugliness to de-register ourself and exit cleanly */
	CORBA_Environment ev;
	Accessible        *desktop;
	Accessible        *app_access;
	Accessibility_Accessible app;
	Accessibility_Registry   registry;

	fprintf (stderr, "Unutterable horror ...\n");

	/* First get the application ! - gack */
	desktop = getDesktop (0);
	app_access = Accessible_getChildAtIndex (desktop, 0);

	/* Good grief */
	app = *(Accessibility_Accessible *) app_access;

	CORBA_exception_init (&ev);

	registry = bonobo_activation_activate_from_id (
		"OAFIID:Accessibility_Registry:proto0.1", 0, NULL, &ev);

	Accessibility_Registry_deregisterApplication (
		registry, app, &ev);

	bonobo_object_release_unref (registry, &ev); /* the original ref */
	bonobo_object_release_unref (registry, &ev); /* taken by the bridge */
	bonobo_object_release_unref (registry, &ev); /* taken by spi_main.c */

	Accessible_unref (desktop);
	Accessible_unref (app_access);

	/* Urgh ! - get a pointer to app */
	bonobo_object_unref (bonobo_object (ORBit_small_get_servant (app)));
}

static void
global_listener_cb (AccessibleEvent     *event,
		    void                *user_data)
{
	TestWindow *win = user_data;
	Accessible *desktop;
	AccessibleApplication *application;

	g_assert (win->magic == WINDOW_MAGIC);
	g_assert (!strcmp (event->type, "focus:"));

	fprintf (stderr, "Fielded focus event ...");

	/* The application is now registered - this happens idly */
	desktop = getDesktop (0);
	application = Accessible_getChildAtIndex (desktop, 0);
	g_assert (application != NULL);
	Accessible_unref (desktop);

	test_application (application);
	
	AccessibleApplication_unref (application);

	print_tree = TRUE;
	validate_accessible (event->source, TRUE, TRUE);
	print_tree = FALSE;

	gtk_main_quit ();
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

	test_roles ();
	test_desktop ();

	win = create_test_window ();

	global_listener = createAccessibleEventListener (global_listener_cb, win);
	g_assert (registerGlobalEventListener (global_listener, "focus:"));

	fprintf (stderr, "Waiting for focus event ...");
	gtk_main ();

	g_assert (deregisterGlobalEventListenerAll (global_listener));

	test_window_destroy (win);

/* FIXME: we need to resolve the exit / quit mainloop function */
/*	SPI_exit ();
	SPI_exit (); */
	unutterable_horror (); /* ! */

	fprintf (stderr, "All tests passed\n");

	return bonobo_debug_shutdown ();
}
