/* vim:set et sts=4: */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <ibus.h>
#include <m17n.h>
#include <string.h>
#include <errno.h>
#include "m17nutil.h"

enum {
    COLUMN_KEY,
    COLUMN_VALUE,
    COLUMN_DESCRIPTION,
    NUM_COLS
};

struct _ConfigContext {
    IBusConfig *config;
    MSymbol language;
    MSymbol name;
    GtkListStore *store;
    gchar *section;
    GtkWidget *colorbutton_foreground;
    GtkWidget *colorbutton_background;

};
typedef struct _ConfigContext ConfigContext;

static IBusConfig *config = NULL;

static gchar *opt_name = NULL;
static const GOptionEntry options[] = {
    {"name", '\0', 0, G_OPTION_ARG_STRING, &opt_name,
     "IBus engine name like \"m17n:si:wijesekera\"."},
    {NULL}
};

void
ibus_m17n_init (IBusBus *bus)
{
    config = ibus_bus_get_config (bus);
    if (config)
        g_object_ref_sink (config);
    ibus_m17n_init_common ();
}

static gchar *
format_value (MPlist *plist)
{
    if (mplist_key (plist) == Msymbol)
        return g_strdup (msymbol_name ((MSymbol) mplist_value (plist)));

    if (mplist_key (plist) == Mtext)
        return g_strdup (mtext_data ((MText *) mplist_value (plist),
                                     NULL, NULL, NULL, NULL));

    if (mplist_key (plist) == Minteger)
        return g_strdup_printf ("%d", (gint) (long) mplist_value (plist));

    return NULL;
}

static MPlist *
parse_value (MPlist *plist, gchar *text)
{
    MPlist *value;

    if (mplist_key (plist) == Msymbol) {
        value = mplist ();
        mplist_add (value, Msymbol, msymbol (text));
        return value;
    }

    if (mplist_key (plist) == Mtext) {
        MText *mtext;

        mtext = mtext_from_data (text, strlen (text), MTEXT_FORMAT_UTF_8);
        value = mplist ();
        mplist_add (value, Mtext, mtext);
        return value;
    }

    if (mplist_key (plist) == Minteger) {
        long val;

        errno = 0;
        val = strtol (text, NULL, 10);
        if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
            || (errno != 0 && val == 0))
            return NULL;
        value = mplist ();
        mplist_add (value, Minteger, (void *)val);
        return value;
    }

    return NULL;
}

static void
insert_items (GtkListStore *store, MSymbol language, MSymbol name)
{
    MPlist *plist;

    plist = minput_get_variable (language, name, Mnil);

    for (; plist && mplist_key (plist) == Mplist; plist = mplist_next (plist)) {
        GtkTreeIter iter;
        MSymbol key;
        MPlist *p, *value;
        gchar *description;

        p = mplist_value (plist);
        key = mplist_value (p); /* name */

        p = mplist_next (p);  /* description */
        description = ibus_m17n_mtext_to_utf8 ((MText *) mplist_value (p));
        p = mplist_next (p);  /* status */
        value = mplist_next (p);

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COLUMN_KEY, msymbol_name (key),
                            COLUMN_DESCRIPTION, description,
                            COLUMN_VALUE, format_value (value),
                            -1);
        g_free (description);
    }
}

static gboolean
on_query_tooltip (GtkWidget  *widget,
                  gint        x,
                  gint        y,
                  gboolean    keyboard_tip,
                  GtkTooltip *tooltip,
                  gpointer    user_data)
{
    GtkTreeView *treeview = GTK_TREE_VIEW(widget);
    GtkTreeModel *model = gtk_tree_view_get_model (treeview);
    GtkTreePath *path = NULL;
    GtkTreeIter iter;
    gchar *description;

    if (!gtk_tree_view_get_tooltip_context (treeview, &x, &y,
                                            keyboard_tip,
                                            &model, &path, &iter))
        return FALSE;

    gtk_tree_model_get (model, &iter, COLUMN_DESCRIPTION, &description, -1);
    gtk_tooltip_set_text (tooltip, description);
    gtk_tree_view_set_tooltip_row (treeview, tooltip, path);
    gtk_tree_path_free (path);
    g_free (description);
    return TRUE;
}

static void
on_edited (GtkCellRendererText *cell,
           gchar               *path_string,
           gchar               *new_text,
           gpointer             data)
{
    ConfigContext *context = data;
    GtkTreeModel *model = GTK_TREE_MODEL (context->store);
    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
    MPlist *plist, *p, *value;
    gchar *key;

    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_model_get (model, &iter, COLUMN_KEY, &key, -1);

    plist = minput_get_variable (context->language, context->name,
                                 msymbol (key));
    if (!plist)
        goto fail;

    p = mplist_next (mplist_next (mplist_next (mplist_value (plist))));
    if (!p)
        goto fail;

    value = parse_value (p, new_text);
    if (!value)
        goto fail;

    if (minput_config_variable (context->language, context->name,
                                msymbol (key), value) != 0)
        goto fail;

    if (minput_save_config () != 1)
        goto fail;

    gtk_list_store_set (context->store, &iter,
                        COLUMN_VALUE, new_text,
                        -1);

 fail:
    gtk_tree_path_free (path);
}

static void
color_to_gdk (guint color, GdkColor *color_gdk)
{
    memset (color_gdk, 0, sizeof *color_gdk);
    color_gdk->red = (color >> 8) & 0xFF00;
    color_gdk->green = color & 0xFF00;
    color_gdk->blue = (color & 0xFF) << 8;
}

static void
set_color (ConfigContext *context, const gchar *name, GdkColor *color)
{
    gchar buf[8];

    if (color)
        sprintf (buf, "#%02X%02X%02X",
                 (color->red & 0xFF00) >> 8,
                 (color->green & 0xFF00) >> 8,
                 (color->blue & 0xFF00) >> 8);
    else
        strcpy (buf, "none");
    ibus_m17n_config_set_string (config, context->section, name, buf);
}

static void
on_foreground_color_set (GtkColorButton *widget,
                         gpointer        user_data)
{
    ConfigContext *context = user_data;
    GdkColor color;

    gtk_color_button_get_color (GTK_COLOR_BUTTON(widget), &color);
    set_color (context, "preedit_foreground", &color);
}

static void
on_background_color_set (GtkColorButton *widget,
                         gpointer        user_data)
{
    ConfigContext *context = user_data;
    GdkColor color;

    gtk_color_button_get_color (GTK_COLOR_BUTTON(widget), &color);
    set_color (context, "preedit_background", &color);
}

static void
on_underline_changed (GtkComboBox *combo,
                      gpointer     user_data)
{
    ConfigContext *context = user_data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint active;

    model = gtk_combo_box_get_model (combo);
    gtk_combo_box_get_active_iter (combo, &iter);
    gtk_tree_model_get (model, &iter, COLUMN_VALUE, &active, -1);

    ibus_m17n_config_set_int (config,
                              context->section,
                              "preedit_underline",
                              active);
}

static void
on_orientation_changed (GtkComboBox *combo,
                        gpointer     user_data)
{
    ConfigContext *context = user_data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint active;

    model = gtk_combo_box_get_model (combo);
    gtk_combo_box_get_active_iter (combo, &iter);
    gtk_tree_model_get (model, &iter, COLUMN_VALUE, &active, -1);

    ibus_m17n_config_set_int (config,
                              context->section,
                              "lookup_table_orientation",
                              active);
}

static void
toggle_color (ConfigContext *context,
              GtkToggleButton *togglebutton,
              GtkWidget *colorbutton,
              const gchar *name)
{
    GdkColor color;

    if (gtk_toggle_button_get_active (togglebutton)) {
        gtk_widget_set_sensitive (colorbutton, TRUE);
        gtk_color_button_get_color (GTK_COLOR_BUTTON(colorbutton), &color);
        set_color (context, name, &color);
    } else {
        gtk_widget_set_sensitive (colorbutton, FALSE);
        gtk_color_button_get_color (GTK_COLOR_BUTTON(colorbutton), &color);
        set_color (context, name, NULL);
    }
}

static void
on_foreground_toggled (GtkToggleButton *togglebutton,
                       gpointer         user_data)
{
    ConfigContext *context = user_data;

    toggle_color (context,
                  togglebutton,
                  context->colorbutton_foreground,
                  "preedit_foreground");
}

static void
on_background_toggled (GtkToggleButton *togglebutton,
                       gpointer         user_data)
{
    ConfigContext *context = user_data;

    toggle_color (context,
                  togglebutton,
                  context->colorbutton_background,
                  "preedit_background");
}

static gint
get_combo_box_index_by_value (GtkComboBox *combobox, gint value)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint index;

    index = 0;
    model = gtk_combo_box_get_model (combobox);
    if (!gtk_tree_model_get_iter_first (model, &iter))
        return -1;

    do {
        gint _value;
        gtk_tree_model_get (model, &iter, COLUMN_VALUE, &_value, -1);
        if (_value == value)
            return index;
        index++;
    } while (gtk_tree_model_iter_next (model, &iter));
    return -1;
}

static void
start (const gchar *engine_name)
{
    IBusBus *bus;
    gchar **strv, *lang, *name;
    GtkBuilder *builder;
    GtkWidget *dialog;
    GtkWidget *combobox_underline, *combobox_orientation;
    GtkWidget *checkbutton_foreground, *checkbutton_background;
    GtkWidget *treeview;
    GtkListStore *store;
    GObject *object;
    GError *error = NULL;
    GtkCellRenderer *renderer;
    ConfigContext context;
    gchar *color;
    gboolean is_foreground_set, is_background_set;
    GdkColor foreground, background;
    gint underline;
    gint orientation;
    gint index;

    ibus_init ();

    bus = ibus_bus_new ();
    //g_signal_connect (bus, "disconnected", G_CALLBACK (ibus_disconnected_cb), NULL);
    ibus_m17n_init (bus);

    strv = g_strsplit (engine_name, ":", 3);
    
    g_assert (g_strv_length (strv) == 3);
    g_assert (g_strcmp0 (strv[0], "m17n") == 0);

    lang = strv[1];
    name = strv[2];

    config = ibus_bus_get_config (bus);
    context.section = g_strdup_printf ("engine/M17N/%s/%s", lang, name);

    builder = gtk_builder_new ();
    gtk_builder_set_translation_domain (builder, "ibus-m17n");
    gtk_builder_add_from_file (builder,
                               SETUPDIR "/ibus-m17n-preferences.ui",
                               &error);
    object = gtk_builder_get_object (builder, "dialog");
    dialog = GTK_WIDGET(object);
    object = gtk_builder_get_object (builder, "checkbutton_foreground");
    checkbutton_foreground = GTK_WIDGET(object);
    object = gtk_builder_get_object (builder, "colorbutton_foreground");
    context.colorbutton_foreground = GTK_WIDGET(object);
    object = gtk_builder_get_object (builder, "checkbutton_background");
    checkbutton_background = GTK_WIDGET(object);
    object = gtk_builder_get_object (builder, "colorbutton_background");
    context.colorbutton_background = GTK_WIDGET(object);
    object = gtk_builder_get_object (builder, "combobox_underline");
    combobox_underline = GTK_WIDGET(object);
    object = gtk_builder_get_object (builder, "combobox_orientation");
    combobox_orientation = GTK_WIDGET(object);
    object = gtk_builder_get_object (builder, "treeviewMimConfig");
    treeview = GTK_WIDGET(object);

    /* General -> Pre-edit Appearance */
    /* foreground color of pre-edit buffer */
    is_foreground_set = FALSE;
    color_to_gdk (PREEDIT_FOREGROUND, &foreground);
    if (ibus_m17n_config_get_string (config,
                                     context.section,
                                     "preedit_foreground",
                                     &color)) {
        if (g_strcmp0 (color, "none") != 0 &&
            gdk_color_parse (color, &foreground))
            is_foreground_set = TRUE;
        g_free (color);
    }

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton_foreground),
                                  is_foreground_set);
    g_signal_connect (checkbutton_foreground, "toggled",
                      G_CALLBACK(on_foreground_toggled),
                      &context);
    gtk_widget_set_sensitive (context.colorbutton_foreground,
                              is_foreground_set);
    gtk_color_button_set_color
        (GTK_COLOR_BUTTON(context.colorbutton_foreground),
         &foreground);
    g_signal_connect (context.colorbutton_foreground, "color-set",
                      G_CALLBACK(on_foreground_color_set), &context);

    
    /* background color of pre-edit buffer */
    is_background_set = FALSE;
    color_to_gdk (PREEDIT_BACKGROUND, &background);
    if (ibus_m17n_config_get_string (config,
                                     context.section,
                                     "preedit_background",
                                     &color)) {
        if (g_strcmp0 (color, "none") != 0 &&
            gdk_color_parse (color, &background))
            is_background_set = TRUE;
        g_debug ("preedit_background %d", is_background_set);
        g_free (color);
    }
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton_background),
                                  is_background_set);
    g_signal_connect (checkbutton_background, "toggled",
                      G_CALLBACK(on_background_toggled),
                      &context);
    gtk_widget_set_sensitive (context.colorbutton_background,
                              is_background_set);
    gtk_color_button_set_color
        (GTK_COLOR_BUTTON(context.colorbutton_background),
         &background);
    g_signal_connect (context.colorbutton_background, "color-set",
                      G_CALLBACK(on_background_color_set), &context);

    /* underline of pre-edit buffer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT(combobox_underline),
                                renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT(combobox_underline),
                                    renderer, "text", 0, NULL);
    if (!ibus_m17n_config_get_int (config,
                                   context.section,
                                   "preedit_underline",
                                   &underline))
        underline = IBUS_ATTR_UNDERLINE_NONE;

    index = get_combo_box_index_by_value (GTK_COMBO_BOX(combobox_underline),
                                              underline);
    gtk_combo_box_set_active (GTK_COMBO_BOX(combobox_underline), index);
    g_signal_connect (combobox_underline, "changed",
                      G_CALLBACK(on_underline_changed), &context);

    /* General -> Other */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT(combobox_orientation),
                                renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT(combobox_orientation),
                                    renderer, "text", 0, NULL);
    if (!ibus_m17n_config_get_int (config,
                                   context.section,
                                   "lookup_table_orientation",
                                   &orientation))
        orientation = IBUS_ORIENTATION_SYSTEM;

    index = get_combo_box_index_by_value (GTK_COMBO_BOX(combobox_orientation),
                                          orientation);
    gtk_combo_box_set_active (GTK_COMBO_BOX(combobox_orientation), index);
    g_signal_connect (combobox_orientation, "changed",
                      G_CALLBACK(on_orientation_changed), &context);

    /* Advanced -> m17n-lib configuration */
    store = gtk_list_store_new (NUM_COLS,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_STRING);
    insert_items (store, msymbol (lang), msymbol (name));

    gtk_tree_view_set_model (GTK_TREE_VIEW(treeview), GTK_TREE_MODEL (store));
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1,
                                                 "Key",
                                                 gtk_cell_renderer_text_new (),
                                                 "text", COLUMN_KEY, NULL);
    g_object_set (treeview, "has-tooltip", TRUE, NULL);
    g_signal_connect (treeview, "query-tooltip", G_CALLBACK(on_query_tooltip),
                      NULL);

    context.language = msymbol (lang);
    context.name = msymbol (name);
    context.store = store;
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1,
                                                 "Value",
                                                 renderer,
                                                 "text", COLUMN_VALUE, NULL);
    g_object_set (renderer, "editable", TRUE, NULL);
    g_signal_connect (renderer, "edited", G_CALLBACK(on_edited), &context);

    gtk_widget_show_all (dialog);
    gtk_dialog_run (GTK_DIALOG(dialog));
}

int
main (gint argc, gchar **argv)
{
    GOptionContext *context;

    context = g_option_context_new ("ibus-setup-m17n");
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_parse (context, &argc, &argv, NULL);
    g_option_context_free (context);

    gtk_init (&argc, &argv);
    if (!opt_name) {
        fprintf (stderr, "can't determine IBus engine name; use --name\n");
        exit (1);
    }

    if (strncmp (opt_name, "m17n:", 5) != 0 ||
        strchr (&opt_name[5], ':') == NULL) {
        fprintf (stderr, "wrong format of IBus engine name\n");
        exit (1);
    }

    start (opt_name);

    return 0;
}
