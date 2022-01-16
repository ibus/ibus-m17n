/* vim:set et sts=4: */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <ibus.h>
#include <m17n.h>
#include <string.h>
#include "m17nutil.h"
#include "engine.h"

typedef struct _IBusM17NEngine IBusM17NEngine;
typedef struct _IBusM17NEngineClass IBusM17NEngineClass;

struct _IBusM17NEngine {
    IBusEngine parent;

    /* members */
    MInputContext *context;
    IBusLookupTable *table;
    IBusProperty    *status_prop;
#ifdef HAVE_SETUP
    IBusProperty    *setup_prop;
#endif  /* HAVE_SETUP */
    IBusPropList    *prop_list;
    IBusKeymap      *us_keymap;
    IBusInputPurpose purpose;
    IBusInputHints   hints;
};

struct _IBusM17NEngineClass {
    IBusEngineClass parent;

    /* configurations are per class */
    GSettings *gsettings;
    guint preedit_foreground;
    guint preedit_background;
    gint preedit_underline;
    IBusPreeditFocusMode preedit_focus_mode;
    gint lookup_table_orientation;
    gboolean use_us_layout;

    MInputMethod *im;
};

/* functions prototype */
static void ibus_m17n_engine_class_init     (IBusM17NEngineClass    *klass);
static void ibus_m17n_config_value_changed  (GSettings              *gsettings,
                                             const gchar            *key,
                                             IBusM17NEngineClass    *klass);

static GObject*
            ibus_m17n_engine_constructor    (GType                   type,
                                             guint                   n_construct_params,
                                             GObjectConstructParam  *construct_params);
static void ibus_m17n_engine_init           (IBusM17NEngine         *m17n);
static void ibus_m17n_engine_destroy        (IBusM17NEngine         *m17n);
static gboolean
            ibus_m17n_engine_process_key_event
                                            (IBusEngine             *engine,
                                             guint                   keyval,
                                             guint                   keycode,
                                             guint                   modifiers);
static void ibus_m17n_engine_focus_in       (IBusEngine             *engine);
static void ibus_m17n_engine_focus_out      (IBusEngine             *engine);
static void ibus_m17n_engine_reset          (IBusEngine             *engine);
static void ibus_m17n_engine_enable         (IBusEngine             *engine);
static void ibus_m17n_engine_disable        (IBusEngine             *engine);
static void ibus_m17n_engine_page_up        (IBusEngine             *engine);
static void ibus_m17n_engine_page_down      (IBusEngine             *engine);
static void ibus_m17n_engine_cursor_up      (IBusEngine             *engine);
static void ibus_m17n_engine_cursor_down    (IBusEngine             *engine);
static void ibus_m17n_engine_property_activate
                                            (IBusEngine             *engine,
                                             const gchar            *prop_name,
                                             guint                   prop_state);
static void ibus_m17n_engine_set_content_type
                                            (IBusEngine             *engine,
                                             IBusInputPurpose        purpose,
                                             IBusInputHints          hints);

static void ibus_m17n_engine_commit_string
                                            (IBusM17NEngine         *m17n,
                                             const gchar            *string);
static void ibus_m17n_engine_callback       (MInputContext          *context,
                                             MSymbol                 command);
static void ibus_m17n_engine_update_preedit (IBusM17NEngine *m17n);
static void ibus_m17n_engine_update_lookup_table
                                            (IBusM17NEngine *m17n);

static IBusEngineClass *parent_class = NULL;

void
ibus_m17n_init (IBusBus *bus)
{
    ibus_m17n_init_common ();
}

static gboolean
ibus_m17n_scan_engine_name (const gchar *engine_name,
                            gchar      **lang,
                            gchar      **name)
{
    gchar **strv;

    g_return_val_if_fail (g_str_has_prefix (engine_name, "m17n:"), FALSE);
    strv = g_strsplit (engine_name, ":", 3);

    if (g_strv_length (strv) != 3) {
        g_strfreev (strv);
        g_return_val_if_reached (FALSE);
    }

    *lang = strv[1];
    *name = strv[2];

    g_free (strv[0]);
    g_free (strv);

    return TRUE;
}

static gboolean
ibus_m17n_scan_class_name (const gchar *class_name,
                           gchar      **lang,
                           gchar      **name)
{
    gchar *p;

    g_return_val_if_fail (g_str_has_prefix (class_name, "IBusM17N"), FALSE);
    g_return_val_if_fail (g_str_has_suffix (class_name, "Engine"), FALSE);

    /* Strip prefix and suffix */
    p = *lang = g_strdup (class_name + 8);
    p = g_strrstr (p, "Engine");
    *p = '\0';

    /* Find the start position of <Name> */
    while (!g_ascii_isupper (*--p) && p > *lang)
        ;
    g_return_val_if_fail (p > *lang, FALSE);
    *name = g_strdup (p);
    *p = '\0';

    *lang[0] = g_ascii_tolower (*lang[0]);
    *name[0] = g_ascii_tolower (*name[0]);

    return TRUE;
}

GType
ibus_m17n_engine_get_type_for_name (const gchar *engine_name)
{
    GType type;
    gchar *type_name, *lang = NULL, *name = NULL;

    GTypeInfo type_info = {
        sizeof (IBusM17NEngineClass),
        (GBaseInitFunc)      NULL,
        (GBaseFinalizeFunc)  NULL,
        (GClassInitFunc)     ibus_m17n_engine_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        sizeof (IBusM17NEngine),
        0,
        (GInstanceInitFunc)  ibus_m17n_engine_init,
    };

    if (!ibus_m17n_scan_engine_name (engine_name, &lang, &name)) {
        g_free (lang);
        g_free (name);
        return G_TYPE_INVALID;
    }
    lang[0] = g_ascii_toupper (lang[0]);
    name[0] = g_ascii_toupper (name[0]);
    type_name = g_strdup_printf ("IBusM17N%s%sEngine", lang, name);
    g_free (lang);
    g_free (name);

    type = g_type_from_name (type_name);
    g_assert (type == 0 || g_type_is_a (type, IBUS_TYPE_ENGINE));

    if (type == 0) {
        type = g_type_register_static (IBUS_TYPE_ENGINE,
                                       type_name,
                                       &type_info,
                                       (GTypeFlags) 0);
    }
    g_free (type_name);

    return type;
}

static void
ibus_m17n_engine_class_init (IBusM17NEngineClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS (klass);
    IBusEngineClass *engine_class = IBUS_ENGINE_CLASS (klass);
    gchar *engine_name, *lang = NULL, *name = NULL;
    IBusM17NEngineConfig *engine_config;

    if (parent_class == NULL)
        parent_class = (IBusEngineClass *) g_type_class_peek_parent (klass);

    object_class->constructor = ibus_m17n_engine_constructor;
    ibus_object_class->destroy = (IBusObjectDestroyFunc) ibus_m17n_engine_destroy;

    engine_class->process_key_event = ibus_m17n_engine_process_key_event;

    engine_class->reset = ibus_m17n_engine_reset;
    engine_class->enable = ibus_m17n_engine_enable;
    engine_class->disable = ibus_m17n_engine_disable;

    engine_class->focus_in = ibus_m17n_engine_focus_in;
    engine_class->focus_out = ibus_m17n_engine_focus_out;

    engine_class->page_up = ibus_m17n_engine_page_up;
    engine_class->page_down = ibus_m17n_engine_page_down;

    engine_class->cursor_up = ibus_m17n_engine_cursor_up;
    engine_class->cursor_down = ibus_m17n_engine_cursor_down;

    engine_class->property_activate = ibus_m17n_engine_property_activate;

    engine_class->set_content_type = ibus_m17n_engine_set_content_type;

    if (!ibus_m17n_scan_class_name (G_OBJECT_CLASS_NAME (klass),
                                    &lang, &name)) {
        g_free (lang);
        g_free (name);
        return;
    }
    klass->gsettings = g_settings_new_with_path (
        "org.freedesktop.ibus.engine.m17n",
        g_strdup_printf ("/org/freedesktop/ibus/engine/m17n/%s/%s/",
                         lang, name));

    engine_name = g_strdup_printf ("m17n:%s:%s", lang, name);
    g_free (lang);
    g_free (name);
    engine_config = ibus_m17n_get_engine_config (engine_name);
    g_free (engine_name);

    /* configurations are per class */
    klass->preedit_foreground = engine_config->preedit_highlight ?
        PREEDIT_FOREGROUND :
        INVALID_COLOR;
    klass->preedit_background = engine_config->preedit_highlight ?
        PREEDIT_BACKGROUND :
        INVALID_COLOR;
    klass->preedit_underline = IBUS_ATTR_UNDERLINE_NONE;
    klass->preedit_focus_mode = IBUS_ENGINE_PREEDIT_COMMIT;
    klass->lookup_table_orientation = IBUS_ORIENTATION_SYSTEM;
    klass->use_us_layout = FALSE;

    ibus_m17n_engine_config_free (engine_config);

    if (klass->gsettings != NULL) {
        GVariant *value;

        value = g_settings_get_value (klass->gsettings,
                                      "preedit-foreground");
        if (value != NULL) {
            const gchar *hex = g_variant_get_string (value, NULL);
            klass->preedit_foreground = ibus_m17n_parse_color (hex);
            g_variant_unref (value);
        }

        value = g_settings_get_value (klass->gsettings,
                                      "preedit-background");
        if (value != NULL) {
            const gchar *hex = g_variant_get_string (value, NULL);
            klass->preedit_background = ibus_m17n_parse_color (hex);
            g_variant_unref (value);
        }

        value = g_settings_get_value (klass->gsettings,
                                      "preedit-underline");
        if (value != NULL) {
            klass->preedit_underline = g_variant_get_int32 (value);
            g_variant_unref (value);
        }

        value = g_settings_get_value (klass->gsettings,
                                      "lookup-table-orientation");
        if (value != NULL) {
            klass->lookup_table_orientation = g_variant_get_int32 (value);
            g_variant_unref (value);
        }

        value = g_settings_get_value (klass->gsettings,
                                      "use-us-layout");
        if (value != NULL) {
            klass->use_us_layout = g_variant_get_boolean (value);
            g_variant_unref (value);
        }
    }

    g_signal_connect (klass->gsettings, "changed",
                      G_CALLBACK(ibus_m17n_config_value_changed),
                      klass);

    klass->im = NULL;
}

static void
ibus_m17n_config_value_changed (GSettings           *gsettings,
                                const gchar         *key,
                                IBusM17NEngineClass *klass)
{
    GVariant *value;
    value = g_settings_get_value (gsettings, key);
    if (g_strcmp0 (key, "preedit-foreground") == 0) {
        const gchar *hex = g_variant_get_string (value, NULL);
        guint color;
        color = ibus_m17n_parse_color (hex);
        if (color != INVALID_COLOR) {
            klass->preedit_foreground = color;
        }
        else {
            klass->preedit_foreground = INVALID_COLOR;
        }
    } else if (g_strcmp0 (key, "preedit-background") == 0) {
        const gchar *hex = g_variant_get_string (value, NULL);
        guint color;
        color = ibus_m17n_parse_color (hex);
        if (color != INVALID_COLOR) {
            klass->preedit_background = color;
        }
        else {
            klass->preedit_background = INVALID_COLOR;
        }
    } else if (g_strcmp0 (key, "preedit-underline") == 0) {
        klass->preedit_underline = g_variant_get_int32 (value);
    } else if (g_strcmp0 (key, "lookup-table-orientation") == 0) {
        klass->lookup_table_orientation = g_variant_get_int32 (value);
    } else if (g_strcmp0 (key, "use-us-layout") == 0) {
        klass->use_us_layout = g_variant_get_boolean (value);
    }
    g_variant_unref (value);
}

static void
ibus_m17n_engine_init (IBusM17NEngine *m17n)
{
    IBusText* label;
    IBusText* tooltip;

    m17n->prop_list = ibus_prop_list_new ();
    g_object_ref_sink (m17n->prop_list);

    m17n->status_prop = ibus_property_new ("status",
                                           PROP_TYPE_NORMAL,
                                           NULL,
                                           NULL,
                                           NULL,
                                           TRUE,
                                           FALSE,
                                           0,
                                           NULL);
    g_object_ref_sink (m17n->status_prop);
    ibus_prop_list_append (m17n->prop_list,  m17n->status_prop);

#ifdef HAVE_SETUP
    label = ibus_text_new_from_string ("Setup");
    tooltip = ibus_text_new_from_string ("Configure M17N engine");
    m17n->setup_prop = ibus_property_new ("setup",
                                          PROP_TYPE_NORMAL,
                                          label,
                                          "gtk-preferences",
                                          tooltip,
                                          TRUE,
                                          TRUE,
                                          PROP_STATE_UNCHECKED,
                                          NULL);
    g_object_ref_sink (m17n->setup_prop);
    ibus_prop_list_append (m17n->prop_list, m17n->setup_prop);
#endif  /* HAVE_SETUP */

    m17n->table = ibus_lookup_table_new (9, 0, TRUE, TRUE);
    g_object_ref_sink (m17n->table);
    m17n->context = NULL;
    m17n->us_keymap = ibus_keymap_get ("us");
}

static GObject*
ibus_m17n_engine_constructor (GType                   type,
                              guint                   n_construct_params,
                              GObjectConstructParam  *construct_params)
{
    IBusM17NEngine *m17n;
    GObjectClass *object_class;
    IBusM17NEngineClass *klass;

    m17n = (IBusM17NEngine *) G_OBJECT_CLASS (parent_class)->constructor (type,
                                                       n_construct_params,
                                                       construct_params);

    object_class = G_OBJECT_GET_CLASS (m17n);
    klass = (IBusM17NEngineClass *) object_class;
    if (klass->im == NULL) {
        const gchar *engine_name;
        gchar *lang = NULL, *name = NULL;

        engine_name = ibus_engine_get_name ((IBusEngine *) m17n);
        if (!ibus_m17n_scan_engine_name (engine_name, &lang, &name)) {
            g_free (lang);
            g_free (name);
            return NULL;
        }

        klass->im = minput_open_im (msymbol (lang), msymbol (name), NULL);
        g_free (lang);
        g_free (name);

        if (klass->im == NULL) {
            g_warning ("Can not find m17n keymap %s", engine_name);
            g_object_unref (m17n);
            return NULL;
        }

        mplist_put (klass->im->driver.callback_list, Minput_preedit_start, ibus_m17n_engine_callback);
        mplist_put (klass->im->driver.callback_list, Minput_preedit_draw, ibus_m17n_engine_callback);
        mplist_put (klass->im->driver.callback_list, Minput_preedit_done, ibus_m17n_engine_callback);
        mplist_put (klass->im->driver.callback_list, Minput_status_start, ibus_m17n_engine_callback);
        mplist_put (klass->im->driver.callback_list, Minput_status_draw, ibus_m17n_engine_callback);
        mplist_put (klass->im->driver.callback_list, Minput_status_done, ibus_m17n_engine_callback);
        mplist_put (klass->im->driver.callback_list, Minput_candidates_start, ibus_m17n_engine_callback);
        mplist_put (klass->im->driver.callback_list, Minput_candidates_draw, ibus_m17n_engine_callback);
        mplist_put (klass->im->driver.callback_list, Minput_candidates_done, ibus_m17n_engine_callback);
        mplist_put (klass->im->driver.callback_list, Minput_set_spot, ibus_m17n_engine_callback);
        mplist_put (klass->im->driver.callback_list, Minput_toggle, ibus_m17n_engine_callback);
        /*
          Does not set reset callback, uses the default callback in m17n.
          mplist_put (klass->im->driver.callback_list, Minput_reset, ibus_m17n_engine_callback);
        */
        mplist_put (klass->im->driver.callback_list, Minput_get_surrounding_text, ibus_m17n_engine_callback);
        mplist_put (klass->im->driver.callback_list, Minput_delete_surrounding_text, ibus_m17n_engine_callback);
    }

    m17n->context = minput_create_ic (klass->im, m17n);

    return (GObject *) m17n;
}

static void
ibus_m17n_engine_destroy (IBusM17NEngine *m17n)
{
    if (m17n->prop_list) {
        g_object_unref (m17n->prop_list);
        m17n->prop_list = NULL;
    }

    if (m17n->status_prop) {
        g_object_unref (m17n->status_prop);
        m17n->status_prop = NULL;
    }

#if HAVE_SETUP
    if (m17n->setup_prop) {
        g_object_unref (m17n->setup_prop);
        m17n->setup_prop = NULL;
    }
#endif  /* HAVE_SETUP */

    if (m17n->table) {
        g_object_unref (m17n->table);
        m17n->table = NULL;
    }

    if (m17n->context) {
        minput_destroy_ic (m17n->context);
        m17n->context = NULL;
    }

    if (m17n->us_keymap) {
        g_object_unref (m17n->us_keymap);
        m17n->us_keymap = NULL;
    }

    IBUS_OBJECT_CLASS (parent_class)->destroy ((IBusObject *)m17n);
}

static void
ibus_m17n_engine_update_preedit (IBusM17NEngine *m17n)
{
    IBusText *text;
    gchar *buf;
    IBusM17NEngineClass *klass = (IBusM17NEngineClass *) G_OBJECT_GET_CLASS (m17n);

    buf = ibus_m17n_mtext_to_utf8 (m17n->context->preedit);
    if (buf) {
        text = ibus_text_new_from_static_string (buf);
        if (klass->preedit_foreground != INVALID_COLOR)
            ibus_text_append_attribute (text, IBUS_ATTR_TYPE_FOREGROUND,
                                        klass->preedit_foreground, 0, -1);
        if (klass->preedit_background != INVALID_COLOR)
            ibus_text_append_attribute (text, IBUS_ATTR_TYPE_BACKGROUND,
                                        klass->preedit_background, 0, -1);
        ibus_text_append_attribute (text, IBUS_ATTR_TYPE_UNDERLINE,
                                    klass->preedit_underline, 0, -1);
        ibus_engine_update_preedit_text_with_mode ((IBusEngine *) m17n,
                                                   text,
                                                   m17n->context->cursor_pos,
                                                   mtext_len (m17n->context->preedit) > 0,
                                                   klass->preedit_focus_mode);
    }
}

static void
ibus_m17n_engine_commit_string (IBusM17NEngine *m17n,
                                const gchar    *string)
{
    IBusText *text;
    text = ibus_text_new_from_static_string (string);
    ibus_engine_commit_text ((IBusEngine *)m17n, text);
    ibus_m17n_engine_update_preedit (m17n);
}

/* Note on AltGr (Level3 Shift) handling: While currently we expect
   AltGr == mod5, it would be better to not expect the modifier always
   be assigned to particular modX.  However, it needs some code like:

   KeyCode altgr = XKeysymToKeycode (display, XK_ISO_Level3_Shift);
   XModifierKeymap *mods = XGetModifierMapping (display);
   for (i = 3; i < 8; i++)
     for (j = 0; j < mods->max_keypermod; j++) {
       KeyCode code = mods->modifiermap[i * mods->max_keypermod + j];
       if (code == altgr)
         ...
     }
                
   Since IBus engines are supposed to be cross-platform, the code
   should go into IBus core, instead of ibus-m17n. */
static MSymbol
ibus_m17n_key_event_to_symbol (IBusM17NEngine *m17n,
                               guint           keycode,
                               guint           keyval,
                               guint           modifiers)
{
    GString *keysym;
    MSymbol mkeysym = Mnil;
    guint mask = 0;

    if (keyval >= IBUS_Shift_L && keyval <= IBUS_Hyper_R) {
        return Mnil;
    }

    /* If keyval is already translated by IBUS_MOD5_MASK.  Try to
       obtain the untranslated keyval from the US keymap. */
    if (modifiers & IBUS_MOD5_MASK) {
        keyval = ibus_keymap_lookup_keysym (m17n->us_keymap,
                                            keycode,
                                            modifiers & ~IBUS_MOD5_MASK);
    }

    keysym = g_string_new ("");

    if (keyval >= IBUS_space && keyval <= IBUS_asciitilde) {
        gint c = keyval;

        if (keyval == IBUS_space && modifiers & IBUS_SHIFT_MASK)
            mask |= IBUS_SHIFT_MASK;

        if (modifiers & IBUS_CONTROL_MASK) {
            if (c >= IBUS_a && c <= IBUS_z)
                c += IBUS_A - IBUS_a;
            mask |= IBUS_CONTROL_MASK;
        }

        g_string_append_c (keysym, c);
    }
    else {
        const gchar *name = ibus_keyval_name (keyval);
        if (name == NULL) {
            g_string_free (keysym, TRUE);
            return Mnil;
        }
        mask |= modifiers & (IBUS_CONTROL_MASK | IBUS_SHIFT_MASK);
        g_string_append (keysym, name);
    }

    mask |= modifiers & (IBUS_MOD1_MASK |
                         IBUS_MOD5_MASK |
                         IBUS_META_MASK |
                         IBUS_SUPER_MASK |
                         IBUS_HYPER_MASK);


    if (mask & IBUS_HYPER_MASK) {
        g_string_prepend (keysym, "H-");
    }
    if (mask & IBUS_SUPER_MASK) {
        g_string_prepend (keysym, "s-");
    }
    if (mask & IBUS_MOD5_MASK) {
        g_string_prepend (keysym, "G-");
    }
    if (mask & IBUS_MOD1_MASK) {
        g_string_prepend (keysym, "A-");
    }
    if (mask & IBUS_META_MASK) {
        g_string_prepend (keysym, "M-");
    }
    if (mask & IBUS_CONTROL_MASK) {
        g_string_prepend (keysym, "C-");
    }
    if (mask & IBUS_SHIFT_MASK) {
        g_string_prepend (keysym, "S-");
    }

    mkeysym = msymbol (keysym->str);
    g_string_free (keysym, TRUE);

    return mkeysym;
}

static gboolean
ibus_m17n_engine_process_key (IBusM17NEngine *m17n,
                              MSymbol         key)
{
    gchar *buf;
    MText *produced;
    gint retval;

    retval = minput_filter (m17n->context, key, NULL);

    if (retval) {
        return TRUE;
    }

    produced = mtext ();

    retval = minput_lookup (m17n->context, key, NULL, produced);

    if (retval) {
        // g_debug ("minput_lookup returns %d", retval);
    }

    buf = ibus_m17n_mtext_to_utf8 (produced);
    m17n_object_unref (produced);

    if (buf && strlen (buf)) {
        ibus_m17n_engine_commit_string (m17n, buf);
    }
    g_free (buf);

    return retval == 0;
}

static gboolean
ibus_m17n_engine_process_key_event (IBusEngine     *engine,
                                    guint           keyval,
                                    guint           keycode,
                                    guint           modifiers)
{
    IBusM17NEngine *m17n = (IBusM17NEngine *) engine;
    IBusM17NEngineClass *klass =
        (IBusM17NEngineClass *) G_OBJECT_GET_CLASS (m17n);
    guint original_keyval = keyval;

    switch (m17n->purpose) {
    case IBUS_INPUT_PURPOSE_PASSWORD:
    case IBUS_INPUT_PURPOSE_PIN:
        /* For password and PIN input, skip any further step
           processing a key event.  */
        return FALSE;

    default:
        break;
    }

    if (modifiers & IBUS_RELEASE_MASK)
        return FALSE;

    if (klass->use_us_layout) {
        keyval = ibus_keymap_lookup_keysym (m17n->us_keymap,
                                            keycode,
                                            modifiers);
    }

    MSymbol m17n_key = ibus_m17n_key_event_to_symbol (m17n,
                                                      keycode,
                                                      keyval,
                                                      modifiers);
    if (m17n_key != Mnil && ibus_m17n_engine_process_key (m17n, m17n_key)) {
        return TRUE;
    }

    /* If keyval is translated in US layout, send the new keyval and
       notify that the event is handled. */
    if (keyval != original_keyval && 0x20 <= keyval && keyval < 0x7F) {
        gchar buf[2];
        buf[0] = keyval;
        buf[1] = '\0';
        ibus_m17n_engine_commit_string (m17n, buf);
        return TRUE;
    }

    return FALSE;
}

static void
ibus_m17n_engine_focus_in (IBusEngine *engine)
{
    IBusM17NEngine *m17n = (IBusM17NEngine *) engine;

    ibus_engine_register_properties (engine, m17n->prop_list);
    ibus_m17n_engine_process_key (m17n, Minput_focus_in);

    parent_class->focus_in (engine);
}

static void
ibus_m17n_engine_focus_out (IBusEngine *engine)
{
    IBusM17NEngine *m17n = (IBusM17NEngine *) engine;

    /* To make ibus_engine_update_preedit_text_with_mode work
       properly, we just reset the IC instead of passing Mfocus_out to
       m17n-lib. */
    minput_reset_ic (m17n->context);

    parent_class->focus_out (engine);
}

static void
ibus_m17n_engine_reset (IBusEngine *engine)
{
    IBusM17NEngine *m17n = (IBusM17NEngine *) engine;

    parent_class->reset (engine);

    minput_reset_ic (m17n->context);
}

static void
ibus_m17n_engine_enable (IBusEngine *engine)
{
    parent_class->enable (engine);

    /* Issue a dummy ibus_engine_get_surrounding_text() call to tell
       input context that we will use surrounding-text. */
    ibus_engine_get_surrounding_text (engine, NULL, NULL, NULL);
}

static void
ibus_m17n_engine_disable (IBusEngine *engine)
{
    ibus_m17n_engine_focus_out (engine);
    parent_class->disable (engine);
}

static void
ibus_m17n_engine_page_up (IBusEngine *engine)
{
    IBusM17NEngine *m17n = (IBusM17NEngine *) engine;

    ibus_m17n_engine_process_key (m17n, msymbol ("Up"));
    parent_class->page_up (engine);
}

static void
ibus_m17n_engine_page_down (IBusEngine *engine)
{

    IBusM17NEngine *m17n = (IBusM17NEngine *) engine;

    ibus_m17n_engine_process_key (m17n, msymbol ("Down"));
    parent_class->page_down (engine);
}

static void
ibus_m17n_engine_cursor_up (IBusEngine *engine)
{

    IBusM17NEngine *m17n = (IBusM17NEngine *) engine;

    ibus_m17n_engine_process_key (m17n, msymbol ("Left"));
    parent_class->cursor_up (engine);
}

static void
ibus_m17n_engine_cursor_down (IBusEngine *engine)
{

    IBusM17NEngine *m17n = (IBusM17NEngine *) engine;

    ibus_m17n_engine_process_key (m17n, msymbol ("Right"));
    parent_class->cursor_down (engine);
}

static void
ibus_m17n_engine_property_activate (IBusEngine  *engine,
                                    const gchar *prop_name,
                                    guint        prop_state)
{
    IBusM17NEngine *m17n = (IBusM17NEngine *) engine;

#ifdef HAVE_SETUP
    if (g_strcmp0 (prop_name, "setup") == 0) {
        const gchar *engine_name;
        gchar *setup;

        engine_name = ibus_engine_get_name ((IBusEngine *) m17n);
        g_assert (engine_name);
        setup = g_strdup_printf ("%s/ibus-setup-m17n --name %s",
                                 LIBEXECDIR, engine_name);
        g_spawn_command_line_async (setup, NULL);
        g_free (setup);
    }
#endif  /* HAVE_SETUP */

    parent_class->property_activate (engine, prop_name, prop_state);
}

static void
ibus_m17n_engine_set_content_type (IBusEngine      *engine,
                                   IBusInputPurpose purpose,
                                   IBusInputHints   hints)
{
    IBusM17NEngine *m17n = (IBusM17NEngine *) engine;

    m17n->purpose = purpose;
    m17n->hints = hints;

    switch (purpose) {
    case IBUS_INPUT_PURPOSE_PASSWORD:
    case IBUS_INPUT_PURPOSE_PIN:
        /* For password and PIN input, emulate 'focus-out' to discard
           any pending input status (e.g. preedit or candidate
           list).  */
        ibus_m17n_engine_focus_out (engine);
        break;

    default:
        ibus_m17n_engine_focus_in (engine);
        break;
    }
}

static void
ibus_m17n_engine_update_lookup_table (IBusM17NEngine *m17n)
{
    ibus_lookup_table_clear (m17n->table);

    if (m17n->context->candidate_list && m17n->context->candidate_show) {
        IBusText *text;
        MPlist *group;
        group = m17n->context->candidate_list;
        gint i = 0;
        gint page = 1;
        IBusM17NEngineClass *klass = (IBusM17NEngineClass *) G_OBJECT_GET_CLASS (m17n);

        while (1) {
            gint len;
            if (mplist_key (group) == Mtext)
                len = mtext_len ((MText *) mplist_value (group));
            else
                len = mplist_length ((MPlist *) mplist_value (group));

            if (i + len > m17n->context->candidate_index)
                break;

            i += len;
            group = mplist_next (group);
            page ++;
        }

        if (mplist_key (group) == Mtext) {
            MText *mt;
            gunichar *buf;
            glong nchars, i;

            mt = (MText *) mplist_value (group);
            ibus_lookup_table_set_page_size (m17n->table, mtext_len (mt));

            buf = ibus_m17n_mtext_to_ucs4 (mt, &nchars);
            g_warn_if_fail (buf != NULL);

            for (i = 0; buf != NULL && i < nchars; i++) {
                IBusText *text = ibus_text_new_from_unichar (buf[i]);
                if (text == NULL) {
                    text = ibus_text_new_from_printf ("INVCODE=U+%04"G_GINT32_FORMAT"X", buf[i]);
                    g_warn_if_reached ();
                }
                ibus_lookup_table_append_candidate (m17n->table, text);
            }
            g_free (buf);
        }
        else {
            MPlist *p;

            p = (MPlist *) mplist_value (group);
            ibus_lookup_table_set_page_size (m17n->table, mplist_length (p));

            for (; mplist_key (p) != Mnil; p = mplist_next (p)) {
                MText *mtext;
                gchar *buf;

                mtext = (MText *) mplist_value (p);
                buf = ibus_m17n_mtext_to_utf8 (mtext);
                if (buf) {
                    ibus_lookup_table_append_candidate (m17n->table,
                        ibus_text_new_from_string (buf));
                    g_free (buf);
                }
                else {
                    ibus_lookup_table_append_candidate (m17n->table,
                        ibus_text_new_from_static_string ("NULL"));
                    g_warn_if_reached();
                }
            }
        }

        ibus_lookup_table_set_cursor_pos (m17n->table, m17n->context->candidate_index - i);
        ibus_lookup_table_set_orientation (m17n->table, klass->lookup_table_orientation);

        text = ibus_text_new_from_printf ("( %d / %d )", page, mplist_length (m17n->context->candidate_list));

        ibus_engine_update_lookup_table ((IBusEngine *)m17n, m17n->table, TRUE);
        ibus_engine_update_auxiliary_text ((IBusEngine *)m17n, text, TRUE);
    }
    else {
        ibus_engine_hide_lookup_table ((IBusEngine *)m17n);
        ibus_engine_hide_auxiliary_text ((IBusEngine *)m17n);
    }
}

static void
ibus_m17n_engine_callback (MInputContext *context,
                           MSymbol        command)
{
    IBusM17NEngine *m17n = NULL;

    m17n = context->arg;
    g_return_if_fail (m17n != NULL);

    /* the callback may be called in minput_create_ic, in the time
     * m17n->context has not be assigned, so need assign it. */
    if (m17n->context == NULL) {
        m17n->context = context;
    }

    if (command == Minput_preedit_start) {
        ibus_engine_hide_preedit_text ((IBusEngine *)m17n);
    }
    else if (command == Minput_preedit_draw) {
        ibus_m17n_engine_update_preedit (m17n);
    }
    else if (command == Minput_preedit_done) {
        ibus_engine_hide_preedit_text ((IBusEngine *)m17n);
    }
    else if (command == Minput_status_start) {
        ibus_engine_hide_preedit_text ((IBusEngine *)m17n);
    }
    else if (command == Minput_status_draw) {
        gchar *status;
        status = ibus_m17n_mtext_to_utf8 (m17n->context->status);

        if (status && strlen (status)) {
            IBusText *text;
            text = ibus_text_new_from_string (status);
            ibus_property_set_label (m17n->status_prop, text);
            ibus_property_set_visible (m17n->status_prop, TRUE);
        }
        else {
            ibus_property_set_label (m17n->status_prop, NULL);
            ibus_property_set_visible (m17n->status_prop, FALSE);
        }

        ibus_engine_update_property ((IBusEngine *)m17n, m17n->status_prop);
        g_free (status);
    }
    else if (command == Minput_status_done) {
    }
    else if (command == Minput_candidates_start) {
        ibus_engine_hide_lookup_table ((IBusEngine *) m17n);
        ibus_engine_hide_auxiliary_text ((IBusEngine *) m17n);
    }
    else if (command == Minput_candidates_draw) {
        ibus_m17n_engine_update_lookup_table (m17n);
    }
    else if (command == Minput_candidates_done) {
        ibus_engine_hide_lookup_table ((IBusEngine *) m17n);
        ibus_engine_hide_auxiliary_text ((IBusEngine *) m17n);
    }
    else if (command == Minput_set_spot) {
    }
    else if (command == Minput_toggle) {
    }
    else if (command == Minput_reset) {
    }
    else if (command == Minput_get_surrounding_text &&
             (((IBusEngine *) m17n)->client_capabilities &
              IBUS_CAP_SURROUNDING_TEXT) != 0) {
        IBusText *text;
        guint cursor_pos, anchor_pos, nchars, nbytes;
        MText *mt, *surround;
        int len, pos;

        ibus_engine_get_surrounding_text ((IBusEngine *) m17n,
                                          &text,
                                          &cursor_pos,
                                          &anchor_pos);
        nchars = ibus_text_get_length (text);
        nbytes = g_utf8_offset_to_pointer (text->text, nchars) - text->text;
        mt = mconv_decode_buffer (Mcoding_utf_8,
                                  (const unsigned char *) text->text,
                                  nbytes);
        g_object_unref (text);

        len = (long) mplist_value (m17n->context->plist);
        if (len < 0) {
            pos = cursor_pos + len;
            if (pos < 0)
                pos = 0;
            surround = mtext_duplicate (mt, pos, cursor_pos);
        }
        else if (len > 0) {
            pos = cursor_pos + len;
            if (pos > nchars)
                pos = nchars;
            surround = mtext_duplicate (mt, cursor_pos, pos);
        }
        else {
            surround = mtext ();
        }
        m17n_object_unref (mt);
        mplist_set (m17n->context->plist, Mtext, surround);
        m17n_object_unref (surround);
    }
    else if (command == Minput_delete_surrounding_text &&
             (((IBusEngine *) m17n)->client_capabilities &
              IBUS_CAP_SURROUNDING_TEXT) != 0) {
        int len;

        len = (long) mplist_value (m17n->context->plist);
        if (len < 0)
            ibus_engine_delete_surrounding_text ((IBusEngine *) m17n,
                                                 len, -len);
        else if (len > 0)
            ibus_engine_delete_surrounding_text ((IBusEngine *) m17n,
                                                 0, len);
    }
}
