/* vim:set et sts=4: */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ibus.h>
#include <locale.h>
#include <m17n.h>
#include "engine.h"
#include "m17nutil.h"

static IBusBus *bus = NULL;
static IBusFactory *factory = NULL;

/* options */
static gboolean xml = FALSE;
static gboolean ibus = FALSE;
static gboolean verbose = FALSE;

static const GOptionEntry entries[] =
{
    { "xml", 'x', 0, G_OPTION_ARG_NONE, &xml, "generate xml for engines", NULL },
    { "ibus", 'i', 0, G_OPTION_ARG_NONE, &ibus, "component is executed by ibus", NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "verbose", NULL },
    { NULL },
};


static void
ibus_disconnected_cb (IBusBus  *bus,
                      gpointer  user_data)
{
    g_debug ("bus disconnected");
    ibus_quit ();
}


static void
start_component (void)
{
    GList *engines, *p;
    IBusComponent *component;

    ibus_init ();

    bus = ibus_bus_new ();
    g_signal_connect (bus, "disconnected", G_CALLBACK (ibus_disconnected_cb), NULL);
    ibus_m17n_init (bus);

    component = ibus_m17n_get_component ();

    factory = ibus_factory_new (ibus_bus_get_connection (bus));

    engines = ibus_component_get_engines (component);
    for (p = engines; p != NULL; p = p->next) {
        IBusEngineDesc *engine = (IBusEngineDesc *)p->data;
#if IBUS_CHECK_VERSION(1,3,99)
        const gchar *engine_name = ibus_engine_desc_get_name (engine);
#else
        const gchar *engine_name = engine->name;
#endif  /* !IBUS_CHECK_VERSION(1,3,99) */
        GType type = ibus_m17n_engine_get_type_for_name (engine_name);

        if (type == G_TYPE_INVALID) {
            g_debug ("Can not create engine type for %s", engine_name);
            continue;
        }
        ibus_factory_add_engine (factory, engine_name, type);
    }

    if (ibus) {
        ibus_bus_request_name (bus, "org.freedesktop.IBus.M17N", 0);
    }
    else {
        ibus_bus_register_component (bus, component);
    }

    g_object_unref (component);

    ibus_main ();
}

static void
print_engines_xml (void)
{
    IBusComponent *component;
    GString *output;

    ibus_init ();

    ibus_m17n_init_common ();

    component = ibus_m17n_get_component ();
    output = g_string_new ("");

    ibus_component_output_engines (component, output, 0);

    fprintf (stdout, "%s", output->str);

    g_string_free (output, TRUE);

}

int
main (gint argc, gchar **argv)
{
    GError *error = NULL;
    GOptionContext *context;

    setlocale (LC_ALL, "");

    context = g_option_context_new ("- ibus M17N engine component");

    g_option_context_add_main_entries (context, entries, "ibus-m17n");

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print ("Option parsing failed: %s\n", error->message);
        exit (-1);
    }

    if (xml) {
        print_engines_xml ();
        exit (0);
    }

    start_component ();
    return 0;
}
