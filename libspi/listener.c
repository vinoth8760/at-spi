/*
 * listener.c: test for accessibility implementation
 *
 */
#define DEBUG_PRINTSTUFF
#ifdef DEBUG_PRINTSTUFF
#include <stdio.h>
#endif
#include <config.h>
#include <bonobo/Bonobo.h>
#include <Accessible.h>

/*
 * This pulls the definition for the BonoboObject (GType)
 */
#include "listener.h"

/*
 * Our parent Gtk object type
 */
#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

/*
 * A pointer to our parent object class
 */
static GObjectClass *listener_parent_class;

/*
 * Implemented GObject::finalize
 */
static void
listener_object_finalize (GObject *object)
{
/*        Listener *listener = LISTENER (object); */

        printf("listener_object_finalize called\n");

        listener_parent_class->finalize (object);
}

/*
 * CORBA Accessibility::Listener::notifyEvent method implementation
 */

static void
impl_notify_event (PortableServer_Servant     servant,
                   const Accessibility_Event *e,
                   CORBA_Environment         *ev)
{
  fprintf (stderr, "notify...\n");
  fprintf (stderr, "source name: '%s'\n",
	   Accessibility_Accessible__get_name(e->target, ev));
}

static void
listener_class_init (ListenerClass *klass)
{
        GObjectClass * object_class = (GObjectClass *) klass;
        POA_Accessibility_EventListener__epv *epv = &klass->epv;
        listener_parent_class = g_type_class_ref (BONOBO_X_OBJECT_TYPE);

        object_class->finalize = listener_object_finalize;

        epv->notifyEvent = impl_notify_event;
}

static void
listener_init (Listener *listener)
{
}

GType
listener_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static const GTypeInfo tinfo = {
                        sizeof (ListenerClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) listener_class_init,
                        (GClassFinalizeFunc) NULL,
                        NULL, /* class data */
                        sizeof (Listener),
                        0, /* n preallocs */
                        (GInstanceInitFunc) listener_init,
                        NULL /* value table */
                };
                /*
                 *   Here we use bonobo_x_type_unique instead of
                 * gtk_type_unique, this auto-generates a load of
                 * CORBA structures for us. All derived types must
                 * use bonobo_x_type_unique.
                 */
                type = bonobo_x_type_unique (
                        PARENT_TYPE,
                        POA_Accessibility_EventListener__init,
                        NULL,
                        G_STRUCT_OFFSET (ListenerClass, epv),
                        &tinfo,
                        "Listener");
        }

        return type;
}

Listener *
listener_new (void)
{
    Listener *retval =
               LISTENER (g_object_new (listener_get_type (), NULL));
    return retval;
}
