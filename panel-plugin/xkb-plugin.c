/* vim: set backspace=2 ts=4 softtabstop=4 sw=4 cinoptions=>4 expandtab autoindent smartindent: */
/* xkb-plugin.c
 * Copyright (C) 2008 Alexander Iliev <sasoiliev@mamul.org>
 *
 * Parts of this program comes from the XfKC tool:
 * Copyright (C) 2006 Gauvain Pocentek <gauvainpocentek@gmail.com>
 *
 * A part of this file comes from the gnome keyboard capplet (control-center):
 * Copyright (C) 2003 Sergey V. Oudaltsov <svu@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4ui/libxfce4ui.h>
#include <libwnck/libwnck.h>
#include <librsvg/rsvg.h>
#include <garcon/garcon.h>

#include "xkb-plugin.h"
#include "xkb-properties.h"
#include "xkb-keyboard.h"
#include "xkb-dialog.h"
#include "xkb-cairo.h"

typedef struct
{
    XkbPlugin *plugin;
    gint group;
} MenuItemData;

struct _XkbPluginClass
{
    XfcePanelPluginClass __parent__;
};

struct _XkbPlugin
{
    XfcePanelPlugin __parent__;
    
    XkbXfconf *config;
    XkbKeyboard *keyboard;

    /* widgets */
    GtkWidget *btn;
    GtkWidget *layout_image;
    GtkWidget *popup;
    MenuItemData *popup_user_data;
};

/* ------------------------------------------------------------------ *
 *                     Panel Plugin Interface                         *
 * ------------------------------------------------------------------ */

static void         xkb_plugin_construct                (XfcePanelPlugin *plugin);
static void         xkb_plugin_orientation_changed      (XfcePanelPlugin *plugin,
                                                         GtkOrientation orientation);
static gboolean     xkb_plugin_size_changed             (XfcePanelPlugin *plugin,
                                                         gint size);
static void         xkb_plugin_free_data                (XfcePanelPlugin *plugin);
static void         xkb_plugin_about_show               (XfcePanelPlugin *plugin);
static void         xkb_plugin_configure_plugin         (XfcePanelPlugin *plugin);

/* ----------------------------------------------------------------- *
 *                           XKB Stuff                               *
 * ----------------------------------------------------------------- */

static void         xkb_plugin_state_changed            (gint current_group,
                                                         gboolean config_changed,
                                                         gpointer user_data);

static gboolean     xkb_plugin_calculate_sizes          (XkbPlugin *plugin,
                                                         GtkOrientation orientation,
                                                         gint panel_size);

static void         xkb_plugin_popup_menu_populate      (XkbPlugin *plugin);
static void         xkb_plugin_popup_menu_destroy       (XkbPlugin *plugin);
static void         xkb_plugin_popup_menu_show          (GtkButton *btn,
                                                         GdkEventButton *event,
                                                         XkbPlugin *plugin);
static void         xkb_plugin_popup_menu_deactivate    (gpointer data,
                                                         GtkMenuShell *menu_shell);

static void         xkb_plugin_refresh_gui              (XkbPlugin *plugin);

static void         xkb_plugin_configure_layout         (GtkWidget *widget,
                                                         gpointer user_data);

static gboolean     xkb_plugin_button_clicked           (GtkButton *btn,
                                                         GdkEventButton *event,
                                                         gpointer data);
static gboolean     xkb_plugin_button_scrolled          (GtkWidget *btn,
                                                         GdkEventScroll *event,
                                                         gpointer data);

static gboolean     xkb_plugin_set_tooltip              (GtkWidget *widget,
                                                         gint x,
                                                         gint y,
                                                         gboolean keyboard_mode,
                                                         GtkTooltip *tooltip,
                                                         XkbPlugin *plugin);

static gboolean     xkb_plugin_layout_image_draw        (GtkWidget *widget,
                                                         cairo_t *cr,
                                                         XkbPlugin *plugin);

static void         xkb_plugin_active_window_changed   (WnckScreen *screen,
                                                        WnckWindow *previously_active_window,
                                                        XkbPlugin *plugin);
static void         xkb_plugin_application_closed      (WnckScreen *screen,
                                                        WnckApplication *app,
                                                        XkbPlugin *plugin);
static void         xkb_plugin_window_closed           (WnckScreen *screen,
                                                        WnckWindow *window,
                                                        XkbPlugin *plugin);

static void         xkb_plugin_display_type_changed     (XkbPlugin *plugin);
static void         xkb_plugin_display_scale_changed    (XkbPlugin *plugin);
static void         xkb_plugin_group_policy_changed     (XkbPlugin *plugin);

/* ================================================================== *
 *                        Implementation                              *
 * ================================================================== */

XFCE_PANEL_DEFINE_PLUGIN (XkbPlugin, xkb_plugin)

static void
xkb_plugin_class_init (XkbPluginClass *klass)
{
    XfcePanelPluginClass *plugin_class;

    plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
    plugin_class->construct = xkb_plugin_construct;
    plugin_class->free_data = xkb_plugin_free_data;
    plugin_class->about = xkb_plugin_about_show;
    plugin_class->configure_plugin = xkb_plugin_configure_plugin;
    plugin_class->orientation_changed = xkb_plugin_orientation_changed;
    plugin_class->size_changed = xkb_plugin_size_changed;
}

static void
xkb_plugin_init (XkbPlugin *plugin)
{
    plugin->config = NULL;
    plugin->keyboard = NULL;

    plugin->btn = NULL;
    plugin->layout_image = NULL;
    plugin->popup = NULL;
    plugin->popup_user_data = NULL;
}

static void
xkb_plugin_construct (XfcePanelPlugin *plugin)
{
    XkbPlugin *xkb_plugin;
    GtkWidget *configure_layouts;
    WnckScreen *wnck_screen;
    GtkCssProvider *css_provider;

    xkb_plugin = XKB_PLUGIN (plugin);

    xkb_plugin->config = xkb_xfconf_new (xfce_panel_plugin_get_property_base (plugin));
    g_signal_connect_swapped (G_OBJECT (xkb_plugin->config), "notify::" DISPLAY_TYPE,
            G_CALLBACK (xkb_plugin_display_type_changed), xkb_plugin);
    g_signal_connect_swapped (G_OBJECT (xkb_plugin->config), "notify::" DISPLAY_SCALE,
            G_CALLBACK (xkb_plugin_display_scale_changed), xkb_plugin);
    g_signal_connect_swapped (G_OBJECT (xkb_plugin->config), "notify::" GROUP_POLICY,
            G_CALLBACK (xkb_plugin_group_policy_changed), xkb_plugin);

    xkb_plugin->btn = gtk_button_new ();
    gtk_button_set_relief (GTK_BUTTON (xkb_plugin->btn), GTK_RELIEF_NONE);
    gtk_container_add (GTK_CONTAINER (plugin), xkb_plugin->btn);
    xfce_panel_plugin_add_action_widget (plugin, xkb_plugin->btn);
    gtk_widget_add_events (xkb_plugin->btn, GDK_SCROLL_MASK);

    /* remove padding inside button */
    css_provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (css_provider, ".xfce4-panel button {padding: 0;}", -1, NULL);
    gtk_style_context_add_provider (GTK_STYLE_CONTEXT (gtk_widget_get_style_context (GTK_WIDGET (xkb_plugin->btn))),
            GTK_STYLE_PROVIDER (css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref (css_provider);

    gtk_widget_show (xkb_plugin->btn);
    g_signal_connect (xkb_plugin->btn, "button-press-event",
            G_CALLBACK (xkb_plugin_button_clicked), xkb_plugin);
    g_signal_connect (xkb_plugin->btn, "button-release-event",
            G_CALLBACK (xkb_plugin_button_clicked), xkb_plugin);
    g_signal_connect (xkb_plugin->btn, "scroll-event",
            G_CALLBACK (xkb_plugin_button_scrolled), xkb_plugin);

    g_object_set (G_OBJECT (xkb_plugin->btn), "has-tooltip", TRUE, NULL);
    g_signal_connect (xkb_plugin->btn, "query-tooltip",
            G_CALLBACK (xkb_plugin_set_tooltip), xkb_plugin);

    xkb_plugin->layout_image = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (xkb_plugin->btn), xkb_plugin->layout_image);
    g_signal_connect (G_OBJECT (xkb_plugin->layout_image), "draw",
            G_CALLBACK (xkb_plugin_layout_image_draw), xkb_plugin);
    gtk_widget_show (GTK_WIDGET (xkb_plugin->layout_image));

    xkb_plugin->keyboard = xkb_keyboard_new (xkb_xfconf_get_group_policy (xkb_plugin->config),
            xkb_plugin_state_changed, xkb_plugin);

    if (xkb_keyboard_get_initialized (xkb_plugin->keyboard))
    {
        xkb_plugin_refresh_gui (xkb_plugin);
        xkb_plugin_popup_menu_populate (xkb_plugin);
    }

    wnck_screen = wnck_screen_get_default ();
    g_signal_connect (G_OBJECT (wnck_screen), "active-window-changed",
            G_CALLBACK (xkb_plugin_active_window_changed), xkb_plugin);
    g_signal_connect (G_OBJECT (wnck_screen), "window-closed",
            G_CALLBACK (xkb_plugin_window_closed), xkb_plugin);
    g_signal_connect (G_OBJECT (wnck_screen), "application-closed",
            G_CALLBACK (xkb_plugin_application_closed), xkb_plugin);

    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    xfce_panel_plugin_menu_show_configure (plugin);
    xfce_panel_plugin_menu_show_about (plugin);

    xfce_panel_plugin_set_small (plugin, TRUE);

    configure_layouts = gtk_menu_item_new_with_label (_("Keyboard settings"));
    gtk_widget_show (configure_layouts);
    xfce_panel_plugin_menu_insert_item (plugin, GTK_MENU_ITEM (configure_layouts));

    g_signal_connect (G_OBJECT (configure_layouts), "activate",
            G_CALLBACK (xkb_plugin_configure_layout), NULL);
}

static void
xkb_plugin_orientation_changed (XfcePanelPlugin *plugin,
                                GtkOrientation orientation)
{
    xkb_plugin_calculate_sizes (XKB_PLUGIN (plugin),
            orientation, xfce_panel_plugin_get_size (plugin));
}

static gboolean
xkb_plugin_size_changed (XfcePanelPlugin *plugin,
                         gint size)
{
    return xkb_plugin_calculate_sizes (XKB_PLUGIN (plugin),
            xfce_panel_plugin_get_orientation (plugin), size);
}

static void
xkb_plugin_free_data (XfcePanelPlugin *plugin)
{
    XkbPlugin *xkb_plugin = XKB_PLUGIN (plugin);

    xkb_plugin_popup_menu_destroy (xkb_plugin);
    gtk_widget_destroy (xkb_plugin->layout_image);
    gtk_widget_destroy (xkb_plugin->btn);

    g_object_unref (G_OBJECT (xkb_plugin->keyboard));
    g_object_unref (G_OBJECT (xkb_plugin->config));
}

static void
xkb_plugin_about_show (XfcePanelPlugin *plugin)
{
    xkb_dialog_about_show ();
}

static void
xkb_plugin_configure_plugin (XfcePanelPlugin *plugin)
{
    XkbPlugin *xkb_plugin = XKB_PLUGIN (plugin);

    xkb_dialog_configure_plugin (plugin, xkb_plugin->config);
}

/* ----------------- xkb plugin stuff -----------------------*/

static void
xkb_plugin_state_changed (gint current_group,
                          gboolean config_changed,
                          gpointer user_data)
{
    XkbPlugin *plugin = user_data;

    xkb_plugin_refresh_gui (plugin);

    if (config_changed)
    {
        xkb_plugin_popup_menu_populate (plugin);
    }
}

static void
xkb_plugin_set_group (GtkMenuItem *item,
                      gpointer data)
{
    MenuItemData *item_data = data;
    xkb_keyboard_set_group (item_data->plugin->keyboard, item_data->group);
}

static gboolean
xkb_plugin_calculate_sizes (XkbPlugin *plugin,
                            GtkOrientation orientation,
                            gint panel_size)
{
    guint nrows;
    gint hsize, vsize;
    gboolean proportional;
    XkbDisplayType display_type;

    display_type = xkb_xfconf_get_display_type (plugin->config);
    nrows = xfce_panel_plugin_get_nrows (XFCE_PANEL_PLUGIN (plugin));
    panel_size /= nrows;
    proportional = nrows > 1 || display_type == DISPLAY_TYPE_SYSTEM;
    TRACE ("calculate_sizes(%p: %d,%d)", plugin, panel_size, nrows);

    switch (orientation)
    {
        case GTK_ORIENTATION_HORIZONTAL:
            vsize = panel_size;
            if (proportional)
            {
                hsize = panel_size;
            }
            else
            {
                hsize = (int) (1.33 * panel_size);
            }

            gtk_widget_set_size_request (plugin->btn, hsize, vsize);
            break;
        case GTK_ORIENTATION_VERTICAL:
            hsize = panel_size;
            if (proportional)
            {
                vsize = panel_size;
            }
            else
            {
                vsize = (int) (0.75 * panel_size);
            }
            if (vsize < 10) vsize = 10;

            gtk_widget_set_size_request (plugin->btn, hsize, vsize);
            break;
        default:
            break;
    }

    DBG ("size requested: h/v (%p: %d/%d), proportional: %d",
            plugin, hsize, vsize, proportional);

    xkb_plugin_refresh_gui (plugin);
    return TRUE;
}

static void
xkb_plugin_popup_menu_destroy (XkbPlugin *plugin)
{
    if (plugin->popup != NULL)
    {
        gtk_menu_popdown (GTK_MENU (plugin->popup));
        gtk_menu_detach (GTK_MENU (plugin->popup));
        g_free (plugin->popup_user_data);
        plugin->popup_user_data = NULL;
        plugin->popup = NULL;
    }
}

static void
xkb_plugin_popup_menu_populate (XkbPlugin *plugin)
{
    gint i, group_count;
    gchar *layout_string;
    GtkWidget *menu_item;

    if (G_UNLIKELY (plugin == NULL)) return;

    group_count = xkb_keyboard_get_group_count (plugin->keyboard);

    xkb_plugin_popup_menu_destroy (plugin);
    plugin->popup = gtk_menu_new ();
    plugin->popup_user_data = g_new0 (MenuItemData, group_count);

    for (i = 0; i < group_count; i++)
    {
        layout_string = xkb_keyboard_get_pretty_layout_name (plugin->keyboard, i);

        menu_item = gtk_menu_item_new_with_label (layout_string);

        plugin->popup_user_data[i].plugin = plugin;
        plugin->popup_user_data[i].group = i;

        g_signal_connect (G_OBJECT (menu_item), "activate",
                G_CALLBACK (xkb_plugin_set_group), &plugin->popup_user_data[i]);

        gtk_widget_show (menu_item);
        gtk_menu_shell_append (GTK_MENU_SHELL (plugin->popup), menu_item);
    }

    g_signal_connect_swapped (GTK_MENU_SHELL (plugin->popup), "deactivate",
            G_CALLBACK (xkb_plugin_popup_menu_deactivate), plugin);

    gtk_menu_attach_to_widget (GTK_MENU (plugin->popup), plugin->btn, NULL);
}

static void
xkb_plugin_popup_menu_show (GtkButton *btn,
                            GdkEventButton *event,
                            XkbPlugin *plugin)
{
    gtk_widget_set_state_flags (GTK_WIDGET (btn), GTK_STATE_FLAG_CHECKED, FALSE);
#if GTK_CHECK_VERSION(3, 22, 0)
    gtk_menu_popup_at_widget (GTK_MENU (plugin->popup), GTK_WIDGET (btn),
            GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST, (GdkEvent *) event);
#else
    gtk_menu_popup (GTK_MENU (plugin->popup), NULL, NULL,
            xfce_panel_plugin_position_menu, xkb_plugin_get_plugin (plugin),
            0, event->time);
#endif
}

static void
xkb_plugin_popup_menu_deactivate (gpointer data,
                                  GtkMenuShell *menu_shell)
{
    XkbPlugin *plugin = data;

    g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

    gtk_widget_unset_state_flags (GTK_WIDGET (plugin->btn), GTK_STATE_FLAG_CHECKED);
}

static void
xkb_plugin_refresh_gui (XkbPlugin *plugin)
{
    GdkDisplay * display;
    GtkAllocation allocation;

    gtk_widget_get_allocation (GTK_WIDGET (plugin->btn), &allocation);

    /* Part of the image may remain visible after display type change */
    gtk_widget_queue_draw_area (plugin->btn, 0, 0,
            allocation.width, allocation.height);

    display = gdk_display_get_default();
    if (display)
    {
        gtk_tooltip_trigger_tooltip_query(display);
    }
}

static void
xkb_plugin_configure_layout (GtkWidget *widget,
                             gpointer user_data)
{
    gchar *desktop_file = xfce_resource_lookup (XFCE_RESOURCE_DATA,
                                 "applications/xfce-keyboard-settings.desktop");

    GarconMenuItem *item = garcon_menu_item_new_for_path (desktop_file);
    if (item)
    {
          GError  *error = NULL;
          gchar  **argv;
          gboolean succeed;
          g_shell_parse_argv (garcon_menu_item_get_command (item), NULL, &argv, &error);
          succeed = xfce_spawn_on_screen (gtk_widget_get_screen (GTK_WIDGET (widget)),
                                garcon_menu_item_get_path (item),
                                argv, NULL, G_SPAWN_SEARCH_PATH,
                                garcon_menu_item_supports_startup_notification (item),
                                gtk_get_current_event_time (),
                                garcon_menu_item_get_icon_name (item),
                                &error);
          g_strfreev (argv);
          garcon_menu_item_unref (item);
          g_assert (succeed);

    }
    g_free (desktop_file);
}

static gboolean
xkb_plugin_button_clicked (GtkButton *btn,
                           GdkEventButton *event,
                           gpointer data)
{
    XkbPlugin *plugin;
    gboolean released, display_popup;

    if (event->button == 1)
    {
        plugin = data;

        released = event->type == GDK_BUTTON_RELEASE;
        display_popup = xkb_keyboard_get_group_count (plugin->keyboard) > 2;

        if (display_popup && !released)
        {
            xkb_plugin_popup_menu_show (btn, event, data);
            return TRUE;
        }

        if (!display_popup && released)
        {
            xkb_keyboard_next_group (plugin->keyboard);
            return FALSE;
        }
    }
    return FALSE;
}

static gboolean
xkb_plugin_button_scrolled (GtkWidget *btn,
                            GdkEventScroll *event,
                            gpointer data)
{
    XkbPlugin *plugin = data;

    switch (event->direction)
    {
      case GDK_SCROLL_UP:
      case GDK_SCROLL_RIGHT:
          xkb_keyboard_next_group (plugin->keyboard);
          return TRUE;
      case GDK_SCROLL_DOWN:
      case GDK_SCROLL_LEFT:
          xkb_keyboard_prev_group (plugin->keyboard);
          return TRUE;
      default:
        return FALSE;
    }

    return FALSE;
}

static gboolean
xkb_plugin_set_tooltip (GtkWidget *widget,
                        gint x,
                        gint y,
                        gboolean keyboard_mode,
                        GtkTooltip *tooltip,
                        XkbPlugin *plugin)
{
    gint group;
    gchar *layout_name;
    GdkPixbuf *pixbuf;

    group = xkb_keyboard_get_current_group (plugin->keyboard);

    if (xkb_xfconf_get_display_tooltip_icon (plugin->config))
    {
        pixbuf = xkb_keyboard_get_tooltip_pixbuf (plugin->keyboard, group);
        gtk_tooltip_set_icon (tooltip, pixbuf);
    }

    layout_name = xkb_keyboard_get_pretty_layout_name (plugin->keyboard, group);

    gtk_tooltip_set_text (tooltip, layout_name);

    return TRUE;
}

static gboolean
xkb_plugin_layout_image_draw (GtkWidget *widget,
                              cairo_t *cr,
                              XkbPlugin *plugin)
{
    const gchar *group_name;
    GtkAllocation allocation;
    GtkStyleContext *style_ctx;
    GtkStateFlags state;
    PangoFontDescription *desc;
    GdkRGBA rgba;
    gint actual_hsize, actual_vsize;
    XkbDisplayType display_type;
    gint display_scale;

    display_type = xkb_xfconf_get_display_type (plugin->config);
    display_scale = xkb_xfconf_get_display_scale (plugin->config);

    gtk_widget_get_allocation (GTK_WIDGET (widget), &allocation);
    actual_hsize = allocation.width;
    actual_vsize = allocation.height;

    state = gtk_widget_get_state_flags (GTK_WIDGET (plugin->btn));
    style_ctx = gtk_widget_get_style_context (GTK_WIDGET (plugin->btn));
    gtk_style_context_get_color (style_ctx, state, &rgba);
    group_name = xkb_keyboard_get_group_name (plugin->keyboard, -1);

    DBG ("img_exposed: actual h/v (%d/%d)",
         actual_hsize, actual_vsize);

    switch (display_type)
    {
        case DISPLAY_TYPE_IMAGE:
            xkb_cairo_draw_flag (cr, group_name,
                    actual_hsize, actual_vsize,
                    xkb_keyboard_variant_index_for_group (plugin->keyboard, -1),
                    xkb_keyboard_get_max_group_count (plugin->keyboard),
                    display_scale,
                    rgba
            );
            break;
        case DISPLAY_TYPE_TEXT:
            xkb_cairo_draw_label (cr, group_name,
                    actual_hsize, actual_vsize,
                    xkb_keyboard_variant_index_for_group (plugin->keyboard, -1),
                    display_scale,
                    rgba
            );
            break;
        case DISPLAY_TYPE_SYSTEM:
            gtk_style_context_get (style_ctx, state, "font", &desc, NULL);
            xkb_cairo_draw_label_system (cr, group_name,
                    actual_hsize, actual_vsize,
                    xkb_keyboard_variant_index_for_group (plugin->keyboard, -1),
                    desc, rgba
            );
            break;
    }

    return FALSE;
}

static void
xkb_plugin_active_window_changed (WnckScreen *screen,
                                  WnckWindow *previously_active_window,
                                  XkbPlugin *plugin)
{
    WnckWindow *window;
    guint window_id, application_id;

    window = wnck_screen_get_active_window (screen);
    if (!WNCK_IS_WINDOW (window)) return;
    window_id = wnck_window_get_xid (window);
    application_id = wnck_window_get_pid (window);

    xkb_keyboard_window_changed (plugin->keyboard, window_id, application_id);
}

static void
xkb_plugin_application_closed (WnckScreen *screen,
                               WnckApplication *app,
                               XkbPlugin *plugin)
{
    guint application_id;

    application_id = wnck_application_get_pid (app);

    xkb_keyboard_application_closed (plugin->keyboard, application_id);
}

static void
xkb_plugin_window_closed (WnckScreen *screen,
                          WnckWindow *window,
                          XkbPlugin *plugin)
{
    guint window_id;

    window_id = wnck_window_get_xid (window);

    xkb_keyboard_window_closed (plugin->keyboard, window_id);
}

static void
xkb_plugin_display_type_changed (XkbPlugin *plugin)
{
    xkb_plugin_calculate_sizes (plugin,
            xfce_panel_plugin_get_orientation (XFCE_PANEL_PLUGIN (plugin)),
            xfce_panel_plugin_get_size (XFCE_PANEL_PLUGIN (plugin)));
}

static void
xkb_plugin_display_scale_changed (XkbPlugin *plugin)
{
    xkb_plugin_refresh_gui (plugin);
}

static void
xkb_plugin_group_policy_changed (XkbPlugin *plugin)
{
    xkb_keyboard_set_group_policy (plugin->keyboard,
            xkb_xfconf_get_group_policy (plugin->config));
}
