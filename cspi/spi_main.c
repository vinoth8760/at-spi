/*
 *
 * Basic SPI initialization and event loop function prototypes
 *
 */
#include <string.h>
#include <stdlib.h>
#include <cspi/spi-private.h>

#undef DEBUG_OBJECTS

static CORBA_Environment ev = { 0 };
static Accessibility_Registry registry = CORBA_OBJECT_NIL;
static SPIBoolean is_gnome_app = FALSE;
static GHashTable *live_refs = NULL;

static guint
spi_object_hash (gconstpointer key)
{
  CORBA_Object object = (CORBA_Object) key;
  guint        retval;
  
  retval = CORBA_Object_hash (object, G_MAXINT, &ev);

  return retval;
}

static gboolean
spi_object_equal (gconstpointer a, gconstpointer b)
{
  CORBA_Object objecta = (CORBA_Object) a;
  CORBA_Object objectb = (CORBA_Object) b;
  gboolean     retval;

  retval = CORBA_Object_is_equivalent (objecta, objectb, &ev);

  return retval;
}

static void
spi_object_release (gpointer  value)
{
  Accessible *a = (Accessible *) value;

#ifdef DEBUG_OBJECTS
  g_print ("releasing %p => %p\n", a, a->objref);
#endif

  bonobo_object_release_unref (a->objref, NULL);

  memset (a, 0xaa, sizeof (Accessible));

  g_free (a);
}

static GHashTable *
get_live_refs (void)
{
  if (!live_refs) 
    {
      live_refs = g_hash_table_new_full (spi_object_hash,
					 spi_object_equal,
					 NULL,
					 spi_object_release);
    }
  return live_refs;
}

CORBA_Environment *
cspi_ev (void)
{
  /* This method is an ugly hack */
  return &ev;
}

Accessibility_Registry
cspi_registry (void)
{
  return registry;
}

SPIBoolean
cspi_is_gnome_app (void)
{
  return is_gnome_app;	
}

SPIBoolean
cspi_exception (void)
{
  SPIBoolean retval;

  if (BONOBO_EX (&ev))
    {
      CORBA_exception_free (&ev);
      retval = TRUE;
    }
  else
    {
      retval = FALSE;
    }

  return retval;
}

Accessible *
cspi_object_add (CORBA_Object corba_object)
{
  Accessible *ref;

  if (corba_object == CORBA_OBJECT_NIL)
    {
      ref = NULL;
    }
  else
    {
      if ((ref = g_hash_table_lookup (get_live_refs (), corba_object)))
        {
          bonobo_object_release_unref (corba_object, NULL);
	  ref->ref_count++;
	}
      else
        {
          ref = g_new (Accessible, 1);

#ifdef DEBUG_OBJECTS
          g_print ("allocating %p => %p\n", ref, corba_object);
#endif

          ref->objref = corba_object;
          ref->ref_count = 1;

          g_hash_table_insert (get_live_refs (), ref->objref, ref);
	}
    }

  return ref;
}

Accessible *
cspi_object_add_check (CORBA_Object corba_object)
{
  Accessible *retval;

  cspi_check_ev (cspi_ev (), "pre method check");

  retval = cspi_object_add (corba_object);

  cspi_check_ev (cspi_ev (), "post method check");

  return retval;
}

void
cspi_object_ref (Accessible *accessible)
{
  g_return_if_fail (accessible != NULL);

  accessible->ref_count++;
}

void
cspi_object_unref (Accessible *accessible)
{
  if (accessible == NULL)
    {
      return;
    }

  if (--accessible->ref_count == 0)
    {
      g_hash_table_remove (get_live_refs (), accessible->objref);
    }
}

static void
cspi_cleanup (void)
{
  GHashTable *refs;

  refs = live_refs;
  live_refs = NULL;
  g_hash_table_destroy (refs);

  if (registry != CORBA_OBJECT_NIL)
    {
      bonobo_object_release_unref (registry, NULL);
      registry = CORBA_OBJECT_NIL;
    }
}

static gboolean SPI_inited = FALSE;

/**
 * SPI_init:
 *
 * Connects to the accessibility registry and initializes the SPI.
 *
 * Returns: 0 on success, otherwise an integer error code.
 **/
int
SPI_init (void)
{
  int argc = 0;
  char *obj_id;

  if (SPI_inited)
    {
      return 1;
    }

  SPI_inited = TRUE;

  CORBA_exception_init (&ev);

  if (!bonobo_init (&argc, NULL))
    {
      g_error ("Could not initialize Bonobo");
    }

  obj_id = "OAFIID:Accessibility_Registry:proto0.1";

  registry = bonobo_activation_activate_from_id (
	  obj_id, 0, NULL, cspi_ev ());

  if (ev._major != CORBA_NO_EXCEPTION)
    {
      g_error ("AT-SPI error: during registry activation: %s\n",
	       bonobo_exception_get_text (cspi_ev ()));
    }

  if (registry == CORBA_OBJECT_NIL)
    {
      g_error ("Could not locate registry");
    }

  bonobo_activate ();
  
  return 0;
}

/**
 * SPI_event_main:
 * @isGNOMEApp: a #SPIBoolean indicating whether the client of the SPI
 *              will use the Gnome event loop or not.  Clients that have
 *              their own GUIS will usually specify #TRUE here, and must
 *              do so if they use Gnome GUI components.
 *
 * Starts/enters the main event loop for the SPI services.
 *
 * (NOTE: This method does not return control, it is exited via a call
 * to SPI_exit() from within an event handler).
 *
 **/
void
SPI_event_main (SPIBoolean isGNOMEApp)
{
  is_gnome_app = isGNOMEApp;
  if (isGNOMEApp)
    {
      g_atexit (cspi_cleanup);
      bonobo_main ();
    }
  else
    {
      /* TODO: install signal handlers to do cleanup */
      CORBA_ORB_run (bonobo_orb (), cspi_ev ());
      fprintf (stderr, "orb loop exited...\n");
    }
}

/**
 * SPI_eventIsReady:
 *
 * Checks to see if an SPI event is waiting in the event queue.
 * Used by clients that don't wish to use SPI_event_main().
 *
 * Not Yet Implemented.
 *
 * Returns: #TRUE if an event is waiting, otherwise #FALSE.
 *
 **/
SPIBoolean
SPI_eventIsReady ()
{
  return FALSE;
}

/**
 * SPI_nextEvent:
 * @waitForEvent: a #SPIBoolean indicating whether to block or not.
 *
 * Gets the next event in the SPI event queue; blocks if no event
 * is pending and @waitForEvent is #TRUE.
 * Used by clients that don't wish to use SPI_event_main().
 *
 * Not Yet Implemented.
 *
 * Returns: the next #AccessibleEvent in the SPI event queue.
 **/
AccessibleEvent *
SPI_nextEvent (SPIBoolean waitForEvent)
{
  return NULL;
}

/**
 * SPI_exit:
 *
 * Disconnects from the Accessibility Registry and releases 
 * any floating resources.
 **/
void
SPI_exit (void)
{
  if (!SPI_inited)
    {
      return;
    }

  SPI_inited = FALSE;

  cspi_cleanup ();

  if (cspi_is_gnome_app ())
    {
      bonobo_main_quit ();
    }
  else
    {
      CORBA_ORB_shutdown (bonobo_orb (), TRUE, cspi_ev ());	    
    }
  fprintf (stderr, "bye-bye!\n");
}

