#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"

#include "quickstreamGUI.h"


static GtkCssProvider *provider = 0;


void CleanupCSS(void) {

    if(provider) {
        g_object_unref(provider);
        provider = 0;
    }
}



void AddCSS(void) {

    provider = gtk_css_provider_new();
    DASSERT(provider);

    GError *error = 0;

    // TODO: Check for others themes using environment variables and/or
    // standard paths to theme files.
    //
    // TODO: This coding in of the colors/themes could back fire if a user
    // has some desktop themes that conflicts with some of these CSS
    // settings.  For example we set the background-color but a user theme
    // makes the font color not visible with that background-color.

    // We put the default theme in the code here so that if this works
    // without having to have other files installed/found.
    //
    // TODO: maybe install this data (css_str) in a file with a relative
    // path like bin/../lib/quickstream/misc/ and/or the GTK3 standard
    // theme paths.
    //
    // This is the default hard coded CSS theme shit:
    char *css_str =

"button#tab_close {\n"
"  padding: 0px;\n"
"  margin: 0px;\n"
"}\n"

// GNU radio version 3.8 colors:
//
// GNU radio block regular background-color is rgb(241,236,255)
//
// GNU radio block select border-color is rgb(241,236,255)
// GNU radio block select box color is    rgb(128,255,255)

// GNU radio block text color invalid block is rgb(248,46,5)
// GNU radio block text color valid block is   rgb(6,0,8)
//
// GNU radio stream line color is rgb(0,0,0)
// GNU radio stream line selected color is rgb(0,255,255)
//


"grid#block_regular, grid#block_selected, grid#block_hover {\n"
"  border: solid " BW_STR "px;\n"
"}\n"

// define DEBUG_BORDER to make very clearly different border colors
// for testing the event callbacks that set these grid colors via
// like for example:
//
//   gtk_widget_set_name(block->grid, "block_selected");
//
// These DEBUG border colors are obnoxious.  This is not for general use.
//
//#define DEBUG_BORDER // Too test with red, green, blue border colors


"grid#block_regular {\n"
"  background-color: rgba(241,236,255,0.66);\n"
#ifdef DEBUG_BORDER
"  border-color: rgb(255,0,0);\n" // test with red
#else
"  border-color: rgba(130,140,150, 0.34);\n"
#endif
"}\n"

"grid#block_hover {\n"
"  background-color: rgba(241,236,255,0.4);\n"
#ifdef DEBUG_BORDER
"  border-color: rgb(0,255,0);\n" // test with green
#else
"  border-color: rgba(116,250,225, 0.36);\n"
#endif
"}\n"

"grid#block_selected {\n"
"  background-color: rgba(41,236,255,0.76);\n"
#ifdef DEBUG_BORDER
"  border-color: rgb(0,0,255);\n" // test with blue
#else
"  border-color: rgba(88,206,241, 0.36);\n"
#endif
"}\n"


" label#name, label#icon {\n"
"  border: solid 1px;\n" 
"}\n"

" label#name, label#path, label#icon {\n"
"  color: rgba(40,40,40,0.5);\n"
"  border-color: rgba(40,40,40,0.2);\n"
"  background-color: rgba(200,228,200,0.5);\n"
"  font: 14px \"Sans\";\n"
"}\n"

"label#configBlockTitle {\n"
"  color: rgba(0,0,0,0.5);\n"
"  border-color: rgba(40,40,40,0.2);\n"
"  background-color: rgba(200,200,200,0.9);\n"
"  font: 16px \"Sans\";\n"
"}\n"

"entry#configBlockTitle {\n"
"  font: 18px \"Sans\";\n"
"}\n"

" label#path {\n"
"  border-top: solid 0px;\n" 
"  border-bottom: solid 0px;\n" 
"  border-left: solid 1px;\n"
"  border-right: solid 1px;\n"
"}\n"

" textview#configDesc text {\n"
"  color: rgba(0,0,0,.9);\n"
"  background-color: rgba(206,206,206,.99);\n"
"}\n"

" textview#configName text {\n"
"  color: rgba(0,0,0,.9);\n"
"  font: 18px \"Sans\";\n"
"  background-color: rgba(206,206,206,.99);\n"
"}\n"


" #buttonBox {\n"
"  background-color: rgba(190,170,170,0.3);\n"
"  margin: 10px;\n"
"  border: solid 2px;\n"
"  border-color: rgba(17,17,17,0.4);\n"
"  border-top-right-radius: 4px;\n"
"  border-top-left-radius: 4px;\n"
"  border-bottom-right-radius: 4px;\n"
"  border-bottom-left-radius: 4px;\n"
"}\n"


" #buttonBar {\n"
"  background-color: rgba(190,170,170,0.7);\n"
"  margin: 10px;\n"
"  border: solid 3px;\n"
"  border-color: rgba(170,17,170,0.8);\n"
"  border-top-right-radius: 10px;\n"
"  border-top-left-radius: 10px;\n"
"  border-bottom-right-radius: 10px;\n"
"  border-bottom-left-radius: 10px;\n"
"  font: 20px \"Sans\";\n"
"}\n"


" separator#config {\n"
"  background-color: rgb(170,170,170);\n"
"  border: solid 5px;\n"
"  border-color: rgb(170,170,170);\n"
"}\n"

" separator#config2 {\n"
"  background-color: rgb(206,206,206);\n"
"}\n"


" frame#configure_attributes {\n"
"  font: 20px \"Sans\";\n"
"  border-width: 4px;\n"
"  border: solid 10px;\n"
"  border-color: rgb(170,170,170);\n"
"}\n"

" grid#configure_attributes {\n"
"  font: 15px \"Sans\";\n"
"}\n"


#if 1
" treeview:hover {\n"
"  background-color: rgba(200,228,200,0.1);\n"
"}\n"
#endif


// This seems to do nothing:
//" layout#graph {\n"
//"  background-color: rgba(200,228,200,0.1);\n"
//"}\n"


" tooltip {\n"
"  font: 16px \"Sans\";\n"
"}\n"

" popover#port_info {\n"
"  font: 18px  \"Sans\";\n"
"  background-color: rgba(250,250,250,0.9);\n"
"  color: rgba(2,2,2,0.9);\n"
"}\n"

;


    gtk_css_provider_load_from_data(provider, css_str,
            strlen(css_str), &error);

    ASSERT(error == 0, "Failed to parse css:\n%s", css_str);


    gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_USER);
}
