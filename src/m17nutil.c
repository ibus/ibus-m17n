/* vim:set et sts=4: */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <errno.h>
#include "m17nutil.h"

#define N_(text) text

static MConverter *utf8_converter = NULL;

#define DEFAULT_XML (SETUPDIR "/default.xml")

struct _IBusM17NEngineConfigNode {
    gchar *name;
    IBusM17NEngineConfig config;
};

typedef struct _IBusM17NEngineConfigNode IBusM17NEngineConfigNode;

static GSList *config_list = NULL;

void
ibus_m17n_init_common (void)
{
    M17N_INIT ();

    if (utf8_converter == NULL) {
        utf8_converter = mconv_buffer_converter (Mcoding_utf_8, NULL, 0);
    }
}

gchar *
ibus_m17n_mtext_to_utf8 (MText *text)
{
    gint bufsize;
    gchar *buf;

    if (text == NULL)
        return NULL;

    mconv_reset_converter (utf8_converter);

    bufsize = (mtext_len (text) + 1) * 6;
    buf = (gchar *) g_malloc (bufsize);

    mconv_rebind_buffer (utf8_converter, buf, bufsize);
    mconv_encode (utf8_converter, text);

    buf [utf8_converter->nbytes] = 0;

    return buf;
}

gunichar *
ibus_m17n_mtext_to_ucs4 (MText *text, glong *nchars)
{
    glong bufsize;
    gchar *buf;
    gunichar *ucs;

    if (text == NULL)
        return NULL;

    mconv_reset_converter (utf8_converter);

    bufsize = (mtext_len (text) + 1) * 6;
    buf = (gchar *) g_malloc (bufsize);

    mconv_rebind_buffer (utf8_converter, buf, bufsize);
    if (mconv_encode (utf8_converter, text) < 0) {
        g_free (buf);
        return NULL;
    }
    ucs = g_utf8_to_ucs4_fast (buf, bufsize, nchars);
    g_free (buf);
    return ucs;
}

guint
ibus_m17n_parse_color (const gchar *hex)
{
    guint color;

    if (!hex || *hex != '#')
        return (guint)-1;

    errno = 0;
    color = strtoul (&hex[1], NULL, 16);
    if ((errno == ERANGE && color == ULONG_MAX)
        || (errno != 0 && color == 0))
        return (guint)-1;
    return color;
}

static IBusEngineDesc *
ibus_m17n_engine_new (MSymbol  lang,
                      MSymbol  name,
                      MText   *title,
                      MText   *icon,
                      MText   *desc,
                      IBusM17NEngineConfig *config)
{
    IBusEngineDesc *engine;
    gchar *engine_name;
    gchar *engine_longname;
    gchar *engine_title;
    gchar *engine_icon;
    gchar *engine_desc;

    engine_name = g_strdup_printf ("m17n:%s:%s", msymbol_name (lang), msymbol_name (name));

    engine_longname = g_strdup_printf ("%s (m17n)", msymbol_name (name));
    engine_title = ibus_m17n_mtext_to_utf8 (title);
    engine_icon = ibus_m17n_mtext_to_utf8 (icon);
    engine_desc = ibus_m17n_mtext_to_utf8 (desc);

#if IBUS_CHECK_VERSION(1,3,99)
    engine = ibus_engine_desc_new_varargs ("name",        engine_name,
                                           "longname",    engine_longname,
                                           "description", engine_desc ? engine_desc : "",
                                           "language",    msymbol_name (lang),
                                           "license",     "GPL",
                                           "icon",        engine_icon ? engine_icon : "",
                                           "layout",      "us",
                                           "rank",        config->rank,
                                           NULL);
#else
    engine = ibus_engine_desc_new (engine_name,
                                   engine_longname,
                                   engine_desc ? engine_desc : "",
                                   msymbol_name (lang),
                                   "GPL",
                                   "",
                                   engine_icon ? engine_icon : "",
                                   "us");
    engine->rank = config->rank;
#endif  /* !IBUS_CHECK_VERSION(1,3,99) */

    g_free (engine_name);
    g_free (engine_longname);
    g_free (engine_title);
    g_free (engine_icon);
    g_free (engine_desc);

    return engine;
}

#ifndef HAVE_MINPUT_LIST
MPlist *minput_list (MSymbol language);
#endif  /* !HAVE_MINPUT_LIST */

GList *
ibus_m17n_list_engines (void)
{
    GList *engines = NULL;
    MPlist *imlist;
    MPlist *elm;

    imlist = minput_list (Mnil);
    for (elm = imlist; elm && mplist_key(elm) != Mnil; elm = mplist_next(elm)) {
        MSymbol lang;
        MSymbol name;
        MSymbol sane;
        MText *title = NULL;
        MText *icon = NULL;
        MText *desc = NULL;
        MPlist *l;
        gchar *engine_name;
        IBusM17NEngineConfig *config;

        l = mplist_value (elm);
        lang = mplist_value (l);
        l = mplist_next (l);
        name = mplist_value (l);
        l = mplist_next (l);
        sane = mplist_value (l);

        if (sane == Mt) {
            /* ignore input-method explicitly blacklisted in default.xml */
            engine_name = g_strdup_printf ("m17n:%s:%s", msymbol_name (lang), msymbol_name (name));
            config = ibus_m17n_get_engine_config (engine_name);
            if (config == NULL) {
                g_warning ("can't load config for %s", engine_name);
                g_free (engine_name);
                continue;
            }
            if (config->rank < 0) {
                g_warning ("skipped %s since its rank is lower than 0",
                           engine_name);
                g_free (engine_name);
                continue;
            }
            g_free (engine_name);

            l = minput_get_variable (lang, name, msymbol ("candidates-charset"));
            if (l) {
                /* check candidates encoding */
                MPlist *sl;
                MSymbol varname;
                MText *vardesc;
                MSymbol varunknown;
                MSymbol varcharset;

                sl = mplist_value (l);
                varname  = mplist_value (sl);
                sl = mplist_next (sl);
                vardesc = mplist_value (sl);
                sl = mplist_next (sl);
                varunknown = mplist_value (sl);
                sl = mplist_next (sl);
                varcharset = mplist_value (sl);

                if (varcharset != Mcoding_utf_8 ||
                    varcharset != Mcoding_utf_8_full) {
                    /*
                    g_debug ("%s != %s or %s",
                                msymbol_name (varcharset),
                                msymbol_name (Mcoding_utf_8),
                                msymbol_name (Mcoding_utf_8_full));
                    */
                    continue;
                }

            }
            if (l)
                m17n_object_unref (l);

            desc = minput_get_description (lang, name);
            l = minput_get_title_icon (lang, name);
            if (l && mplist_key (l) == Mtext) {
                title = mplist_value (l);
            }

            MPlist *n = mplist_next (l);
            if (n && mplist_key (n) == Mtext) {
                icon = mplist_value (n);
            }

            engines = g_list_append (engines, ibus_m17n_engine_new (lang, name, title, icon, desc, config));

            if (desc)
                m17n_object_unref (desc);
            m17n_object_unref (l);
        }
    }

    if (imlist) {
        m17n_object_unref (imlist);
    }

    return engines;
}

IBusM17NEngineConfig *
ibus_m17n_get_engine_config (const gchar *engine_name)
{
    GSList *p;

    for (p = config_list; p != NULL; p = p->next) {
        IBusM17NEngineConfigNode *cnode = p->data;

        if (g_pattern_match_simple (cnode->name, engine_name))
            return &cnode->config;
    }
    g_return_val_if_reached (NULL);
}

static gboolean
ibus_m17n_engine_config_parse_xml_node (IBusM17NEngineConfigNode *cnode,
                                        XMLNode                  *node)
{
    GList *p;

    for (p = node->sub_nodes; p != NULL; p = p->next) {
        XMLNode *sub_node = (XMLNode *) p->data;

        if (g_strcmp0 (sub_node->name, "name") == 0) {
            g_free (cnode->name);
            cnode->name = g_strdup (sub_node->text);
            continue;
        }
        if (g_strcmp0 (sub_node->name , "rank") == 0) {
            cnode->config.rank = atoi (sub_node->text);
            continue;
        }
        if (g_strcmp0 (sub_node->name , "preedit-highlight") == 0) {
            if (g_ascii_strcasecmp ("TRUE", sub_node->text) == 0)
                cnode->config.preedit_highlight = TRUE;
            else if (g_ascii_strcasecmp ("FALSE", sub_node->text) != 0)
                g_warning ("<%s> element contains invalid boolean value %s",
                           sub_node->name, sub_node->text);
            continue;
        }
        g_warning ("<engine> element contains invalid element <%s>",
                   sub_node->name);
    }
    return TRUE;
}

IBusComponent *
ibus_m17n_get_component (void)
{
    GList *engines, *p;
    IBusComponent *component;
    XMLNode *node;

    component = ibus_component_new ("org.freedesktop.IBus.M17n",
                                    N_("M17N"),
                                    "0.1.0",
                                    "GPL",
                                    "Peng Huang <shawn.p.huang@gmail.com>",
                                    "http://code.google.com/p/ibus/",
                                    "",
                                    "ibus-m17n");

    node = ibus_xml_parse_file (DEFAULT_XML);
    if (node && g_strcmp0 (node->name, "engines") == 0) {
        for (p = node->sub_nodes; p != NULL; p = p->next) {
            XMLNode *sub_node = p->data;
            IBusM17NEngineConfigNode *cnode;

            if (g_strcmp0 (sub_node->name, "engine") != 0) {
                g_warning ("<engines> element contains invalid element <%s>",
                           sub_node->name);
                continue;
            }

            cnode = g_slice_new0 (IBusM17NEngineConfigNode);
            if (!ibus_m17n_engine_config_parse_xml_node (cnode, sub_node)) {
                g_slice_free (IBusM17NEngineConfigNode, cnode);
                continue;
            }
            config_list = g_slist_prepend (config_list, cnode);
        }
        config_list = g_slist_reverse (config_list);
    } else
        g_warning ("failed to parse %s", DEFAULT_XML);
    if (node)
        ibus_xml_free (node);

    engines = ibus_m17n_list_engines ();

    for (p = engines; p != NULL; p = p->next)
        ibus_component_add_engine (component, p->data);

    g_list_free (engines);

    return component;
}

void
ibus_m17n_config_set_string (IBusConfig  *config,
                             const gchar *section,
                             const gchar *name,
                             const gchar *value)
{
#if IBUS_CHECK_VERSION(1,3,99)
    ibus_config_set_value (config, section, name, g_variant_new_string (value));
#else
    GValue v = { 0 };

    g_value_init (&v, G_TYPE_STRING);
    g_value_set_string (&v, value);
    ibus_config_set_value (config, section, name, &v);
#endif  /* !IBUS_CHECK_VERSION(1,3,99) */
}

gboolean
ibus_m17n_config_get_string (IBusConfig  *config,
                             const gchar *section,
                             const gchar *name,
                             gchar      **result)
{
#if IBUS_CHECK_VERSION(1,3,99)
    GVariant *value = NULL;

    g_return_val_if_fail (result != NULL, FALSE);

    value = ibus_config_get_value (config, section, name);
    if (value) {
        *result = g_strdup (g_variant_get_string (value, NULL));
        g_variant_unref (value);
        return TRUE;
    }
    return FALSE;
#else
    GValue value = { 0 };

    g_return_val_if_fail (result != NULL, FALSE);

    if (ibus_config_get_value (config, section, name, &value)) {
        *result = g_strdup (g_value_get_string (&value));
        g_value_unset (&value);
        return TRUE;
    }
    return FALSE;
#endif  /* !IBUS_CHECK_VERSION(1,3,99) */
}

void
ibus_m17n_config_set_int (IBusConfig  *config,
                          const gchar *section,
                          const gchar *name,
                          gint         value)
{
#if IBUS_CHECK_VERSION(1,3,99)
    ibus_config_set_value (config, section, name, g_variant_new_int32 (value));
#else
    GValue v = { 0 };

    g_value_init (&v, G_TYPE_INT);
    g_value_set_int (&v, value);
    ibus_config_set_value (config, section, name, &v);
#endif  /* !IBUS_CHECK_VERSION(1,3,99) */
}

gboolean
ibus_m17n_config_get_int (IBusConfig  *config,
                          const gchar *section,
                          const gchar *name,
                          gint        *result)
{
#if IBUS_CHECK_VERSION(1,3,99)
    GVariant *value = NULL;

    g_return_val_if_fail (result != NULL, FALSE);

    value = ibus_config_get_value (config, section, name);
    if (value) {
        *result = g_variant_get_int32 (value);
        g_variant_unref (value);
        return TRUE;
    }
    return FALSE;
#else
    GValue value = { 0 };

    g_return_val_if_fail (result != NULL, FALSE);

    if (ibus_config_get_value (config, section, name, &value)) {
        *result = g_value_get_int (&value);
        g_value_unset (&value);
        return TRUE;
    }
    return FALSE;
#endif  /* !IBUS_CHECK_VERSION(1,3,99) */
}

#ifdef DEBUG
#include <locale.h>

int main ()
{
    IBusComponent *component;
    GString *output;

    setlocale (LC_ALL, "");
    ibus_init ();
    ibus_m17n_init_common ();

    component = ibus_m17n_get_component ();

    output = g_string_new ("");

    ibus_component_output (component, output, 1);

    g_debug ("\n%s", output->str);

    g_string_free (output, TRUE);
    g_object_unref (component);

    return 0;
}
#endif
