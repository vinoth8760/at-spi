/* AT-SPI - Assistive Technology Service Provider Interface
 *
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2001, 2003 Sun Microsystems Inc.,
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

/* deviceeventcontroller.c: implement the DeviceEventController interface */

#include <config.h>

#undef  SPI_XKB_DEBUG
#undef  SPI_DEBUG
#undef  SPI_KEYEVENT_DEBUG

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/time.h>
#include <bonobo/bonobo-exception.h>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/XKBlib.h>
#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>

#ifdef HAVE_XEVIE
#include <X11/Xproto.h>
#include <X11/X.h>
#include <X11/extensions/Xevie.h>
#endif /* HAVE_XEVIE */

#include <gdk/gdk.h>
#include <gdk/gdkx.h> /* TODO: hide dependency (wrap in single porting file) */
#include <gdk/gdkkeysyms.h>

#include "../libspi/spi-private.h"
#include "deviceeventcontroller.h"

KeySym ucs2keysym (long ucs);
long keysym2ucs(KeySym keysym); 

#define CHECK_RELEASE_DELAY 20
#define BIT(c, x)       (c[x/8]&(1<<(x%8)))
static guint check_release_handler = 0;
static Accessibility_DeviceEvent pressed_event;
static SpiDEController *saved_controller; 
static void wait_for_release_event (XEvent *event, SpiDEController *controller);

/* Our parent Gtk object type */
#define PARENT_TYPE BONOBO_TYPE_OBJECT

/* A pointer to our parent object class */
static GObjectClass *spi_device_event_controller_parent_class;
static int spi_error_code = 0;
static GdkPoint last_mouse_pos_static = {0, 0}; 
static GdkPoint *last_mouse_pos = &last_mouse_pos_static;
static unsigned int mouse_mask_state = 0;
static unsigned int mouse_button_mask =
  Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask;
static unsigned int key_modifier_mask =
  Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask | ShiftMask | LockMask | ControlMask | SPI_KEYMASK_NUMLOCK;
static unsigned int _numlock_physical_mask = Mod2Mask; /* a guess, will be reset */

static GQuark spi_dec_private_quark = 0;
static XModifierKeymap* xmkeymap = NULL;

static gboolean have_mouse_listener = FALSE;
static gboolean have_mouse_event_listener = FALSE;

static int (*x_default_error_handler) (Display *display, XErrorEvent *error_event);

typedef enum {
  SPI_DEVICE_TYPE_KBD,
  SPI_DEVICE_TYPE_MOUSE,
  SPI_DEVICE_TYPE_LAST_DEFINED
} SpiDeviceTypeCategory;

typedef struct {
  guint                             ref_count : 30;
  guint                             pending_add : 1;
  guint                             pending_remove : 1;

  Accessibility_ControllerEventMask mod_mask;
  CORBA_unsigned_long               key_val;  /* KeyCode */
} DEControllerGrabMask;

typedef struct {
  CORBA_Object          object;
  SpiDeviceTypeCategory type;
  Accessibility_EventTypeSeq    *typeseq;
} DEControllerListener;

typedef struct {
  DEControllerListener listener;

  Accessibility_KeySet             *keys;
  Accessibility_ControllerEventMask mask;
  Accessibility_EventListenerMode  *mode;	
} DEControllerKeyListener;

typedef struct {
  unsigned int last_press_keycode;
  unsigned int last_release_keycode;
  struct timeval last_press_time;
  struct timeval last_release_time;
  int have_xkb;
  int xkb_major_extension_opcode;
  int xkb_base_event_code;
  int xkb_base_error_code;
  unsigned int xkb_latch_mask;
  unsigned int pending_xkb_mod_relatch_mask;
  XkbDescPtr xkb_desc;
  KeyCode reserved_keycode;
  KeySym reserved_keysym;
  guint  reserved_reset_timeout;
} DEControllerPrivateData;

static void     spi_controller_register_with_devices          (SpiDEController           *controller);
static gboolean spi_controller_update_key_grabs               (SpiDEController           *controller,
							       Accessibility_DeviceEvent *recv);
static gboolean spi_controller_register_device_listener       (SpiDEController           *controller,
							       DEControllerListener      *l,
							       CORBA_Environment         *ev);
static gboolean spi_device_event_controller_forward_key_event (SpiDEController           *controller,
							       const XEvent              *event);
static void     spi_deregister_controller_device_listener (SpiDEController            *controller,
					                   DEControllerListener *listener,
					                   CORBA_Environment          *ev);
static void     spi_deregister_controller_key_listener (SpiDEController         *controller,
							DEControllerKeyListener *key_listener,
							CORBA_Environment       *ev);
static gboolean spi_controller_notify_mouselisteners (SpiDEController                 *controller,
						      const Accessibility_DeviceEvent *event,
						      CORBA_Environment               *ev);

static gboolean spi_eventtype_seq_contains_event (Accessibility_EventTypeSeq      *type_seq,
						  const Accessibility_DeviceEvent *event);
static gboolean spi_clear_error_state (void);
static gboolean spi_dec_poll_mouse_moved (gpointer data);
static gboolean spi_dec_poll_mouse_moving (gpointer data);
static gboolean spi_dec_poll_mouse_idle (gpointer data);

#define spi_get_display() GDK_DISPLAY()

/* Private methods */

static unsigned int
keysym_mod_mask (KeySym keysym, KeyCode keycode)
{
	/* we really should use XKB and look directly at the keymap */
	/* this is very inelegant */
	Display *display = spi_get_display ();
	unsigned int mods_rtn = 0;
	unsigned int retval = 0;
	KeySym sym_rtn;

	if (XkbLookupKeySym (display, keycode, 0, &mods_rtn, &sym_rtn) &&
	    (sym_rtn == keysym)) {
		retval = 0;
	}
	else if (XkbLookupKeySym (display, keycode, ShiftMask, &mods_rtn, &sym_rtn) &&
		 (sym_rtn == keysym)) {
		retval = ShiftMask;
	}
	else if (XkbLookupKeySym (display, keycode, Mod2Mask, &mods_rtn, &sym_rtn) &&
		 (sym_rtn == keysym)) {
		retval = Mod2Mask;
	}
	else if (XkbLookupKeySym (display, keycode, Mod3Mask, &mods_rtn, &sym_rtn) &&
		 (sym_rtn == keysym)) {
		retval = Mod3Mask;
	}
	else if (XkbLookupKeySym (display, keycode, 
				  ShiftMask | Mod2Mask, &mods_rtn, &sym_rtn) &&
		 (sym_rtn == keysym)) {
		retval = (Mod2Mask | ShiftMask);
	}
	else if (XkbLookupKeySym (display, keycode, 
				  ShiftMask | Mod3Mask, &mods_rtn, &sym_rtn) &&
		 (sym_rtn == keysym)) {
		retval = (Mod3Mask | ShiftMask);
	}
	else if (XkbLookupKeySym (display, keycode, 
				  ShiftMask | Mod4Mask, &mods_rtn, &sym_rtn) &&
		 (sym_rtn == keysym)) {
		retval = (Mod4Mask | ShiftMask);
	}
	else
		retval = 0xFFFF;
	return retval;
}

static gboolean
spi_dec_replace_map_keysym (DEControllerPrivateData *priv, KeyCode keycode, KeySym keysym)
{
#ifdef HAVE_XKB
  Display *dpy = spi_get_display ();
  XkbDescPtr desc;
  if (!(desc = XkbGetMap (dpy, XkbAllMapComponentsMask, XkbUseCoreKbd)))
    {
      fprintf (stderr, "ERROR getting map\n");
    }
  XFlush (dpy);
  XSync (dpy, False);
  if (desc && desc->map)
    {
      gint offset = desc->map->key_sym_map[keycode].offset;
      desc->map->syms[offset] = keysym; 
    }
  else
    {
      fprintf (stderr, "Error changing key map: empty server structure\n");
    }		
  XkbSetMap (dpy, XkbAllMapComponentsMask, desc);
  /**
   *  FIXME: the use of XkbChangeMap, and the reuse of the priv->xkb_desc structure, 
   * would be far preferable.
   * HOWEVER it does not seem to work using XFree 4.3. 
   **/
  /*	    XkbChangeMap (dpy, priv->xkb_desc, priv->changes); */
  XFlush (dpy);
  XSync (dpy, False);
  XkbFreeKeyboard (desc, 0, TRUE);

  return TRUE;
#else
  return FALSE;
#endif
}

static gboolean
spi_dec_reset_reserved (gpointer data)
{
  DEControllerPrivateData *priv = data;
  spi_dec_replace_map_keysym (priv, priv->reserved_keycode, priv->reserved_keysym);
  priv->reserved_reset_timeout = 0;
  return FALSE;
}

static KeyCode
keycode_for_keysym (SpiDEController *controller, long keysym, unsigned int *modmask)
{
	KeyCode keycode = 0;
	keycode = XKeysymToKeycode (spi_get_display (), (KeySym) keysym);
	if (!keycode) 
	{
		DEControllerPrivateData *priv = (DEControllerPrivateData *)
			g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);
		/* if there's no keycode available, fix it */
		if (spi_dec_replace_map_keysym (priv, priv->reserved_keycode, keysym))
		{
			keycode = priv->reserved_keycode;
			/* 
			 * queue a timer to restore the old keycode.  Ugly, but required 
			 * due to races / asynchronous X delivery.   
			 * Long-term fix is to extend the X keymap here instead of replace entries.
			 */
			priv->reserved_reset_timeout = g_timeout_add (500, spi_dec_reset_reserved, priv);
		}		
		*modmask = 0;
		return keycode;
	}
	if (modmask) 
		*modmask = keysym_mod_mask (keysym, keycode);
	return keycode;
}

static DEControllerGrabMask *
spi_grab_mask_clone (DEControllerGrabMask *grab_mask)
{
  DEControllerGrabMask *clone = g_new (DEControllerGrabMask, 1);

  memcpy (clone, grab_mask, sizeof (DEControllerGrabMask));

  clone->ref_count = 1;
  clone->pending_add = TRUE;
  clone->pending_remove = FALSE;

  return clone;
}

static void
spi_grab_mask_free (DEControllerGrabMask *grab_mask)
{
  g_free (grab_mask);
}

static gint
spi_grab_mask_compare_values (gconstpointer p1, gconstpointer p2)
{
  DEControllerGrabMask *l1 = (DEControllerGrabMask *) p1;
  DEControllerGrabMask *l2 = (DEControllerGrabMask *) p2;

  if (p1 == p2)
    {
      return 0;
    }
  else
    { 
      return ((l1->mod_mask != l2->mod_mask) || (l1->key_val != l2->key_val));
    }
}

static void
spi_dec_set_unlatch_pending (SpiDEController *controller, unsigned mask)
{
  DEControllerPrivateData *priv = 
    g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);
#ifdef SPI_XKB_DEBUG
  if (priv->xkb_latch_mask) fprintf (stderr, "unlatch pending! %x\n", 
				     priv->xkb_latch_mask);
#endif
  priv->pending_xkb_mod_relatch_mask |= priv->xkb_latch_mask; 
}
 
static void
spi_dec_clear_unlatch_pending (SpiDEController *controller)
{
  DEControllerPrivateData *priv = 
    g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);
  priv->xkb_latch_mask = 0; 
}
 
static gboolean
spi_dec_button_update_and_emit (SpiDEController *controller, 
				guint mask_return)
{
  CORBA_Environment ev;
  Accessibility_Event e;
  Accessibility_DeviceEvent mouse_e;
  gchar event_name[24];
  gboolean is_consumed = FALSE;

  if ((mask_return & mouse_button_mask) !=
      (mouse_mask_state & mouse_button_mask)) 
    {
      int button_number = 0;
      gboolean is_down = False;
      
      if (!(mask_return & Button1Mask) &&
	  (mouse_mask_state & Button1Mask)) 
	{
	  mouse_mask_state &= ~Button1Mask;
	  button_number = 1;
	} 
      else if ((mask_return & Button1Mask) &&
	       !(mouse_mask_state & Button1Mask)) 
	{
	  mouse_mask_state |= Button1Mask;
	  button_number = 1;
	  is_down = True;
	} 
      else if (!(mask_return & Button2Mask) &&
	       (mouse_mask_state & Button2Mask)) 
	{
	  mouse_mask_state &= ~Button2Mask;
	  button_number = 2;
	} 
      else if ((mask_return & Button2Mask) &&
	       !(mouse_mask_state & Button2Mask)) 
	{
	  mouse_mask_state |= Button2Mask;
	  button_number = 2;
	  is_down = True;
	} 
      else if (!(mask_return & Button3Mask) &&
	       (mouse_mask_state & Button3Mask)) 
	{
	  mouse_mask_state &= ~Button3Mask;
	  button_number = 3;
	} 
      else if ((mask_return & Button3Mask) &&
	       !(mouse_mask_state & Button3Mask)) 
	{
	  mouse_mask_state |= Button3Mask;
	  button_number = 3;
	  is_down = True;
	} 
      else if (!(mask_return & Button4Mask) &&
	       (mouse_mask_state & Button4Mask)) 
	{
	  mouse_mask_state &= ~Button4Mask;
	  button_number = 4;
	} 
      else if ((mask_return & Button4Mask) &&
	       !(mouse_mask_state & Button4Mask)) 
	{
	  mouse_mask_state |= Button4Mask;
	  button_number = 4;
	  is_down = True;
	} 
      else if (!(mask_return & Button5Mask) &&
	       (mouse_mask_state & Button5Mask)) 
	{
	  mouse_mask_state &= ~Button5Mask;
	  button_number = 5;
	}
      else if ((mask_return & Button5Mask) &&
	       !(mouse_mask_state & Button5Mask)) 
	{
	  mouse_mask_state |= Button5Mask;
	  button_number = 5;
	  is_down = True;
	}
      if (button_number) {
#ifdef SPI_DEBUG		  
	fprintf (stderr, "Button %d %s\n",
		 button_number, (is_down) ? "Pressed" : "Released");
#endif
	snprintf (event_name, 22, "mouse:button:%d%c", button_number,
		  (is_down) ? 'p' : 'r');
	/* TODO: FIXME distinguish between physical and 
	 * logical buttons 
	 */
	mouse_e.type      = (is_down) ? 
	  Accessibility_BUTTON_PRESSED_EVENT :
	  Accessibility_BUTTON_RELEASED_EVENT;
	mouse_e.id        = button_number;
	mouse_e.hw_code   = button_number;
	mouse_e.modifiers = (CORBA_unsigned_short) mouse_mask_state; 
	mouse_e.timestamp = 0;
	mouse_e.event_string = "";
	mouse_e.is_text   = CORBA_FALSE;
	is_consumed = 
	  spi_controller_notify_mouselisteners (controller, 
						&mouse_e, 
						&ev);
	e.type = event_name;
	e.source = BONOBO_OBJREF (controller->registry->desktop);
	e.detail1 = last_mouse_pos->x;
	e.detail2 = last_mouse_pos->y;
	spi_init_any_nil (&e.any_data, 
		    spi_accessible_new_return (atk_get_root (), FALSE, NULL),
		    Accessibility_ROLE_UNKNOWN,
		    "");
	CORBA_exception_init (&ev);
	if (!is_consumed)
	  {
	    Accessibility_Registry_notifyEvent (BONOBO_OBJREF (controller->registry),
						&e,
						&ev);  
	  }
	else
	  spi_dec_set_unlatch_pending (controller, mask_return);

        CORBA_free (e.any_data._value);
      }
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}


static guint
spi_dec_mouse_check (SpiDEController *controller, 
		     int *x, int *y, gboolean *moved)
{
  Accessibility_Event e;
  Accessibility_EventDetails *details;
  CORBA_Environment ev;
  int win_x_return,win_y_return;
  unsigned int mask_return;
  Window root_return, child_return;
  Display *display = spi_get_display ();

  if (display != NULL)
    XQueryPointer(display, DefaultRootWindow (display),
		  &root_return, &child_return,
		  x, y,
		  &win_x_return, &win_y_return, &mask_return);
  /* 
   * Since many clients grab the pointer, and X goes an automatic
   * pointer grab on mouse-down, we often must detect mouse button events
   * by polling rather than via a button grab. 
   * The while loop (rather than if) is used since it's possible that 
   * multiple buttons have changed state since we last checked.
   */
  if (mask_return != mouse_mask_state) 
    {
      while (spi_dec_button_update_and_emit (controller, mask_return));
    }

  if (*x != last_mouse_pos->x || *y != last_mouse_pos->y) 
    {
      e.type = "mouse:abs";  
      e.source = BONOBO_OBJREF (controller->registry->desktop);
      e.detail1 = *x;
      e.detail2 = *y;
      spi_init_any_nil (&e.any_data,
		    spi_accessible_new_return (atk_get_root (), FALSE, NULL),
		    Accessibility_ROLE_UNKNOWN,
		    "");
      CORBA_exception_init (&ev);
      Accessibility_Registry_notifyEvent (BONOBO_OBJREF (controller->registry),
					  &e,
					  &ev);
      details = e.any_data._value;
      CORBA_free (details);
      e.type = "mouse:rel";  
      e.source = BONOBO_OBJREF (controller->registry->desktop);
      e.detail1 = *x - last_mouse_pos->x;
      e.detail2 = *y - last_mouse_pos->y;
      spi_init_any_nil (&e.any_data,
		    spi_accessible_new_return (atk_get_root (), FALSE, NULL),
		    Accessibility_ROLE_UNKNOWN,
		    "");
      CORBA_exception_init (&ev);
      last_mouse_pos->x = *x;
      last_mouse_pos->y = *y;
      Accessibility_Registry_notifyEvent (BONOBO_OBJREF (controller->registry),
					  &e,
					  &ev);
      details = e.any_data._value;
      CORBA_free (details);
      *moved = True;
    }
  else
    {
      *moved = False;
    }

  return mask_return;
}

static void
spi_dec_emit_modifier_event (SpiDEController *controller, guint prev_mask, 
			     guint current_mask)
{
  Accessibility_Event e;
  CORBA_Environment ev;

#ifdef SPI_XKB_DEBUG
  fprintf (stderr, "MODIFIER CHANGE EVENT! %x to %x\n", 
	   prev_mask, current_mask);
#endif

  /* set bits for the virtual modifiers like NUMLOCK */
  if (prev_mask & _numlock_physical_mask) 
    prev_mask |= SPI_KEYMASK_NUMLOCK;
  if (current_mask & _numlock_physical_mask) 
    current_mask |= SPI_KEYMASK_NUMLOCK;

  e.type = "keyboard:modifiers";  
  e.source = BONOBO_OBJREF (controller->registry->desktop);
  e.detail1 = prev_mask & key_modifier_mask;
  e.detail2 = current_mask & key_modifier_mask;
  spi_init_any_nil (&e.any_data,
		    spi_accessible_new_return (atk_get_root (), FALSE, NULL),
		    Accessibility_ROLE_UNKNOWN,
		    "");
  CORBA_exception_init (&ev);
  Accessibility_Registry_notifyEvent (BONOBO_OBJREF (controller->registry),
				      &e,
				      &ev);
  CORBA_free (e.any_data._value);
}

static gboolean
spi_dec_poll_mouse_moved (gpointer data)
{
  SpiRegistry *registry = SPI_REGISTRY (data);
  SpiDEController *controller = registry->de_controller;
  int x, y;  
  gboolean moved;
  guint mask_return;

  mask_return = spi_dec_mouse_check (controller, &x, &y, &moved);
  
  if ((mask_return & key_modifier_mask) !=
      (mouse_mask_state & key_modifier_mask)) 
    {
      spi_dec_emit_modifier_event (controller, mouse_mask_state, mask_return);
      mouse_mask_state = mask_return;
    }

  return moved;
}

static gboolean
spi_dec_poll_mouse_idle (gpointer data)
{
  if (!have_mouse_event_listener && !have_mouse_listener)
    return FALSE;
  else if (! spi_dec_poll_mouse_moved (data)) 
    return TRUE;
  else
    {
      g_timeout_add (20, spi_dec_poll_mouse_moving, data);	    
      return FALSE;	    
    }
}

static gboolean
spi_dec_poll_mouse_moving (gpointer data)
{
  if (!have_mouse_event_listener && !have_mouse_listener)
    return FALSE;
  else if (spi_dec_poll_mouse_moved (data))
    return TRUE;
  else
    {
      g_timeout_add (100, spi_dec_poll_mouse_idle, data);	    
      return FALSE;
    }
}

#ifdef WE_NEED_UGRAB_MOUSE
static int
spi_dec_ungrab_mouse (gpointer data)
{
	Display *display = spi_get_display ();
	if (display)
	  {
	    XUngrabButton (spi_get_display (), AnyButton, AnyModifier,
			   XDefaultRootWindow (spi_get_display ()));
	  }
	return FALSE;
}
#endif

static void
spi_dec_init_mouse_listener (SpiRegistry *registry)
{
#ifdef GRAB_BUTTON
  Display *display = spi_get_display ();

  if (display)
    {
      if (XGrabButton (display, AnyButton, AnyModifier,
		       gdk_x11_get_default_root_xwindow (),
		       True, ButtonPressMask | ButtonReleaseMask,
		       GrabModeSync, GrabModeAsync, None, None) != Success) {
#ifdef SPI_DEBUG
	fprintf (stderr, "WARNING: could not grab mouse buttons!\n");
#endif
	;
      }
      XSync (display, False);
#ifdef SPI_DEBUG
      fprintf (stderr, "mouse buttons grabbed\n");
#endif
    }
#endif
}

/**
 * Eventually we can use this to make the marshalling of mask types
 * more sane, but for now we just use this to detect 
 * the use of 'virtual' masks such as numlock and convert them to
 * system-specific mask values (i.e. ModMask).
 * 
 **/
static Accessibility_ControllerEventMask
spi_dec_translate_mask (Accessibility_ControllerEventMask mask)
{
  Accessibility_ControllerEventMask tmp_mask;
  gboolean has_numlock;

  has_numlock = (mask & SPI_KEYMASK_NUMLOCK);
  tmp_mask = mask;
  if (has_numlock)
    {
      tmp_mask = mask ^ SPI_KEYMASK_NUMLOCK;
      tmp_mask |= _numlock_physical_mask;
    }
 
  return tmp_mask;
}

static DEControllerKeyListener *
spi_dec_key_listener_new (CORBA_Object                            l,
			  const Accessibility_KeySet             *keys,
			  const Accessibility_ControllerEventMask mask,
			  const Accessibility_EventTypeSeq    *typeseq,
			  const Accessibility_EventListenerMode  *mode,
			  CORBA_Environment                      *ev)
{
  DEControllerKeyListener *key_listener = g_new0 (DEControllerKeyListener, 1);
  key_listener->listener.object = bonobo_object_dup_ref (l, ev);
  key_listener->listener.type = SPI_DEVICE_TYPE_KBD;
  key_listener->keys = ORBit_copy_value (keys, TC_Accessibility_KeySet);
  key_listener->mask = spi_dec_translate_mask (mask);
  key_listener->listener.typeseq = ORBit_copy_value (typeseq, TC_Accessibility_EventTypeSeq);
  if (mode)
    key_listener->mode = ORBit_copy_value (mode, TC_Accessibility_EventListenerMode);
  else
    key_listener->mode = NULL;

#ifdef SPI_DEBUG
  g_print ("new listener, with mask %x, is_global %d, keys %p (%d)\n",
	   (unsigned int) key_listener->mask,
           (int) (mode ? mode->global : 0),
	   (void *) key_listener->keys,
	   (int) (key_listener->keys ? key_listener->keys->_length : 0));
#endif

  return key_listener;	
}

static DEControllerListener *
spi_dec_listener_new (CORBA_Object                            l,
		      const Accessibility_EventTypeSeq    *typeseq,
		      CORBA_Environment                      *ev)
{
  DEControllerListener *listener = g_new0 (DEControllerListener, 1);
  listener->object = bonobo_object_dup_ref (l, ev);
  listener->type = SPI_DEVICE_TYPE_MOUSE;
  listener->typeseq = ORBit_copy_value (typeseq, TC_Accessibility_EventTypeSeq);
  return listener;	
}

static DEControllerListener *
spi_listener_clone (DEControllerListener *listener, CORBA_Environment *ev)
{
  DEControllerListener *clone = g_new0 (DEControllerListener, 1);
  clone->object =
	  CORBA_Object_duplicate (listener->object, ev);
  clone->type = listener->type;
  clone->typeseq = ORBit_copy_value (listener->typeseq, TC_Accessibility_EventTypeSeq);
  return clone;
}

static DEControllerKeyListener *
spi_key_listener_clone (DEControllerKeyListener *key_listener, CORBA_Environment *ev)
{
  DEControllerKeyListener *clone = g_new0 (DEControllerKeyListener, 1);
  clone->listener.object =
	  CORBA_Object_duplicate (key_listener->listener.object, ev);
  clone->listener.type = SPI_DEVICE_TYPE_KBD;
  clone->keys = ORBit_copy_value (key_listener->keys, TC_Accessibility_KeySet);
  clone->mask = key_listener->mask;
  clone->listener.typeseq = ORBit_copy_value (key_listener->listener.typeseq, TC_Accessibility_EventTypeSeq);
  if (key_listener->mode)
    clone->mode = ORBit_copy_value (key_listener->mode, TC_Accessibility_EventListenerMode);
  else
    clone->mode = NULL;
  return clone;
}

static void
spi_key_listener_data_free (DEControllerKeyListener *key_listener, CORBA_Environment *ev)
{
  CORBA_free (key_listener->listener.typeseq);
  CORBA_free (key_listener->keys);
  g_free (key_listener);
}

static void
spi_key_listener_clone_free (DEControllerKeyListener *clone, CORBA_Environment *ev)
{
  CORBA_Object_release (clone->listener.object, ev);
  spi_key_listener_data_free (clone, ev);
}

static void
spi_listener_clone_free (DEControllerListener *clone, CORBA_Environment *ev)
{
  CORBA_Object_release (clone->object, ev);
  CORBA_free (clone->typeseq);
  g_free (clone);
}

static void
spi_dec_listener_free (DEControllerListener    *listener,
		       CORBA_Environment       *ev)
{
  bonobo_object_release_unref (listener->object, ev);
  if (listener->type == SPI_DEVICE_TYPE_KBD) 
    spi_key_listener_data_free ((DEControllerKeyListener *) listener, ev);
}

static void
_register_keygrab (SpiDEController      *controller,
		   DEControllerGrabMask *grab_mask)
{
  GList *l;

  l = g_list_find_custom (controller->keygrabs_list, grab_mask,
			  spi_grab_mask_compare_values);
  if (l)
    {
      DEControllerGrabMask *cur_mask = l->data;

      cur_mask->ref_count++;
      if (cur_mask->pending_remove)
        {
          cur_mask->pending_remove = FALSE;
	}
    }
  else
    {
      controller->keygrabs_list =
        g_list_prepend (controller->keygrabs_list,
			spi_grab_mask_clone (grab_mask));
    }
}

static void
_deregister_keygrab (SpiDEController      *controller,
		     DEControllerGrabMask *grab_mask)
{
  GList *l;

  l = g_list_find_custom (controller->keygrabs_list, grab_mask,
			  spi_grab_mask_compare_values);

  if (l)
    {
      DEControllerGrabMask *cur_mask = l->data;

      cur_mask->ref_count--;
      if (cur_mask->ref_count <= 0)
        {
          cur_mask->pending_remove = TRUE;
	}
    }
  else
    {
      DBG (1, g_warning ("De-registering non-existant grab"));
    }
}

static void
handle_keygrab (SpiDEController         *controller,
		DEControllerKeyListener *key_listener,
		void                   (*process_cb) (SpiDEController *controller,
						      DEControllerGrabMask *grab_mask))
{
  DEControllerGrabMask grab_mask = { 0 };

  grab_mask.mod_mask = key_listener->mask;
  if (key_listener->keys->_length == 0) /* special case means AnyKey/AllKeys */
    {
      grab_mask.key_val = AnyKey;
#ifdef SPI_DEBUG
      fprintf (stderr, "AnyKey grab!");
#endif
      process_cb (controller, &grab_mask);
    }
  else
    {
      int i;

      for (i = 0; i < key_listener->keys->_length; ++i)
        {
	  Accessibility_KeyDefinition keydef = key_listener->keys->_buffer[i];	
	  long int key_val = keydef.keysym;
	  /* X Grabs require keycodes, not keysyms */
	  if (keydef.keystring && keydef.keystring[0])
	    {
	      key_val = XStringToKeysym(keydef.keystring);		    
	    }
	  if (key_val > 0)
	    {
	      key_val = XKeysymToKeycode (spi_get_display (), (KeySym) key_val);
	    }
	  else
	    {
	      key_val = keydef.keycode;
	    }
	  grab_mask.key_val = key_val;
	  process_cb (controller, &grab_mask);
	}
    }
}

static gboolean
spi_controller_register_global_keygrabs (SpiDEController         *controller,
					 DEControllerKeyListener *key_listener)
{
  handle_keygrab (controller, key_listener, _register_keygrab);
  if (controller->xevie_display == NULL)
    return spi_controller_update_key_grabs (controller, NULL);
  else
    return TRUE;
}

static void
spi_controller_deregister_global_keygrabs (SpiDEController         *controller,
					   DEControllerKeyListener *key_listener)
{
  handle_keygrab (controller, key_listener, _deregister_keygrab);
  if (controller->xevie_display == NULL)
    spi_controller_update_key_grabs (controller, NULL);
}

static gboolean
spi_controller_register_device_listener (SpiDEController      *controller,
					 DEControllerListener *listener,
					 CORBA_Environment    *ev)
{
  DEControllerKeyListener *key_listener;
  
  switch (listener->type) {
  case SPI_DEVICE_TYPE_KBD:
      key_listener = (DEControllerKeyListener *) listener;

      controller->key_listeners = g_list_prepend (controller->key_listeners,
						  key_listener);
      if (key_listener->mode->global)
        {
	  return spi_controller_register_global_keygrabs (controller, key_listener);	
	}
      else
	      return TRUE;
      break;
  case SPI_DEVICE_TYPE_MOUSE:
      controller->mouse_listeners = g_list_prepend (controller->mouse_listeners, listener);
      if (!have_mouse_listener)
        {
          have_mouse_listener = TRUE;
          if (!have_mouse_event_listener)
            g_timeout_add (100, spi_dec_poll_mouse_idle, controller->registry);
        }
      break;
  default:
      DBG (1, g_warning ("listener registration for unknown device type.\n"));
      break;
  }
  return FALSE; 
}

static gboolean
spi_controller_notify_mouselisteners (SpiDEController                 *controller,
				      const Accessibility_DeviceEvent *event,
				      CORBA_Environment               *ev)
{
  GList   *l;
  GSList  *notify = NULL, *l2;
  GList  **listeners = &controller->mouse_listeners;
  gboolean is_consumed;
#ifdef SPI_KEYEVENT_DEBUG
  gboolean found = FALSE;
#endif
  if (!listeners)
    {
      return FALSE;
    }

  for (l = *listeners; l; l = l->next)
    {
       DEControllerListener *listener = l->data;

       if (spi_eventtype_seq_contains_event (listener->typeseq, event))
         {
           Accessibility_DeviceEventListener ls = listener->object;

	   if (ls != CORBA_OBJECT_NIL)
	     {
	       /* we clone (don't dup) the listener, to avoid refcount inc. */
	       notify = g_slist_prepend (notify,
					 spi_listener_clone (listener, ev));
#ifdef SPI_KEYEVENT_DEBUG
               found = TRUE;
#endif
	     }
         }
    }

#ifdef SPI_KEYEVENT_DEBUG
  if (!found)
    {
      g_print ("no match for event\n");
    }
#endif

  is_consumed = FALSE;
  for (l2 = notify; l2 && !is_consumed; l2 = l2->next)
    {
      DEControllerListener *listener = l2->data;	    
      Accessibility_DeviceEventListener ls = listener->object;

      CORBA_exception_init (ev);
      is_consumed = Accessibility_DeviceEventListener_notifyEvent (ls, event, ev);
      if (BONOBO_EX (ev))
        {
          is_consumed = FALSE;
	  DBG (2, g_warning ("error notifying listener, removing it\n"));
	  spi_deregister_controller_device_listener (controller, listener,
						     ev);
          CORBA_exception_free (ev);
        }
      
      spi_listener_clone_free ((DEControllerListener *) l2->data, ev);
    }

  for (; l2; l2 = l2->next)
    {
      DEControllerListener *listener = l2->data;	    
      spi_listener_clone_free (listener, ev);
      /* clone doesn't have its own ref, so don't use spi_device_listener_free */
    }

  g_slist_free (notify);

#ifdef SPI_DEBUG
  if (is_consumed) g_message ("consumed\n");
#endif
  return is_consumed;
}

static void
spi_device_event_controller_forward_mouse_event (SpiDEController *controller,
						 XEvent *xevent)
{
  Accessibility_Event e;
  Accessibility_DeviceEvent mouse_e;
  CORBA_Environment ev;
  gchar event_name[24];
  gboolean is_consumed = FALSE;
  gboolean xkb_mod_unlatch_occurred;
  XButtonEvent *xbutton_event = (XButtonEvent *) xevent;

  int button = xbutton_event->button;
  
  unsigned int mouse_button_state = xbutton_event->state;

  switch (button)
    {
    case 1:
	    mouse_button_state |= Button1Mask;
	    break;
    case 2:
	    mouse_button_state |= Button2Mask;
	    break;
    case 3:
	    mouse_button_state |= Button3Mask;
	    break;
    case 4:
	    mouse_button_state |= Button4Mask;
	    break;
    case 5:
	    mouse_button_state |= Button5Mask;
	    break;
    }

  last_mouse_pos->x = ((XButtonEvent *) xevent)->x_root;
  last_mouse_pos->y = ((XButtonEvent *) xevent)->y_root;

#ifdef SPI_DEBUG  
  fprintf (stderr, "mouse button %d %s (%x)\n",
	   xbutton_event->button, 
	   (xevent->type == ButtonPress) ? "Press" : "Release",
	   mouse_button_state);
#endif
  snprintf (event_name, 22, "mouse:button:%d%c", button,
	    (xevent->type == ButtonPress) ? 'p' : 'r');

  /* TODO: FIXME distinguish between physical and logical buttons */
  mouse_e.type      = (xevent->type == ButtonPress) ? 
                      Accessibility_BUTTON_PRESSED_EVENT :
                      Accessibility_BUTTON_RELEASED_EVENT;
  mouse_e.id        = button;
  mouse_e.hw_code   = button;
  mouse_e.modifiers = (CORBA_unsigned_short) xbutton_event->state;
  mouse_e.timestamp = (CORBA_unsigned_long) xbutton_event->time;
  mouse_e.event_string = "";
  mouse_e.is_text   = CORBA_FALSE;
  if ((mouse_button_state & mouse_button_mask) != 
       (mouse_mask_state & mouse_button_mask))
    { 
      if ((mouse_mask_state & key_modifier_mask) !=
	  (mouse_button_state & key_modifier_mask))
	spi_dec_emit_modifier_event (controller, 
				     mouse_mask_state, mouse_button_state);
      mouse_mask_state = mouse_button_state;
      is_consumed = 
	spi_controller_notify_mouselisteners (controller, &mouse_e, &ev);
      e.type = CORBA_string_dup (event_name);
      e.source = BONOBO_OBJREF (controller->registry->desktop);
      e.detail1 = last_mouse_pos->x;
      e.detail2 = last_mouse_pos->y;
      spi_init_any_nil (&e.any_data,
		    spi_accessible_new_return (atk_get_root (), FALSE, NULL),
		    Accessibility_ROLE_UNKNOWN,
		    "");
      CORBA_exception_init (&ev);
      
      Accessibility_Registry_notifyEvent (BONOBO_OBJREF (controller->registry),
					  &e,
					  &ev);
      CORBA_free (e.any_data._value);
    }

  xkb_mod_unlatch_occurred = (xevent->type == ButtonPress ||
			      xevent->type == ButtonRelease);
  
  /* if client wants to consume this event, and XKB latch state was
   *   unset by this button event, we reset it
   */
  if (is_consumed && xkb_mod_unlatch_occurred)
    spi_dec_set_unlatch_pending (controller, mouse_mask_state);
  
  XAllowEvents (spi_get_display (),
		(is_consumed) ? SyncPointer : ReplayPointer,
		CurrentTime);
}

static GdkFilterReturn
global_filter_fn (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
  XEvent *xevent = gdk_xevent;
  SpiDEController *controller;
  DEControllerPrivateData *priv;
  Display *display = spi_get_display ();
  controller = SPI_DEVICE_EVENT_CONTROLLER (data);
  priv = (DEControllerPrivateData *)
	  g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);  

  if (xevent->type == MappingNotify)
    xmkeymap = NULL;

  if (xevent->type == KeyPress || xevent->type == KeyRelease)
    {
      if (controller->xevie_display == NULL)
        {
          gboolean is_consumed;

          is_consumed =
            spi_device_event_controller_forward_key_event (controller, xevent);

          if (is_consumed)
            {
              int n_events;
              int i;
              XEvent next_event;
              n_events = XPending (display);

#ifdef SPI_KEYEVENT_DEBUG
              g_print ("Number of events pending: %d\n", n_events);
#endif
              for (i = 0; i < n_events; i++)
                {
                  XNextEvent (display, &next_event);
		  if (next_event.type != KeyPress &&
		      next_event.type != KeyRelease)
			g_warning ("Unexpected event type %d in queue", next_event.type);
                 }

              XAllowEvents (display, AsyncKeyboard, CurrentTime);
              if (n_events)
                XUngrabKeyboard (display, CurrentTime);
            }
          else
            {
              if (xevent->type == KeyPress)
                wait_for_release_event (xevent, controller);
              XAllowEvents (display, ReplayKeyboard, CurrentTime);
            }
        }

      return GDK_FILTER_CONTINUE;
    }
  if (xevent->type == ButtonPress || xevent->type == ButtonRelease)
    {
      spi_device_event_controller_forward_mouse_event (controller, xevent);
    }
  if (xevent->type == priv->xkb_base_event_code)
    {
      XkbAnyEvent * xkb_ev = (XkbAnyEvent *) xevent;
      /* ugly but probably necessary...*/
      XSynchronize (display, TRUE);

      if (xkb_ev->xkb_type == XkbStateNotify)
        {
	  XkbStateNotifyEvent *xkb_snev =
		  (XkbStateNotifyEvent *) xkb_ev;
	  /* check the mouse, to catch mouse events grabbed by
	   * another client; in case we should revert this XKB delatch 
	   */
	  if (!priv->pending_xkb_mod_relatch_mask)
	    {
	      int x, y;
	      gboolean moved;
	      spi_dec_mouse_check (controller, &x, &y, &moved);
	    }
	  /* we check again, since the previous call may have 
	     changed this flag */
	  if (priv->pending_xkb_mod_relatch_mask)
	    {
	      unsigned int feedback_mask;
#ifdef SPI_XKB_DEBUG
	      fprintf (stderr, "relatching %x\n",
		       priv->pending_xkb_mod_relatch_mask);
#endif
	      /* temporarily turn off the latch bell, if it's on */
	      XkbGetControls (display,
			      XkbAccessXFeedbackMask,
			      priv->xkb_desc);
	      feedback_mask = priv->xkb_desc->ctrls->ax_options;
	      if (feedback_mask & XkbAX_StickyKeysFBMask)
	      {
	        XkbControlsChangesRec changes = {XkbAccessXFeedbackMask,
						 0, False};      
	        priv->xkb_desc->ctrls->ax_options
			      &= ~(XkbAX_StickyKeysFBMask);
	        XkbChangeControls (display, priv->xkb_desc, &changes);
	      }
	      /* TODO: account for lock as well as latch */
	      XkbLatchModifiers (display,
				 XkbUseCoreKbd,
				 priv->pending_xkb_mod_relatch_mask,
				 priv->pending_xkb_mod_relatch_mask);
	      if (feedback_mask & XkbAX_StickyKeysFBMask)
	      {	
	        XkbControlsChangesRec changes = {XkbAccessXFeedbackMask,
						 0, False};      
		priv->xkb_desc->ctrls->ax_options = feedback_mask;
		XkbChangeControls (display, priv->xkb_desc, &changes);
	      }
#ifdef SPI_XKB_DEBUG
	      fprintf (stderr, "relatched %x\n",
		       priv->pending_xkb_mod_relatch_mask);
#endif
	      priv->pending_xkb_mod_relatch_mask = 0;
	    }
	  else
	    {
	      priv->xkb_latch_mask = xkb_snev->latched_mods;
	    }
	}
        else
	       DBG (2, g_warning ("XKB event %d\n", xkb_ev->xkb_type));
      XSynchronize (display, FALSE);
    }
  
  return GDK_FILTER_CONTINUE;
}

static int
_spi_controller_device_error_handler (Display *display, XErrorEvent *error)
{
  if (error->error_code == BadAccess) 
    {  
      g_message ("Could not complete key grab: grab already in use.\n");
      spi_error_code = BadAccess;
      return 0;
    }
  else 
    {
      return (*x_default_error_handler) (display, error);
    }
}

static void
spi_controller_register_with_devices (SpiDEController *controller)
{
  DEControllerPrivateData *priv = (DEControllerPrivateData *) 
	  g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);	 
  /* FIXME: should check for extension first! */
  XTestGrabControl (spi_get_display (), True);

  /* calls to device-specific implementations and routines go here */
  /* register with: keyboard hardware code handler */
  /* register with: (translated) keystroke handler */

  priv->have_xkb = XkbQueryExtension (spi_get_display (),
				      &priv->xkb_major_extension_opcode,
				      &priv->xkb_base_event_code,
				      &priv->xkb_base_error_code, NULL, NULL);
  if (priv->have_xkb)
    {
      gint i;
      guint64 reserved = 0;
      priv->xkb_desc = XkbGetMap (spi_get_display (), XkbKeySymsMask, XkbUseCoreKbd);
      XkbSelectEvents (spi_get_display (),
		       XkbUseCoreKbd,
		       XkbStateNotifyMask, XkbStateNotifyMask);	    
      _numlock_physical_mask = XkbKeysymToModifiers (spi_get_display (), 
						     XK_Num_Lock);
      for (i = priv->xkb_desc->max_key_code; i >= priv->xkb_desc->min_key_code; --i)
      {
	  if (priv->xkb_desc->map->key_sym_map[i].kt_index[0] == XkbOneLevelIndex)
	  { 
	      if (XKeycodeToKeysym (spi_get_display (), i, 0) != 0)
	      {
		  /* don't use this one if there's a grab client! */
		  gdk_error_trap_push ();
		  XGrabKey (spi_get_display (), i, 0, 
			    gdk_x11_get_default_root_xwindow (),
			    TRUE,
			    GrabModeSync, GrabModeSync);
		  XSync (spi_get_display (), TRUE);
		  XUngrabKey (spi_get_display (), i, 0, 
			      gdk_x11_get_default_root_xwindow ());
		  if (!gdk_error_trap_pop ())
		  {
		      reserved = i;
		      break;
		  }
	      }
	  }
      }
      if (reserved) 
      {
	  priv->reserved_keycode = reserved;
	  priv->reserved_keysym = XKeycodeToKeysym (spi_get_display (), reserved, 0);
      }
      else
      { 
	  priv->reserved_keycode = XKeysymToKeycode (spi_get_display (), XK_numbersign);
	  priv->reserved_keysym = XK_numbersign;
      }
#ifdef SPI_RESERVED_DEBUG
      unsigned sym = 0;
      sym = XKeycodeToKeysym (spi_get_display (), reserved, 0);
      fprintf (stderr, "%x\n", sym);
      fprintf (stderr, "setting the reserved keycode to %d (%s)\n", 
	       reserved, 
	       XKeysymToString (XKeycodeToKeysym (spi_get_display (),
                                                            reserved, 0)));
#endif
    }	

  gdk_window_add_filter (NULL, global_filter_fn, controller);

  gdk_window_set_events (gdk_get_default_root_window (),
			 GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

  x_default_error_handler = XSetErrorHandler (_spi_controller_device_error_handler);
}

static gboolean
spi_key_set_contains_key (Accessibility_KeySet            *key_set,
			  const Accessibility_DeviceEvent *key_event)
{
  gint i;
  gint len;

  if (!key_set)
    {
      g_print ("null key set!");
      return TRUE;
    }

  len = key_set->_length;
  
  if (len == 0) /* special case, means "all keys/any key" */
    {
#ifdef SPI_DEBUG
      g_print ("anykey\n");
#endif
      return TRUE;
    }

  for (i = 0; i < len; ++i)
    {
#ifdef SPI_KEYEVENT_DEBUG	    
      g_print ("key_set[%d] event = %d, code = %d; key_event %d, code %d, string %s\n",
                i,
                (int)key_set->_buffer[i].keysym,
                (int) key_set->_buffer[i].keycode,
                (int) key_event->id,
                (int) key_event->hw_code,
                key_event->event_string); 
#endif
      if (key_set->_buffer[i].keysym == (CORBA_long) key_event->id)
        {
          return TRUE;
	}
      if (key_set->_buffer[i].keycode == (CORBA_long) key_event->hw_code)
        {
          return TRUE;
	}
      if (key_event->event_string && key_event->event_string[0] &&
	  !strcmp (key_set->_buffer[i].keystring, key_event->event_string))
        {
          return TRUE;
	}
    }

  return FALSE;
}

static gboolean
spi_eventtype_seq_contains_event (Accessibility_EventTypeSeq      *type_seq,
				  const Accessibility_DeviceEvent *event)
{
  gint i;
  gint len;


  if (!type_seq)
    {
      g_print ("null type seq!");
      return TRUE;
    }

  len = type_seq->_length;
  
  if (len == 0) /* special case, means "all events/any event" */
    {
      return TRUE;
    }

  for (i = 0; i < len; ++i)
    {
#ifdef SPI_DEBUG	    
      g_print ("type_seq[%d] = %d; event type = %d\n", i,
	       (int) type_seq->_buffer[i], (int) event->type);
#endif      
      if (type_seq->_buffer[i] == (CORBA_long) event->type)
        {
          return TRUE;
	}
    }
  
  return FALSE;
}

static gboolean
spi_key_event_matches_listener (const Accessibility_DeviceEvent *key_event,
				DEControllerKeyListener         *listener,
				CORBA_boolean                    is_system_global)
{
  if (((key_event->modifiers & 0xFF) == (CORBA_unsigned_short) (listener->mask & 0xFF)) &&
       spi_key_set_contains_key (listener->keys, key_event) &&
       spi_eventtype_seq_contains_event (listener->listener.typeseq, key_event) && 
      (is_system_global == listener->mode->global))
    {
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
spi_controller_notify_keylisteners (SpiDEController                 *controller,
				    Accessibility_DeviceEvent       *key_event,
				    CORBA_boolean                    is_system_global,
				    CORBA_Environment               *ev)
{
  GList   *l;
  GSList  *notify = NULL, *l2;
  GList  **key_listeners = &controller->key_listeners;
  gboolean is_consumed;

  if (!key_listeners)
    {
      return FALSE;
    }

  /* set the NUMLOCK event mask bit if appropriate: see bug #143702 */
  if (key_event->modifiers & _numlock_physical_mask)
      key_event->modifiers |= SPI_KEYMASK_NUMLOCK;

  for (l = *key_listeners; l; l = l->next)
    {
       DEControllerKeyListener *key_listener = l->data;

       if (spi_key_event_matches_listener (key_event, key_listener, is_system_global))
         {
           Accessibility_DeviceEventListener ls = key_listener->listener.object;

	   if (ls != CORBA_OBJECT_NIL)
	     {
	       /* we clone (don't dup) the listener, to avoid refcount inc. */
	       notify = g_slist_prepend (notify,
					 spi_key_listener_clone (key_listener, ev));
	     }
         }
    }

#ifdef SPI_KEYEVENT_DEBUG
  if (!notify)
    {
      g_print ("no match for event\n");
    }
#endif

  is_consumed = FALSE;
  for (l2 = notify; l2 && !is_consumed; l2 = l2->next)
    {
      DEControllerKeyListener *key_listener = l2->data;	    
      Accessibility_DeviceEventListener ls = key_listener->listener.object;

      is_consumed = Accessibility_DeviceEventListener_notifyEvent (ls, key_event, ev) &&
	            key_listener->mode->preemptive;

      if (BONOBO_EX (ev))
        {
          is_consumed = FALSE;
	  spi_deregister_controller_key_listener (controller, key_listener,
						  ev);
	  CORBA_exception_free (ev);
        }

      spi_key_listener_clone_free (key_listener, ev);
    }

  for (; l2; l2 = l2->next)
    {
      DEControllerKeyListener *key_listener = l2->data;	    
      spi_key_listener_clone_free (key_listener, ev);
      /* clone doesn't have its own ref, so don't use spi_dec_listener_free */
    }

  g_slist_free (notify);

#ifdef SPI_DEBUG
  if (is_consumed) g_message ("consumed\n");
#endif
  return is_consumed;
}

static gboolean
spi_clear_error_state (void)
{
	gboolean retval = spi_error_code != 0;
	spi_error_code = 0;
	return retval;
}

static Accessibility_DeviceEvent
spi_keystroke_from_x_key_event (XKeyEvent *x_key_event)
{
  Accessibility_DeviceEvent key_event;
  KeySym keysym;
  const int cbuf_bytes = 20;
  char cbuf [21];
  int nbytes;

  nbytes = XLookupString (x_key_event, cbuf, cbuf_bytes, &keysym, NULL);  
  key_event.id = (CORBA_long)(keysym);
  key_event.hw_code = (CORBA_short) x_key_event->keycode;
  if (((XEvent *) x_key_event)->type == KeyPress)
    {
      key_event.type = Accessibility_KEY_PRESSED_EVENT;
    }
  else
    {
      key_event.type = Accessibility_KEY_RELEASED_EVENT;
    } 
  key_event.modifiers = (CORBA_unsigned_short)(x_key_event->state);
  key_event.is_text = CORBA_FALSE;
  switch (keysym)
    {
      case ' ':
        key_event.event_string = CORBA_string_dup ("space");
        break;
      case XK_Tab:
        key_event.event_string = CORBA_string_dup ("Tab");
	break;
      case XK_BackSpace:
        key_event.event_string = CORBA_string_dup ("Backspace");
	break;
      case XK_Return:
        key_event.event_string = CORBA_string_dup ("Return");
	break;
      case XK_Home:
        key_event.event_string = CORBA_string_dup ("Home");
	break;
      case XK_Page_Down:
        key_event.event_string = CORBA_string_dup ("Page_Down");
	break;
      case XK_Page_Up:
        key_event.event_string = CORBA_string_dup ("Page_Up");
	break;
      case XK_F1:
        key_event.event_string = CORBA_string_dup ("F1");
	break;
      case XK_F2:
        key_event.event_string = CORBA_string_dup ("F2");
	break;
      case XK_F3:
        key_event.event_string = CORBA_string_dup ("F3");
	break;
      case XK_F4:
        key_event.event_string = CORBA_string_dup ("F4");
	break;
      case XK_F5:
        key_event.event_string = CORBA_string_dup ("F5");
	break;
      case XK_F6:
        key_event.event_string = CORBA_string_dup ("F6");
	break;
      case XK_F7:
        key_event.event_string = CORBA_string_dup ("F7");
	break;
      case XK_F8:
        key_event.event_string = CORBA_string_dup ("F8");
	break;
      case XK_F9:
        key_event.event_string = CORBA_string_dup ("F9");
	break;
      case XK_F10:
        key_event.event_string = CORBA_string_dup ("F10");
	break;
      case XK_F11:
        key_event.event_string = CORBA_string_dup ("F11");
	break;
      case XK_F12:
        key_event.event_string = CORBA_string_dup ("F12");
	break;
      case XK_End:
        key_event.event_string = CORBA_string_dup ("End");
	break;
      case XK_Escape:
        key_event.event_string = CORBA_string_dup ("Escape");
	break;
      case XK_Up:
        key_event.event_string = CORBA_string_dup ("Up");
	break;
      case XK_Down:
        key_event.event_string = CORBA_string_dup ("Down");
	break;
      case XK_Left:
        key_event.event_string = CORBA_string_dup ("Left");
	break;
      case XK_Right:
        key_event.event_string = CORBA_string_dup ("Right");
	break;
      default:
        if (nbytes > 0)
          {
	    gunichar c;
	    cbuf[nbytes] = '\0'; /* OK since length is cbuf_bytes+1 */
            key_event.event_string = CORBA_string_dup (cbuf);
	    c = keysym2ucs (keysym);
	    if (c > 0 && !g_unichar_iscntrl (c))
	      {
	        key_event.is_text = CORBA_TRUE; 
		/* incorrect for some composed chars? */
	      }
          }
        else
          {
            key_event.event_string = CORBA_string_dup ("");
          }
    }

  key_event.timestamp = (CORBA_unsigned_long) x_key_event->time;
#ifdef SPI_KEYEVENT_DEBUG
  {
    char *pressed_str  = "pressed";
    char *released_str = "released";
    char *state_ptr;

    if (key_event.type == Accessibility_KEY_PRESSED_EVENT)
      state_ptr = pressed_str;
    else
      state_ptr = released_str;
 
    fprintf (stderr,
	     "Key %lu %s (%c), modifiers %d; string=%s [%x] %s\n",
	     (unsigned long) keysym,
	     state_ptr,
	     keysym ? (int) keysym : '*',
	     (int) x_key_event->state,
	     key_event.event_string,
	     key_event.event_string[0],
	     (key_event.is_text == CORBA_TRUE) ? "(text)" : "(not text)");
  }
#endif
#ifdef SPI_DEBUG
  fprintf (stderr, "%s%c\n",
     (x_key_event->state & Mod1Mask)?"Alt-":"",
     ((x_key_event->state & ShiftMask)^(x_key_event->state & LockMask))?
     g_ascii_toupper (keysym) : g_ascii_tolower (keysym));
  fprintf (stderr, "serial: %x Time: %x\n", x_key_event->serial, x_key_event->time);
#endif /* SPI_DEBUG */
  return key_event;	
}

static gboolean
spi_controller_update_key_grabs (SpiDEController           *controller,
				 Accessibility_DeviceEvent *recv)
{
  GList *l, *next;
  gboolean   update_failed = FALSE;
  KeyCode keycode = 0;
  
  g_return_val_if_fail (controller != NULL, FALSE);

  /*
   * masks known to work with default RH 7.1+:
   * 0 (no mods), LockMask, Mod1Mask, Mod2Mask, ShiftMask,
   * ShiftMask|LockMask, Mod1Mask|LockMask, Mod2Mask|LockMask,
   * ShiftMask|Mod1Mask, ShiftMask|Mod2Mask, Mod1Mask|Mod2Mask,
   * ShiftMask|LockMask|Mod1Mask, ShiftMask|LockMask|Mod2Mask,
   *
   * ControlMask grabs are broken, must be in use already
   */
  if (recv)
    keycode = keycode_for_keysym (controller, recv->id, NULL);
  for (l = controller->keygrabs_list; l; l = next)
    {
      gboolean do_remove;
      gboolean re_issue_grab;
      DEControllerGrabMask *grab_mask = l->data;

      next = l->next;

      re_issue_grab = recv &&
	      (recv->modifiers & grab_mask->mod_mask) &&
	      (grab_mask->key_val == keycode);

#ifdef SPI_DEBUG
      fprintf (stderr, "mask=%lx %lx (%c%c) %s\n",
	       (long int) grab_mask->key_val,
	       (long int) grab_mask->mod_mask,
	       grab_mask->pending_add ? '+' : '.',
	       grab_mask->pending_remove ? '-' : '.',
	       re_issue_grab ? "re-issue": "");
#endif

      do_remove = FALSE;

      if (grab_mask->pending_add && grab_mask->pending_remove)
        {
          do_remove = TRUE;
	}
      else if (grab_mask->pending_remove)
        {
#ifdef SPI_DEBUG
      fprintf (stderr, "ungrabbing, mask=%x\n", grab_mask->mod_mask);
#endif
	  XUngrabKey (spi_get_display (),
		      grab_mask->key_val,
		      grab_mask->mod_mask,
		      gdk_x11_get_default_root_xwindow ());

          do_remove = TRUE;
	}
      else if (grab_mask->pending_add || re_issue_grab)
        {

#ifdef SPI_DEBUG
	  fprintf (stderr, "grab %d with mask %x\n", grab_mask->key_val, grab_mask->mod_mask);
#endif
          XGrabKey (spi_get_display (),
		    grab_mask->key_val,
		    grab_mask->mod_mask,
		    gdk_x11_get_default_root_xwindow (),
		    True,
		    GrabModeSync,
		    GrabModeSync);
	  XSync (spi_get_display (), False);
	  update_failed = spi_clear_error_state ();
	  if (update_failed) {
		  while (grab_mask->ref_count > 0) --grab_mask->ref_count;
		  do_remove = TRUE;
	  }
	}

      grab_mask->pending_add = FALSE;
      grab_mask->pending_remove = FALSE;

      if (do_remove)
        {
          g_assert (grab_mask->ref_count <= 0);

	  controller->keygrabs_list = g_list_delete_link (
	    controller->keygrabs_list, l);

	  spi_grab_mask_free (grab_mask);
	}

    } 

  return ! update_failed;
}

/*
 * Implemented GObject::finalize
 */
static void
spi_device_event_controller_object_finalize (GObject *object)
{
  SpiDEController *controller;
  DEControllerPrivateData *private;
  controller = SPI_DEVICE_EVENT_CONTROLLER (object);

#ifdef SPI_DEBUG
  fprintf(stderr, "spi_device_event_controller_object_finalize called\n");
#endif
  /* disconnect any special listeners, get rid of outstanding keygrabs */
  XUngrabKey (spi_get_display (), AnyKey, AnyModifier, DefaultRootWindow (spi_get_display ()));

#ifdef HAVE_XEVIE
  if (controller->xevie_display != NULL)
    {
      XevieEnd(controller->xevie_display);
#ifdef SPI_KEYEVENT_DEBUG
      printf("XevieEnd(dpy) finished \n");
#endif
    }
#endif

  private = g_object_get_data (G_OBJECT (controller), "spi-dec-private");
  if (private->xkb_desc)
	  XkbFreeKeyboard (private->xkb_desc, 0, True);
  g_free (private);
  spi_device_event_controller_parent_class->finalize (object);
}

/*
 * CORBA Accessibility::DEController::registerKeystrokeListener
 *     method implementation
 */
static CORBA_boolean
impl_register_keystroke_listener (PortableServer_Servant                  servant,
				  const Accessibility_DeviceEventListener l,
				  const Accessibility_KeySet             *keys,
				  const Accessibility_ControllerEventMask mask,
				  const Accessibility_EventTypeSeq       *type,
				  const Accessibility_EventListenerMode  *mode,
				  CORBA_Environment                      *ev)
{
  SpiDEController *controller = SPI_DEVICE_EVENT_CONTROLLER (
	  bonobo_object_from_servant (servant));
  DEControllerKeyListener *dec_listener;
#ifdef SPI_DEBUG
  fprintf (stderr, "registering keystroke listener %p with maskVal %lu\n",
	   (void *) l, (unsigned long) mask);
#endif
  dec_listener = spi_dec_key_listener_new (l, keys, mask, type, mode, ev);
  return spi_controller_register_device_listener (
	  controller, (DEControllerListener *) dec_listener, ev);
}


/*
 * CORBA Accessibility::DEController::registerDeviceEventListener
 *     method implementation
 */
static CORBA_boolean
impl_register_device_listener (PortableServer_Servant                  servant,
			       const Accessibility_DeviceEventListener l,
			       const Accessibility_EventTypeSeq       *event_types,
			       CORBA_Environment                      *ev)
{
  SpiDEController *controller = SPI_DEVICE_EVENT_CONTROLLER (
	  bonobo_object_from_servant (servant));
  DEControllerListener *dec_listener;

  dec_listener = spi_dec_listener_new (l, event_types, ev);
  return spi_controller_register_device_listener (
	  controller, (DEControllerListener *) dec_listener, ev);
}

typedef struct {
	CORBA_Environment       *ev;
	DEControllerListener    *listener;
} RemoveListenerClosure;

static SpiReEntrantContinue
remove_listener_cb (GList * const *list,
		    gpointer       user_data)
{
  DEControllerListener  *listener = (*list)->data;
  RemoveListenerClosure *ctx = user_data;

  if (CORBA_Object_is_equivalent (ctx->listener->object,
				  listener->object, ctx->ev))
    {
      spi_re_entrant_list_delete_link (list);
      spi_dec_listener_free (listener, ctx->ev);
    }

  return SPI_RE_ENTRANT_CONTINUE;
}

static SpiReEntrantContinue
copy_key_listener_cb (GList * const *list,
		      gpointer       user_data)
{
  DEControllerKeyListener  *key_listener = (*list)->data;
  RemoveListenerClosure    *ctx = user_data;

  if (CORBA_Object_is_equivalent (ctx->listener->object,
				  key_listener->listener.object, ctx->ev))
    {
      /* TODO: FIXME aggregate keys in case the listener is registered twice */
      DEControllerKeyListener *ctx_key_listener = 
	(DEControllerKeyListener *) ctx->listener; 
      CORBA_free (ctx_key_listener->keys);	    
      ctx_key_listener->keys = ORBit_copy_value (key_listener->keys, TC_Accessibility_KeySet);
    }

  return SPI_RE_ENTRANT_CONTINUE;
}

static void
spi_deregister_controller_device_listener (SpiDEController            *controller,
					   DEControllerListener *listener,
					   CORBA_Environment          *ev)
{
  RemoveListenerClosure  ctx;

  ctx.ev = ev;
  ctx.listener = listener;

  spi_re_entrant_list_foreach (&controller->mouse_listeners,
			       remove_listener_cb, &ctx);
  if (!controller->mouse_listeners)
    have_mouse_listener = FALSE;
}

static void
spi_deregister_controller_key_listener (SpiDEController            *controller,
					DEControllerKeyListener    *key_listener,
					CORBA_Environment          *ev)
{
  RemoveListenerClosure  ctx;

  ctx.ev = ev;
  ctx.listener = (DEControllerListener *) key_listener;

  /* special case, copy keyset from existing controller list entry */
  if (key_listener->keys->_length == 0) 
    {
      spi_re_entrant_list_foreach (&controller->key_listeners,
				  copy_key_listener_cb, &ctx);
    }
  
  spi_controller_deregister_global_keygrabs (controller, key_listener);

  spi_re_entrant_list_foreach (&controller->key_listeners,
				remove_listener_cb, &ctx);

}

/*
 * CORBA Accessibility::DEController::deregisterKeystrokeListener
 *     method implementation
 */
static void
impl_deregister_keystroke_listener (PortableServer_Servant                  servant,
				    const Accessibility_DeviceEventListener l,
				    const Accessibility_KeySet             *keys,
				    const Accessibility_ControllerEventMask mask,
				    const Accessibility_EventTypeSeq       *type,
				    CORBA_Environment                      *ev)
{
  DEControllerKeyListener  *key_listener;
  SpiDEController *controller;

  controller = SPI_DEVICE_EVENT_CONTROLLER (bonobo_object (servant));

  key_listener = spi_dec_key_listener_new (l, keys, mask, type, NULL, ev);

#ifdef SPI_DEREGISTER_DEBUG
  fprintf (stderr, "deregistering keystroke listener %p with maskVal %lu\n",
	   (void *) l, (unsigned long) mask->value);
#endif

  spi_deregister_controller_key_listener (controller, key_listener, ev);

  spi_dec_listener_free ((DEControllerListener *) key_listener, ev);
}

/*
 * CORBA Accessibility::DEController::deregisterDeviceEventListener
 *     method implementation
 */
static void
impl_deregister_device_listener (PortableServer_Servant                  servant,
				 const Accessibility_DeviceEventListener l,
				 const Accessibility_EventTypeSeq       *event_types,
				 CORBA_Environment                      *ev)
{
  SpiDEController *controller;
  DEControllerListener *listener = 
          spi_dec_listener_new (l, event_types, ev);

  controller = SPI_DEVICE_EVENT_CONTROLLER (bonobo_object (servant));

  spi_deregister_controller_device_listener (controller, listener, ev);

  spi_dec_listener_free (listener, ev);
}

static unsigned int dec_xkb_get_slowkeys_delay (SpiDEController *controller)
{
  unsigned int retval = 0;
  DEControllerPrivateData *priv = (DEControllerPrivateData *)
	  g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);
#ifdef HAVE_XKB
#ifdef XKB_HAS_GET_SLOW_KEYS_DELAY	
  retval = XkbGetSlowKeysDelay (spi_get_display (),
				XkbUseCoreKbd, &bounce_delay);
#else
  if (!(priv->xkb_desc == (XkbDescPtr) BadAlloc || priv->xkb_desc == NULL))
    {
      Status s = XkbGetControls (spi_get_display (),
				 XkbAllControlsMask, priv->xkb_desc);
      if (s == Success)
        {
	 if (priv->xkb_desc->ctrls->enabled_ctrls & XkbSlowKeysMask)
		 retval = priv->xkb_desc->ctrls->slow_keys_delay;
	}
    }
#endif
#endif
#ifdef SPI_XKB_DEBUG
	fprintf (stderr, "SlowKeys delay: %d\n", (int) retval);
#endif
        return retval;
}

static unsigned int dec_xkb_get_bouncekeys_delay (SpiDEController *controller)
{
  unsigned int retval = 0;
  DEControllerPrivateData *priv = (DEControllerPrivateData *)
	  g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);
#ifdef HAVE_XKB
#ifdef XKB_HAS_GET_BOUNCE_KEYS_DELAY	
  retval = XkbGetBounceKeysDelay (spi_get_display (),
				  XkbUseCoreKbd, &bounce_delay);
#else
  if (!(priv->xkb_desc == (XkbDescPtr) BadAlloc || priv->xkb_desc == NULL))
    {
      Status s = XkbGetControls (spi_get_display (),
				 XkbAllControlsMask, priv->xkb_desc);
      if (s == Success)
        {
	  if (priv->xkb_desc->ctrls->enabled_ctrls & XkbBounceKeysMask)
		  retval = priv->xkb_desc->ctrls->debounce_delay;
	}
    }
#endif
#endif
#ifdef SPI_XKB_DEBUG
  fprintf (stderr, "BounceKeys delay: %d\n", (int) retval);
#endif
  return retval;
}

static gboolean
dec_synth_keycode_press (SpiDEController *controller,
			 unsigned int keycode)
{
	unsigned int time = CurrentTime;
	unsigned int bounce_delay;
	unsigned int elapsed_msec;
	struct timeval tv;
	DEControllerPrivateData *priv =
		(DEControllerPrivateData *) g_object_get_qdata (G_OBJECT (controller),
								spi_dec_private_quark);
	if (keycode == priv->last_release_keycode)
	{
		bounce_delay = dec_xkb_get_bouncekeys_delay (controller); 
                if (bounce_delay)
		{
			gettimeofday (&tv, NULL);
			elapsed_msec =
				(tv.tv_sec - priv->last_release_time.tv_sec) * 1000
				+ (tv.tv_usec - priv->last_release_time.tv_usec) / 1000;
#ifdef SPI_XKB_DEBUG			
			fprintf (stderr, "%d ms elapsed (%ld usec)\n", elapsed_msec,
				 (long) (tv.tv_usec - priv->last_release_time.tv_usec));
#endif
#ifdef THIS_IS_BROKEN
			if (elapsed_msec < bounce_delay)
				time = bounce_delay - elapsed_msec + 1;
#else
			time = bounce_delay + 10;
			/* fudge for broken XTest */
#endif
#ifdef SPI_XKB_DEBUG			
			fprintf (stderr, "waiting %d ms\n", time);
#endif
		}
	}
        XTestFakeKeyEvent (spi_get_display (), keycode, True, time);
	priv->last_press_keycode = keycode;
	XFlush (spi_get_display ());
	XSync (spi_get_display (), False);
	gettimeofday (&priv->last_press_time, NULL);
	return TRUE;
}

static gboolean
dec_synth_keycode_release (SpiDEController *controller,
			   unsigned int keycode)
{
	unsigned int time = CurrentTime;
	unsigned int slow_delay;
	unsigned int elapsed_msec;
	struct timeval tv;
	DEControllerPrivateData *priv =
		(DEControllerPrivateData *) g_object_get_qdata (G_OBJECT (controller),
								spi_dec_private_quark);
	if (keycode == priv->last_press_keycode)
	{
		slow_delay = dec_xkb_get_slowkeys_delay (controller);
		if (slow_delay)
		{
			gettimeofday (&tv, NULL);
			elapsed_msec =
				(tv.tv_sec - priv->last_press_time.tv_sec) * 1000
				+ (tv.tv_usec - priv->last_press_time.tv_usec) / 1000;
#ifdef SPI_XKB_DEBUG			
			fprintf (stderr, "%d ms elapsed (%ld usec)\n", elapsed_msec,
				 (long) (tv.tv_usec - priv->last_press_time.tv_usec));
#endif
#ifdef THIS_IS_BROKEN_DUNNO_WHY
			if (elapsed_msec < slow_delay)
				time = slow_delay - elapsed_msec + 1;
#else
			time = slow_delay + 10;
			/* our XTest seems broken, we have to add slop as above */
#endif
#ifdef SPI_XKB_DEBUG			
			fprintf (stderr, "waiting %d ms\n", time);
#endif
		}
	}
        XTestFakeKeyEvent (spi_get_display (), keycode, False, time);
	priv->last_release_keycode = keycode;
	XSync (spi_get_display (), False);
	gettimeofday (&priv->last_release_time, NULL);
	return TRUE;
}

static unsigned
dec_get_modifier_state (SpiDEController *controller)
{
	return mouse_mask_state;
}

static gboolean
dec_lock_modifiers (SpiDEController *controller, unsigned modifiers)
{
    DEControllerPrivateData *priv = (DEControllerPrivateData *) 
    g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);	 
    
    if (priv->have_xkb) {
        return XkbLockModifiers (spi_get_display (), XkbUseCoreKbd, 
                                  modifiers, modifiers);
    } else {
	int mod_index;
	for (mod_index=0;mod_index<8;mod_index++)
	    if (modifiers & (1<<mod_index))
	        dec_synth_keycode_press(controller, xmkeymap->modifiermap[mod_index]);
	return TRUE;
    }
}

static gboolean
dec_unlock_modifiers (SpiDEController *controller, unsigned modifiers)
{
    DEControllerPrivateData *priv = (DEControllerPrivateData *) 
    g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);	 
    
    if (priv->have_xkb) {
        return XkbLockModifiers (spi_get_display (), XkbUseCoreKbd, 
                                  modifiers, 0);
    } else {
	int mod_index;
	for (mod_index=0;mod_index<8;mod_index++)
	    if (modifiers & (1<<mod_index))
	        dec_synth_keycode_release(controller, xmkeymap->modifiermap[mod_index]);
	return TRUE;
    }
}

static KeySym
dec_keysym_for_unichar (SpiDEController *controller, gunichar unichar)
{
	return ucs2keysym ((long) unichar);
}

static gboolean
dec_synth_keysym (SpiDEController *controller, KeySym keysym)
{
	KeyCode key_synth_code;
	unsigned int modifiers, synth_mods, lock_mods;

	key_synth_code = keycode_for_keysym (controller, keysym, &synth_mods);

	if ((key_synth_code == 0) || (synth_mods == 0xFF)) return FALSE;

	/* TODO: set the modifiers accordingly! */
	modifiers = dec_get_modifier_state (controller);
	/* side-effect; we may unset mousebutton modifiers here! */

	lock_mods = 0;
	if (synth_mods != modifiers) {
		lock_mods = synth_mods & ~modifiers;
		dec_lock_modifiers (controller, lock_mods);
	}
	dec_synth_keycode_press (controller, key_synth_code);
	dec_synth_keycode_release (controller, key_synth_code);

	if (synth_mods != modifiers) 
		dec_unlock_modifiers (controller, lock_mods);
	return TRUE;
}


static gboolean
dec_synth_keystring (SpiDEController *controller, const CORBA_char *keystring)
{
	/* probably we need to create and inject an XIM handler eventually. */
	/* for now, try to match the string to existing 
	 * keycode+modifier states. 
         */
	KeySym *keysyms;
	gint maxlen = 0;
	gunichar unichar = 0;
	gint i = 0;
	gboolean retval = TRUE;
	const gchar *c;

	maxlen = strlen (keystring) + 1;
	keysyms = g_new0 (KeySym, maxlen);
	if (!(keystring && *keystring && g_utf8_validate (keystring, -1, &c))) { 
		retval = FALSE;
	} 
	else {
#ifdef SPI_DEBUG
		fprintf (stderr, "[keystring synthesis attempted on %s]\n", keystring);
#endif
		while (keystring && (unichar = g_utf8_get_char (keystring))) {
			KeySym keysym;
			char bytes[6];
			gint mbytes;
			
			mbytes = g_unichar_to_utf8 (unichar, bytes);
			bytes[mbytes] = '\0';
#ifdef SPI_DEBUG
		        fprintf (stderr, "[unichar %s]", bytes);
#endif
			keysym = dec_keysym_for_unichar (controller, unichar);
			if (keysym == NoSymbol) {
#ifdef SPI_DEBUG
				fprintf (stderr, "no keysym for %s", bytes);
#endif
				retval = FALSE;
				break;
			}
			keysyms[i++] = keysym;
			keystring = g_utf8_next_char (keystring); 
		}
		keysyms[i++] = 0;
		XSynchronize (spi_get_display (), TRUE);
		for (i = 0; keysyms[i]; ++i) {
			if (!dec_synth_keysym (controller, keysyms[i])) {
#ifdef SPI_DEBUG
				fprintf (stderr, "could not synthesize %c\n",
					 (int) keysyms[i]);
#endif
				retval = FALSE;
				break;
			}
		}
		XSynchronize (spi_get_display (), FALSE);
	}
	g_free (keysyms);

	return retval;
}


/*
 * CORBA Accessibility::DEController::registerKeystrokeListener
 *     method implementation
 */
static void
impl_generate_keyboard_event (PortableServer_Servant           servant,
			      const CORBA_long                 keycode,
			      const CORBA_char                *keystring,
			      const Accessibility_KeySynthType synth_type,
			      CORBA_Environment               *ev)
{
  SpiDEController *controller =
	SPI_DEVICE_EVENT_CONTROLLER (bonobo_object (servant));
  gint err;
  KeySym keysym;
  DEControllerPrivateData *priv;

#ifdef SPI_DEBUG
	fprintf (stderr, "synthesizing keystroke %ld, type %d\n",
		 (long) keycode, (int) synth_type);
#endif
  /* TODO: hide/wrap/remove X dependency */

  /*
   * TODO: when initializing, query for XTest extension before using,
   * and fall back to XSendEvent() if XTest is not available.
   */
  
  gdk_error_trap_push ();

  priv = (DEControllerPrivateData *) 
      g_object_get_qdata (G_OBJECT (controller), spi_dec_private_quark);

  if (!priv->have_xkb && xmkeymap==NULL) {
    xmkeymap = XGetModifierMapping(spi_get_display ());
  }

  switch (synth_type)
    {
      case Accessibility_KEY_PRESS:
	      dec_synth_keycode_press (controller, keycode);
	      break;
      case Accessibility_KEY_PRESSRELEASE:
	      dec_synth_keycode_press (controller, keycode);
      case Accessibility_KEY_RELEASE:
	      dec_synth_keycode_release (controller, keycode);
	      break;
      case Accessibility_KEY_SYM:
#ifdef SPI_XKB_DEBUG	      
	      fprintf (stderr, "KeySym synthesis\n");
#endif
	      /* 
	       * note: we are using long for 'keycode'
	       * in our arg list; it can contain either
	       * a keycode or a keysym.
	       */
	      dec_synth_keysym (controller, (KeySym) keycode);
	      break;
      case Accessibility_KEY_STRING:
	      if (!dec_synth_keystring (controller, keystring))
		      fprintf (stderr, "Keystring synthesis failure, string=%s\n",
			       keystring);
	      break;
    }
  if ((err = gdk_error_trap_pop ()))
    {
      DBG (-1, g_warning ("Error [%d] emitting keystroke", err));
    }
  if (synth_type == Accessibility_KEY_SYM) {
    keysym = keycode;
  }
  else {
    keysym = XkbKeycodeToKeysym (spi_get_display (), keycode, 0, 0);
  }
  if (XkbKeysymToModifiers (spi_get_display (), keysym) == 0) 
    {
      spi_dec_clear_unlatch_pending (controller);
    }
}

/* Accessibility::DEController::generateMouseEvent */
static void
impl_generate_mouse_event (PortableServer_Servant servant,
			   const CORBA_long       x,
			   const CORBA_long       y,
			   const CORBA_char      *eventName,
			   CORBA_Environment     *ev)
{
  int button = 0;
  gboolean error = FALSE;
  Display *display = spi_get_display ();
#ifdef SPI_DEBUG
  fprintf (stderr, "generating mouse %s event at %ld, %ld\n",
	   eventName, (long int) x, (long int) y);
#endif
  switch (eventName[0])
    {
      case 'b':
        switch (eventName[1])
	  {
	  /* TODO: check number of buttons before parsing */
	  case '1':
		    button = 1;
		    break;
	  case '2':
		  button = 2;
		  break;
	  case '3':
	          button = 3;
	          break;
	  case '4':
	          button = 4;
	          break;
	  case '5':
	          button = 5;
	          break;
	  default:
		  error = TRUE;
	  }
	if (!error)
	  {
	    if (x != -1 && y != -1)
	      {
	        XTestFakeMotionEvent (display, DefaultScreen (display),
				      x, y, 0);
	      }
	    XTestFakeButtonEvent (display, button, !(eventName[2] == 'r'), 0);
	    if (eventName[2] == 'c')
	      XTestFakeButtonEvent (display, button, FALSE, 1);
	    else if (eventName[2] == 'd')
	      {
	      XTestFakeButtonEvent (display, button, FALSE, 1);
	      XTestFakeButtonEvent (display, button, TRUE, 2);
	      XTestFakeButtonEvent (display, button, FALSE, 3);
	      }
	  }
	break;
      case 'r': /* relative motion */ 
	XTestFakeRelativeMotionEvent (display, x, y, 0);
        break;
      case 'a': /* absolute motion */
	XTestFakeMotionEvent (display, DefaultScreen (display),
			      x, y, 0);
        break;
    }
}

/* Accessibility::DEController::notifyListenersSync */
static CORBA_boolean
impl_notify_listeners_sync (PortableServer_Servant           servant,
			    const Accessibility_DeviceEvent *event,
			    CORBA_Environment               *ev)
{
  SpiDEController *controller = SPI_DEVICE_EVENT_CONTROLLER (
    bonobo_object_from_servant (servant));
#ifdef SPI_DEBUG
  g_print ("notifylistening listeners synchronously: controller %p, event id %d\n",
	   controller, (int) event->id);
#endif
  return spi_controller_notify_keylisteners (controller,
					     (Accessibility_DeviceEvent *) 
					     event, CORBA_FALSE, ev) ?
	  CORBA_TRUE : CORBA_FALSE; 
}

/* Accessibility::DEController::notifyListenersAsync */
static void
impl_notify_listeners_async (PortableServer_Servant           servant,
			     const Accessibility_DeviceEvent *event,
			     CORBA_Environment               *ev)
{
  SpiDEController *controller = SPI_DEVICE_EVENT_CONTROLLER (
    bonobo_object_from_servant (servant));
#ifdef SPI_DEBUG
  fprintf (stderr, "notifying listeners asynchronously\n");
#endif
  spi_controller_notify_keylisteners (controller, (Accessibility_DeviceEvent *)
				      event, CORBA_FALSE, ev); 
}

static void
spi_device_event_controller_class_init (SpiDEControllerClass *klass)
{
  GObjectClass * object_class = (GObjectClass *) klass;
  POA_Accessibility_DeviceEventController__epv *epv = &klass->epv;

  spi_device_event_controller_parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = spi_device_event_controller_object_finalize;
	
  epv->registerKeystrokeListener   = impl_register_keystroke_listener;
  epv->deregisterKeystrokeListener = impl_deregister_keystroke_listener;
  epv->registerDeviceEventListener = impl_register_device_listener;
  epv->deregisterDeviceEventListener = impl_deregister_device_listener;
  epv->generateKeyboardEvent       = impl_generate_keyboard_event;
  epv->generateMouseEvent          = impl_generate_mouse_event;
  epv->notifyListenersSync         = impl_notify_listeners_sync;
  epv->notifyListenersAsync        = impl_notify_listeners_async;

  if (!spi_dec_private_quark)
	  spi_dec_private_quark = g_quark_from_static_string ("spi-dec-private");
}

#ifdef HAVE_XEVIE
static Bool isEvent(Display *dpy, XEvent *event, char *arg)
{
   return TRUE;
}

static gboolean
handle_io (GIOChannel *source,
           GIOCondition condition,
           gpointer data) 
{
  SpiDEController *controller = (SpiDEController *) data;
  gboolean is_consumed = FALSE;
  XEvent ev;

  while (XCheckIfEvent(controller->xevie_display, &ev, isEvent, NULL))
    {
      if (ev.type == KeyPress || ev.type == KeyRelease)
        is_consumed = spi_device_event_controller_forward_key_event (controller, &ev);

      if (! is_consumed)
        XevieSendEvent(controller->xevie_display, &ev, XEVIE_UNMODIFIED);
    }

  return TRUE;
}
#endif /* HAVE_XEVIE */

static void
spi_device_event_controller_init (SpiDEController *device_event_controller)
{
#ifdef HAVE_XEVIE
  GIOChannel *ioc;
  int fd;
#endif /* HAVE_XEVIE */

  DEControllerPrivateData *private;	
  device_event_controller->key_listeners   = NULL;
  device_event_controller->mouse_listeners = NULL;
  device_event_controller->keygrabs_list   = NULL;
  device_event_controller->xevie_display   = NULL;

#ifdef HAVE_XEVIE
  device_event_controller->xevie_display = XOpenDisplay(NULL);

  if (XevieStart(device_event_controller->xevie_display) == TRUE)
    {
#ifdef SPI_KEYEVENT_DEBUG
      fprintf (stderr, "XevieStart() success \n");
#endif
      XevieSelectInput(device_event_controller->xevie_display, KeyPressMask | KeyReleaseMask);

      fd = ConnectionNumber(device_event_controller->xevie_display);
      ioc = g_io_channel_unix_new (fd);
      g_io_add_watch (ioc, G_IO_IN | G_IO_HUP, handle_io, device_event_controller);
      g_io_channel_unref (ioc);
    }
  else
    {
      device_event_controller->xevie_display = NULL;
#ifdef SPI_KEYEVENT_DEBUG
      fprintf (stderr, "XevieStart() failed, only one client is allowed to do event int exception\n");
#endif
    }
#endif /* HAVE_XEVIE */

  private = g_new0 (DEControllerPrivateData, 1);
  gettimeofday (&private->last_press_time, NULL);
  gettimeofday (&private->last_release_time, NULL);
  g_object_set_qdata (G_OBJECT (device_event_controller),
		      spi_dec_private_quark,
		      private);
  spi_controller_register_with_devices (device_event_controller);
}

static gboolean
spi_device_event_controller_forward_key_event (SpiDEController *controller,
					       const XEvent    *event)
{
  CORBA_Environment ev;
  Accessibility_DeviceEvent key_event;
  gboolean ret;

  g_assert (event->type == KeyPress || event->type == KeyRelease);

  CORBA_exception_init (&ev);

  key_event = spi_keystroke_from_x_key_event ((XKeyEvent *) event);

  if (controller->xevie_display == NULL)
    spi_controller_update_key_grabs (controller, &key_event);

  /* relay to listeners, and decide whether to consume it or not */
  ret = spi_controller_notify_keylisteners (controller, &key_event, CORBA_TRUE, &ev);
  CORBA_free(key_event.event_string);
  return ret;
}

SpiDEController *
spi_device_event_controller_new (SpiRegistry *registry)
{
  SpiDEController *retval = g_object_new (
    SPI_DEVICE_EVENT_CONTROLLER_TYPE, NULL);
  
  retval->registry = SPI_REGISTRY (bonobo_object_ref (
	  BONOBO_OBJECT (registry)));

  spi_dec_init_mouse_listener (registry);
  /* TODO: kill mouse listener on finalize */  
  return retval;
}
void
spi_device_event_controller_start_poll_mouse (SpiRegistry *registry)
{
  if (!have_mouse_event_listener)
    {
      have_mouse_event_listener = TRUE;
      if (!have_mouse_listener)
        g_timeout_add (100, spi_dec_poll_mouse_idle, registry);
    }
}

void
spi_device_event_controller_stop_poll_mouse (void)
{
  have_mouse_event_listener = FALSE;
}

static gboolean
is_key_released (KeyCode code)
{
  char keys[32];
  int down;

  XQueryKeymap (spi_get_display (), keys);
  down = BIT (keys, code);
  return (down == 0);
}

static gboolean
check_release (gpointer data)
{
  gboolean released;
  Accessibility_DeviceEvent *event = (Accessibility_DeviceEvent *)data;
  KeyCode code = event->hw_code;
  CORBA_Environment ev;

  released = is_key_released (code);

  if (released)
    {
      check_release_handler = 0;
      event->type = Accessibility_KEY_RELEASED_EVENT;
      ev._major = CORBA_NO_EXCEPTION;
      spi_controller_notify_keylisteners (saved_controller, event, CORBA_TRUE, &ev);
    }
  return (released == 0);
}

static void wait_for_release_event (XEvent          *event,
                                    SpiDEController *controller)
{
  pressed_event = spi_keystroke_from_x_key_event ((XKeyEvent *) event);
  saved_controller = controller;
  check_release_handler = g_timeout_add (CHECK_RELEASE_DELAY, check_release, &pressed_event);
}

BONOBO_TYPE_FUNC_FULL (SpiDEController,
		       Accessibility_DeviceEventController,
		       PARENT_TYPE,
		       spi_device_event_controller)
