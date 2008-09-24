/* vim: set backspace=2 ts=4 softtabstop=4 sw=4 cinoptions=>4 expandtab autoindent smartindent: */
/* xkb-cairo.c
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

#include <glib.h>
#include <gdk/gdk.h>
#include <cairo/cairo.h>
#include <librsvg/rsvg.h>
#include <pango/pangocairo.h>

#include "xkb-cairo.h"
#include "xkb-util.h"

#define XKB_PREFERRED_FONT "Courier New, Courier 10 Pitch, Monospace Bold %d"

#define xkb_cairo_arc_for_flag(cr, x, y, r, a1, a2) \
    xx = layoutx + width - 12 + x; \
    yy = layouty + height - 12 + y; \
    cairo_device_to_user (cr, &xx, &yy); \
    cairo_arc (cr, xx, yy, r, a1, a2);

#define xkb_cairo_arc_for_label(cr, x, y, r, a1, a2) xx = x; \
    yy = y; \
    cairo_device_to_user (cr, &xx, &yy); \
    cairo_arc (cr, xx, yy, r, a1, a2);  

#define xkb_cairo_move_to(cr, x, y) xx = x; \
    yy = y; \
    cairo_device_to_user (cr, &xx, &yy); \
    cairo_move_to (cr, xx, yy);

gint font_sizes[113] = {
    6,        6,        6,        6,        6,        6,        6,        6,
    7,        7,        8,        8,        8,        8,        8,        8,
    10,       10,       10,       10,       12,       12,       14,       14,
    14,       14,       14,       14,       16,       16,       16,       16,
    18,       18,       18,       18,       20,       20,       20,       20,
    20,       20,       22,       22,       22,       22,       24,       24,
    24,       24,       26,       26,       26,       26,       28,       28,
    28,       28,       30,       30,       30,       30,       30,       30,
    32,       32,       32,       32,       34,       34,       34,       34,
    34,       34,       36,       36,       36,       36,       36,       36,
    38,       38,       38,       38,       38,       38,       38,       38,
    40,       40,       40,       40,       40,       40,       42,       42,
    42,       42,       42,       42,       44,       44,       44,       44,
    44,       44,       46,       46,       46,       46,       46,       46,
    48
};

void
xkb_cairo_draw_flag (cairo_t *cr,
                     gchar *group_name,
                     gint panel_size,
                     gint actual_width,
                     gint actual_height,
                     gint width,
                     gint height,
                     gint variant_markers_count,
                     GdkColor fgcolor)
{
    g_assert (cr != NULL);

    gchar *filename;
    RsvgHandle *handle;
    RsvgDimensionData dim;
    double scalex, scaley;
    double xx, yy;
    gint i;
    double layoutx, layouty;

    filename = xkb_util_get_flag_filename (group_name);
    handle = rsvg_handle_new_from_file (filename, NULL);
    if (!handle)
    {
        xkb_cairo_draw_label (cr, group_name, 
                panel_size,
                actual_width, actual_height,
                width, height,
                variant_markers_count,
                fgcolor); 
        return;
    }

    rsvg_handle_get_dimensions (handle, &dim);

    scalex = (double) (width - 4) / dim.width;
    scaley = (double) (height - 4) / dim.height;

    if (handle)
    {
        layoutx = (actual_width - width) / 2 + 2;
        layouty = (actual_height - height) / 2 + 2;
        cairo_translate (cr, layoutx, layouty);

        //cairo_translate (cr, 2, 2);

        cairo_save (cr);

        cairo_scale (cr, scalex, scaley);
        rsvg_handle_render_cairo (handle, cr);

        cairo_restore (cr);

        /* draw variant_markers_count circles */
        for (i = 0; i < variant_markers_count; i++)
        {
            cairo_set_source_rgb (cr, 0, 0, 0);

            cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_line_width (cr, 1);

            xkb_cairo_arc_for_flag (cr, -(7 * i) + 4, 4, 2.5, 0, 2 * G_PI);

            cairo_set_source_rgb (cr, 0, 0, 0);
            cairo_fill_preserve (cr);
            cairo_set_source_rgb (cr, 1, 1, 1);
            cairo_stroke (cr);
        }

        g_object_unref (handle);
    }
}

void
xkb_cairo_draw_label (cairo_t *cr,
                      gchar *group_name,
                      gint panel_size,
                      gint actual_width,
                      gint actual_height,
                      gint width,
                      gint height,
                      gint variant_markers_count,
                      GdkColor fgcolor)
{
    g_assert (cr != NULL);

    gchar **normalized_group_name;
    gchar font_str[80];
    gint pango_width, pango_height;
    gint layoutx, layouty;
    double xx, yy;
    gint i;
    gint radius;

    normalized_group_name = xkb_util_normalize_group_name (group_name);

    PangoLayout *layout;
    PangoFontDescription *desc;
    layout = pango_cairo_create_layout (cr);

    pango_layout_set_text (layout, normalized_group_name, -1);
    g_sprintf (font_str, XKB_PREFERRED_FONT, font_sizes[panel_size - 16]);
    desc = pango_font_description_from_string (font_str);
    pango_layout_set_font_description (layout, desc);
    pango_font_description_free (desc);

    gdk_cairo_set_source_color (cr, &fgcolor);
    pango_layout_get_pixel_size (layout, &pango_width, &pango_height);

    layoutx = (int) (actual_width - (pango_width + variant_markers_count * 7)) / 2;
    layouty = (int) (actual_height - pango_height) / 2;

    xkb_cairo_move_to (cr, layoutx, layouty);
    pango_cairo_show_layout (cr, layout);

    for (i = 0; i < variant_markers_count; i++)
    {
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_width (cr, 1);
        radius = (panel_size < 32) ? 1.5 : 2.5;
        xkb_cairo_arc_for_label (cr, 
                layoutx + pango_width + 3 + (7 * i), 
                layouty + pango_height - (pango_height / 5), 
                radius, 0, 2 * G_PI
        );
        cairo_fill (cr);
    }

    g_free (normalized_group_name);
    g_object_unref (layout);
}

