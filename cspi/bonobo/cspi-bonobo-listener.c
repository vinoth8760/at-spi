#include <libbonobo.h>
#include "../spi-private.h"
#include "cspi-bonobo-listener.h"

typedef struct
{
  union
    {
      AccessibleEventListenerCB     event;
      AccessibleKeystrokeListenerCB key_event;
      gpointer                      method;
    } cb;
  gpointer user_data;
} EventHandler;

GObjectClass *event_parent_class;
GObjectClass *keystroke_parent_class;

/*
 * Misc. helpers.
 */

static EventHandler *
event_handler_new (gpointer method, gpointer user_data)
{
  EventHandler *eh = g_new0 (EventHandler, 1);

  eh->cb.method = method;
  eh->user_data = user_data;

  return eh;
}

static void
event_handler_free (EventHandler *handler)
{
  g_free (handler);
}

static GList *
event_list_remove_by_cb (GList *list, gpointer callback)
{
  GList *l, *next;
	
  for (l = list; l; l = next)
    {
      EventHandler *eh = l->data;
      next = l->next;
      
      list = g_list_delete_link (list, l);
      
      event_handler_free (eh);
    }

  return list;
}

/*
 * Standard event dispatcher
 */

BONOBO_CLASS_BOILERPLATE (CSpiEventListener, cspi_event_listener,
			  GObject, spi_event_listener_get_type ())

static void
cspi_event (SpiEventListener    *listener,
	    Accessibility_Event *event)
{
  GList *l;
  CSpiEventListener *clistener = (CSpiEventListener *) listener;
  AccessibleEvent    aevent;
  Accessible        *source;

  source = cspi_object_add (bonobo_object_dup_ref (event->source, cspi_ev ()));

  aevent.type    = event->type;
  aevent.source  = source;
  aevent.detail1 = event->detail1;
  aevent.detail2 = event->detail2;

  /* FIXME: re-enterancy hazard on this list */
  for (l = clistener->callbacks; l; l = l->next)
    {
      EventHandler *eh = l->data;

      eh->cb.event (&aevent, eh->user_data);
    }

  cspi_object_unref (source);
}

static void
cspi_event_listener_instance_init (CSpiEventListener *listener)
{
}

static void
cspi_event_listener_finalize (GObject *object)
{
  CSpiEventListener *listener = (CSpiEventListener *) object;
  GList *l;
  
  for (l = listener->callbacks; l; l = l->next)
    {
      event_handler_free (l->data);
    }
  
  g_list_free (listener->callbacks);

  event_parent_class->finalize (object);
}

static void
cspi_event_listener_class_init (CSpiEventListenerClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  event_parent_class = g_type_class_peek_parent (klass);
  object_class->finalize = cspi_event_listener_finalize;

  klass->event = cspi_event;
}

CORBA_Object
cspi_event_listener_new (void)
{
  CSpiEventListener *listener;

  listener = g_object_new (cspi_event_listener_get_type (), NULL);

  return CORBA_Object_duplicate (BONOBO_OBJREF (listener), cspi_ev ());
}

void
cspi_event_listener_add_cb (AccessibleEventListener  *al,
			    AccessibleEventListenerCB callback,
			    void                     *user_data)
{
  CSpiEventListener *listener = bonobo_object (
    ORBit_small_get_servant (CSPI_OBJREF (al)));

  g_return_if_fail (CSPI_IS_EVENT_LISTENER (listener));

  listener->callbacks = g_list_prepend (listener->callbacks,
					event_handler_new (callback, user_data));
}

void
cspi_event_listener_remove_cb (AccessibleEventListener  *al,
			       AccessibleEventListenerCB callback)
{
  CSpiEventListener *listener = bonobo_object (
    ORBit_small_get_servant (CSPI_OBJREF (al)));

  g_return_if_fail (CSPI_IS_EVENT_LISTENER (listener));

  listener->callbacks = event_list_remove_by_cb (listener->callbacks, callback);
}

/*
 * Key event dispatcher
 */

static gboolean
cspi_key_event (SpiKeystrokeListener          *listener,
		const Accessibility_KeyStroke *keystroke)
{
  GList *l;
  CSpiKeystrokeListener *clistener = (CSpiKeystrokeListener *) listener;
  AccessibleKeystroke akeystroke;
  gboolean handled = FALSE;

#ifdef SPI_KEYEVENT_DEBUG
  fprintf (stderr, "%s%c",
	   (keystroke->modifiers & SPI_KEYMASK_ALT)?"Alt-":"",
	   ((keystroke->modifiers & SPI_KEYMASK_SHIFT)^(keystroke->modifiers & SPI_KEYMASK_SHIFTLOCK))?
	   (char) toupper((int) keystroke->keyID) : (char) tolower((int) keystroke->keyID));
  
  fprintf (stderr, "Key:\tsym %ld\n\tmods %x\n\tcode %d\n\ttime %ld\n",
	   (long) keystroke->keyID,
	   (unsigned int) keystroke->modifiers,
	   (int) keystroke->keycode,
	   (long int) keystroke->timestamp);
#endif

  switch (keystroke->type)
    {
      case Accessibility_KEY_PRESSED:
	akeystroke.type = SPI_KEY_PRESSED;
	break;
      case Accessibility_KEY_RELEASED:
	akeystroke.type = SPI_KEY_RELEASED;
	break;
      default:
	akeystroke.type = 0;
	break;
    }
  akeystroke.keyID     = keystroke->keyID;
  akeystroke.keycode   = keystroke->keycode;
  akeystroke.timestamp = keystroke->timestamp;
  akeystroke.modifiers = keystroke->modifiers;

  /* FIXME: re-enterancy hazard on this list */
  for (l = clistener->callbacks; l; l = l->next)
    {
      EventHandler *eh = l->data;

      if ((handled = eh->cb.key_event (&akeystroke, eh->user_data)))
        {
	  break;
	}
    }
  
  return handled;
}

static void
cspi_keystroke_listener_init (CSpiKeystrokeListener *listener)
{
}


static void
cspi_keystroke_listener_finalize (GObject *object)
{
  CSpiKeystrokeListener *listener = (CSpiKeystrokeListener *) object;
  GList *l;
  
  for (l = listener->callbacks; l; l = l->next)
    {
      event_handler_free (l->data);
    }
  
  g_list_free (listener->callbacks);

  keystroke_parent_class->finalize (object);
}

static void
cspi_keystroke_listener_class_init (CSpiKeystrokeListenerClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  keystroke_parent_class = g_type_class_peek_parent (klass);
  object_class->finalize = cspi_keystroke_listener_finalize;

  klass->key_event = cspi_key_event;
}

BONOBO_TYPE_FUNC (CSpiKeystrokeListener, 
		  spi_keystroke_listener_get_type (),
		  cspi_keystroke_listener);

CORBA_Object
cspi_keystroke_listener_new (void)
{
  CSpiEventListener *listener;

  listener = g_object_new (cspi_keystroke_listener_get_type (), NULL);

  return CORBA_Object_duplicate (BONOBO_OBJREF (listener), cspi_ev ());
}

void
cspi_keystroke_listener_add_cb (AccessibleKeystrokeListener  *al,
				AccessibleKeystrokeListenerCB callback,
				void                         *user_data)
{
  CSpiKeystrokeListener *listener = bonobo_object (
    ORBit_small_get_servant (CSPI_OBJREF (al)));

  g_return_if_fail (CSPI_IS_KEYSTROKE_LISTENER (listener));

  listener->callbacks = g_list_prepend (listener->callbacks,
					event_handler_new (callback, user_data));
}

void
cspi_keystroke_listener_remove_cb (AccessibleKeystrokeListener  *al,
				   AccessibleKeystrokeListenerCB callback)
{
  CSpiKeystrokeListener *listener = bonobo_object (
    ORBit_small_get_servant (CSPI_OBJREF (al)));

  g_return_if_fail (CSPI_IS_KEYSTROKE_LISTENER (listener));

  listener->callbacks = event_list_remove_by_cb (listener->callbacks, callback);
}
