#include <stdbool.h>

#include <gtk/gtk.h>

#include "qsb.h"



void InitCSS() {

    GtkCssProvider *provider;

    provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(
            gdk_display_get_default_screen(gdk_display_get_default()),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GError *error = 0;

    gtk_css_provider_load_from_data(provider,
        /* The CSS stuff in GTK3 version 3.24.20 does not work as
         * documented.  All the example codes that I found did not work.
         * I expect this is a moving target, and this will break in the
         * future.
         */
            // This CSS works for GTK3 version 3.24.20
            //
            // #block > #foo
            //    is for child of parent named "block" with name "foo"
            //
            // Clearly widget name is not a unique widget ID; it's more
            // like a CSS class name then a name.  The documentation is
            // miss-leading on this.  This is not consistent with regular
            // www3 CSS.  We can probably count on this breaking in new
            // versions of GTK3 or maybe not.
            //
            // This took 3 days of trial and error.  Yes: GTK3 sucks;
            // maybe a little less than QT.
            //
            // TODO: It'd be nice to put this in gtk builder files,
            // and compiling it into the code like the .ui widgets.
            // There currently does not appear to be a way to do this.
            "#block {\n"
                "background-color: rgba(83,134,240,0.3);\n"
                "border: 0px solid rgba(118,162,247,0.3);\n"
                "}\n"
            "#disabledBlock {\n"
                "background-color: rgba(83,138,250,0.1);\n"
                "border: 0px solid rgba(118,162,247,0.3);\n"
                "}\n"
            "#selectedBlock {\n"
                "background-color: rgba(83,138,250,0.0);\n"
                "border: 4px solid rgba(234,12,234,0.4);\n"
                "}\n"

            "#getters {\n"
                "background-color: rgba(232,211,22,0.5);\n"
                "}\n"
            "#setters {\n"
                "background-color: rgba(22,100,232,0.5);\n"
                "}\n"
            "#constants {\n"
                "background-color: rgba(22,232,43,0.5);\n"
                "}\n"
            "#input {\n"
                "background-color: rgba(232,74,22,0.5);\n"
                "}\n"
            "#output {\n"
                "background-color: rgba(232,74,22,0.5);\n"
                "}\n"

            "#getters {\n"
                "color: rgba(139,136,65,0.6);\n"
                "}\n"
            "#setters {\n"
                "color: rgba(36,74,139,0.6);\n"
                "}\n"
            "#constants {\n"
                "color: rgba(42,107,62,0.6);\n"
                "}\n"
            "#input {\n"
                "color: rgba(129,75,67,0.6);\n"
                "}\n"
            "#output {\n"
                "color: rgba(129,76,67,0.6);\n"
                "}\n"

            "#path,\n"
            "#name {\n"
                "color: black;\n"
            "}\n"
            "#path {\n"
                "font-size: 90%;\n"
                "}\n"
            "#name {\n"
                "font-size: 120%;\n"
                "}\n"
            , -1, &error);

    g_object_unref (provider);
}
