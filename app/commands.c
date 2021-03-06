/* Dia -- an diagram creation/manipulation program
 * Copyright (C) 1998 Alexander Larsson *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>
#include <glib.h>

#ifdef GNOME
#undef GTK_DISABLE_DEPRECATED
#  include <gnome.h> 	 
#endif 	 

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

#include <textedit.h>
#include <focus.h>
#include "confirm.h"

/** Functions called on menu selects.
 *  Note that GTK (at least up to 2.12) doesn't disable the keyboard shortcuts
 *  when the menu is made insensitive, so we have to check the constrains again
 *  in the functions.
 */
#ifdef G_OS_WIN32
/*
 * Instead of polluting the Dia namespace with windoze headers, declare the
 * required prototype here. This is bad style, but not as bad as namespace
 * clashes to be resolved without C++   --hb
 */
long __stdcall
ShellExecuteA (long        hwnd,
               const char* lpOperation,
               const char* lpFile,
               const char* lpParameters,
               const char* lpDirectory,
               int         nShowCmd);
#endif

#include "intl.h"
#include "commands.h"
#include "app_procs.h"
#include "diagram.h"
#include "display.h"
#include "object_ops.h"
#include "cut_n_paste.h"
#include "interface.h"
#include "load_save.h"
#include "utils.h"
#include "message.h"
#include "grid.h"
#include "properties-dialog.h"
#include "propinternals.h"
#include "preferences.h"
#include "layer_dialog.h"
#include "dic_dialog.h"
#include "connectionpoint_ops.h"
#include "undo.h"
#include "pagesetup.h"
#include "text.h"
#include "dia_dirs.h"
#include "focus.h"
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include "lib/properties.h"
#include "lib/parent.h"
#include "dia-props.h"
#include "diagram_tree_window.h"
#include "authors.h"                /* master contributors data */

void 
file_quit_callback (GtkAction *action)
{
  app_exit();
}

void
file_pagesetup_callback (GtkAction *action)
{
  Diagram *dia;

  dia = ddisplay_active_diagram();
  if (!dia) return;
  create_page_setup_dlg(dia);
}

void
file_print_callback (GtkAction *_action)
{
  Diagram *dia;
  DDisplay *ddisp;
  GtkAction *action;

  dia = ddisplay_active_diagram();
  if (!dia) return;
  ddisp = ddisplay_active();
  if (!ddisp) return;
  
  action = menus_get_action ("FilePrintGTK");
  if (!action)
    action = menus_get_action ("FilePrintGDI");
  if (!action)
    action = menus_get_action ("FilePrintPS");

  if (action) {
    if (confirm_export_size (dia, GTK_WINDOW(ddisp->shell), CONFIRM_PRINT|CONFIRM_PAGES))
      gtk_action_activate (action);
  } else {
    message_error (_("No print plug-in found!"));
  }
}

void
file_close_callback (GtkAction *action)
{
  /* some people use tear-off menus and insist to close non existing displays */
  if (ddisplay_active())
    ddisplay_close(ddisplay_active());
}

void
file_new_callback (GtkAction *action)
{
  Diagram *dia;
  DDisplay *ddisp;
  static int untitled_nr = 1;
  gchar *name, *filename;

  name = g_strdup_printf("Diagram%d.red", untitled_nr++);
  filename = g_filename_from_utf8(name, -1, NULL, NULL, NULL);
  dia = new_diagram(filename);
  ddisp = new_display(dia);
  diagram_tree_add(diagram_tree(), dia);
  g_free (name);
  g_free (filename);
}

void
file_preferences_callback (GtkAction *action)
{
  prefs_show();
}



/* Signal handler for getting the clipboard contents */
/* Note that the clipboard is for M$-style cut/copy/paste copying, while
   the selection is for Unix-style mark-and-copy.  We can't really do
   mark-and-copy.
*/

static void
insert_text(DDisplay *ddisp, Focus *focus, const gchar *text)
{
  ObjectChange *change = NULL;
  int modified = FALSE, any_modified = FALSE;
  DiaObject *obj = focus_get_object(focus);

  while (text != NULL) {
    gchar *next_line = g_utf8_strchr(text, -1, '\n');
    if (next_line != text) {
      gint len = g_utf8_strlen(text, (next_line-text));
      modified = (*focus->key_event)(focus, GDK_A, text, len, &change);
    }
    if (next_line != NULL) {
      modified = (*focus->key_event)(focus, GDK_Return, "\n", 1, &change);
      text = g_utf8_next_char(next_line);
    } else {
      text = NULL;
    }
    { /* Make sure object updates its data: */
      Point p = obj->position;
      (obj->ops->move)(obj,&p);  }

    /* Perhaps this can be improved */
    object_add_updates(obj, ddisp->diagram);
    
    if (modified && (change != NULL)) {
      undo_object_change(ddisp->diagram, obj, change);
      any_modified = TRUE;
    }
    
    diagram_flush(ddisp->diagram);
  }

  if (any_modified) {
    diagram_modified(ddisp->diagram);
    undo_set_transactionpoint(ddisp->diagram->undo);
  }
}


static void
received_clipboard_handler(GtkClipboard *clipboard, 
			   const gchar *text,
			   gpointer data) {
  DDisplay *ddisp = (DDisplay *)data;
  Focus *focus = get_active_focus((DiagramData *) ddisp->diagram);
  
  if (text == NULL) return;

  if ((focus == NULL) || (!focus->has_focus)) return;

  if (!g_utf8_validate(text, -1, NULL)) {
    message_error("Not valid UTF8");
    return;
  }

  insert_text(ddisp, focus, text);
}

static PropDescription text_prop_singleton_desc[] = {
    { "text", PROP_TYPE_TEXT },
    PROP_DESC_END};

static void 
make_text_prop_singleton(GPtrArray **props, TextProperty **prop) 
{
  *props = prop_list_from_descs(text_prop_singleton_desc,pdtpp_true);
  g_assert((*props)->len == 1);

  *prop = g_ptr_array_index((*props),0);
  g_free((*prop)->text_data);
  (*prop)->text_data = NULL;
}


void
edit_copy_callback (GtkAction *action)
{
  GList *copy_list;
  DDisplay *ddisp;

  ddisp = ddisplay_active();
  if (!ddisp) return;
  if (textedit_mode(ddisp)) {
    Focus *focus = get_active_focus((DiagramData *) ddisp->diagram);
    DiaObject *obj = focus_get_object(focus);
    GPtrArray *textprops;
    TextProperty *prop;
    
    if (obj->ops->get_props == NULL) 
      return;
    
    make_text_prop_singleton(&textprops,&prop);
    /* Get the first text property */
    obj->ops->get_props(obj, textprops);
    
    /* GTK docs claim the selection clipboard is ignored on Win32.
     * The "clipboard" clipboard is mostly ignored in Unix
     */
#ifdef G_OS_WIN32
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_NONE),
			   prop->text_data, -1);
#else
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY),
			   prop->text_data, -1);
#endif
    prop_list_free(textprops);
  } else {
    copy_list = parent_list_affected(diagram_get_sorted_selected(ddisp->diagram));
    
    cnp_store_objects(object_copy_list(copy_list), 1, ddisp);
    g_list_free(copy_list);
    
    ddisplay_do_update_menu_sensitivity(ddisp);
  }
}

void
edit_cut_callback (GtkAction *action)
{
  GList *cut_list;
  DDisplay *ddisp;
  Change *change;

  ddisp = ddisplay_active();
  if (!ddisp) return;

  if (textedit_mode(ddisp)) {
  } else {
    diagram_selected_break_external(ddisp->diagram);

    cut_list = parent_list_affected(diagram_get_sorted_selected(ddisp->diagram));

    cnp_store_objects(object_copy_list(cut_list), 0, ddisp);

    change = undo_delete_objects_children(ddisp->diagram, cut_list);
    (change->apply)(change, ddisp->diagram);
  
    ddisplay_do_update_menu_sensitivity(ddisp);
    diagram_flush(ddisp->diagram);

    diagram_modified(ddisp->diagram);
    diagram_update_extents(ddisp->diagram);
    undo_set_transactionpoint(ddisp->diagram->undo);
  }
}

void
edit_paste_callback (GtkAction *action)
{
  GList *paste_list;
  DDisplay *ddisp;
  Point paste_corner;
  Point delta;
  Change *change;
  int generation = 0;
  
  ddisp = ddisplay_active();
  if (!ddisp) return;
  if (textedit_mode(ddisp)) {
#ifdef G_OS_WIN32
    gtk_clipboard_request_text(gtk_clipboard_get(GDK_NONE), 
			       received_clipboard_handler, ddisp);
#else
    gtk_clipboard_request_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), 
			       received_clipboard_handler, ddisp);
#endif
  } else {
    if (!cnp_exist_stored_objects()) {
      message_warning(_("No existing object to paste.\n"));
      return;
    }
  
    paste_list = cnp_get_stored_objects(&generation,ddisp); /* Gets a copy */
    if (paste_list == NULL) {
      return;
    }

    paste_corner = object_list_corner(paste_list);
  
    delta.x = ddisp->visible.left - paste_corner.x;
    delta.y = ddisp->visible.top - paste_corner.y;

    /* Move down some 10% of the visible area. */
    delta.x += (ddisp->visible.right - ddisp->visible.left) * 0.1 * generation;
    delta.y += (ddisp->visible.bottom - ddisp->visible.top) * 0.1 * generation;

    if (generation)
      object_list_move_delta(paste_list, &delta);

    change = undo_insert_objects(ddisp->diagram, paste_list, 0);
    (change->apply)(change, ddisp->diagram);

    diagram_modified(ddisp->diagram);
    undo_set_transactionpoint(ddisp->diagram->undo);
  
    diagram_remove_all_selected(ddisp->diagram, TRUE);
    diagram_select_list(ddisp->diagram, paste_list);

    diagram_update_extents(ddisp->diagram);
    diagram_flush(ddisp->diagram);
  }
  dic_dialog_update_dialog();
}

/*
 * ALAN: Paste should probably paste to different position, feels
 * wrong somehow.  ALAN: The offset should increase a little each time
 * if you paste/duplicate several times in a row, because it is
 * clearer what is happening than if you were to piling them all in
 * one place.
 *
 * completely untested, basically it is copy+paste munged together
 */
void
edit_duplicate_callback (GtkAction *action)
{ 
  GList *duplicate_list;
  DDisplay *ddisp;
  Point duplicate_corner;
  Point delta;
  Change *change;

  ddisp = ddisplay_active();
  if (!ddisp || textedit_mode(ddisp)) return;
  duplicate_list = object_copy_list(diagram_get_sorted_selected(ddisp->diagram));
  duplicate_corner = object_list_corner(duplicate_list);
  cnp_prepare_copy_embed_object_list(duplicate_list,NULL);
  
  /* Move down some 10% of the visible area. */
  delta.x = (ddisp->visible.right - ddisp->visible.left)*0.05;
  delta.y = (ddisp->visible.bottom - ddisp->visible.top)*0.05;

  object_list_move_delta(duplicate_list, &delta);

  change = undo_insert_objects(ddisp->diagram, duplicate_list, 0);
  (change->apply)(change, ddisp->diagram);

  diagram_modified(ddisp->diagram);
  undo_set_transactionpoint(ddisp->diagram->undo);
  
  diagram_remove_all_selected(ddisp->diagram, TRUE);
  diagram_select_list(ddisp->diagram, duplicate_list);

  diagram_flush(ddisp->diagram);
  
  ddisplay_do_update_menu_sensitivity(ddisp);
}

void
objects_move_up_layer(GtkAction *action)
{
  DDisplay *ddisp = ddisplay_active();
  GList *selected_list;
  Change *change;

  if (!ddisp || textedit_mode(ddisp)) return;
  selected_list = diagram_get_sorted_selected(ddisp->diagram);

  change = undo_move_object_other_layer(ddisp->diagram, selected_list, TRUE);
  
  (change->apply)(change, ddisp->diagram);

  diagram_modified(ddisp->diagram);
  undo_set_transactionpoint(ddisp->diagram->undo);
  
  diagram_flush(ddisp->diagram);
  
  ddisplay_do_update_menu_sensitivity(ddisp);
}

 void
objects_move_down_layer(GtkAction *action)
{
  DDisplay *ddisp = ddisplay_active();
  GList *selected_list;
  Change *change;

  if (!ddisp || textedit_mode(ddisp)) return;
  selected_list = diagram_get_sorted_selected(ddisp->diagram);

  /* Must check if move is legal here */

  change = undo_move_object_other_layer(ddisp->diagram, selected_list, FALSE);
  
  (change->apply)(change, ddisp->diagram);

  diagram_modified(ddisp->diagram);
  undo_set_transactionpoint(ddisp->diagram->undo);
  
  diagram_flush(ddisp->diagram);
  
  ddisplay_do_update_menu_sensitivity(ddisp);
}

void
edit_copy_text_callback (GtkAction *action)
{
  Focus *focus;
  DDisplay *ddisp = ddisplay_active();
  DiaObject *obj;
  GPtrArray *textprops;
  TextProperty *prop;

  if (ddisp == NULL) return;

  focus = get_active_focus((DiagramData *) ddisp->diagram);

  if ((focus == NULL) || (!focus->has_focus)) return;

  obj = focus_get_object(focus);

  if (obj->ops->get_props == NULL) 
    return;

  make_text_prop_singleton(&textprops,&prop);
  /* Get the first text property */
  obj->ops->get_props(obj, textprops);
  
  /* GTK docs claim the selection clipboard is ignored on Win32.
   * The "clipboard" clipboard is mostly ignored in Unix
   */
#ifdef G_OS_WIN32
  gtk_clipboard_set_text(gtk_clipboard_get(GDK_NONE),
			 prop->text_data, -1);
#else
  gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY),
			 prop->text_data, -1);
#endif
  prop_list_free(textprops);
}

void
edit_cut_text_callback (GtkAction *action)
{
  Focus *focus;
  DDisplay *ddisp;
  DiaObject *obj;
  Text *text;
  GPtrArray *textprops;
  TextProperty *prop;
  ObjectChange *change;

  ddisp = ddisplay_active();
  if (!ddisp) return;

  focus = get_active_focus((DiagramData *) ddisp->diagram);
  if ((focus == NULL) || (!focus->has_focus)) return;

  obj = focus_get_object(focus);
  text = (Text*)focus->user_data;

  if (obj->ops->get_props == NULL) 
    return;

  make_text_prop_singleton(&textprops,&prop);
  /* Get the first text property */
  obj->ops->get_props(obj, textprops);
  
  /* GTK docs claim the selection clipboard is ignored on Win32.
   * The "clipboard" clipboard is mostly ignored in Unix
   */
#ifdef G_OS_WIN32
  gtk_clipboard_set_text(gtk_clipboard_get(GDK_NONE),
			 prop->text_data, -1);
#else
  gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY),
			 prop->text_data, -1);
#endif

  prop_list_free(textprops);

  if (text_delete_all(text, &change)) { 
    object_add_updates(obj, ddisp->diagram);
    undo_object_change(ddisp->diagram, obj, change);
    undo_set_transactionpoint(ddisp->diagram->undo);
    diagram_modified(ddisp->diagram);
    diagram_flush(ddisp->diagram);
  }
}

void
edit_paste_text_callback (GtkAction *action)
{
  DDisplay *ddisp;

  ddisp = ddisplay_active();
  if (!ddisp) return;

#ifdef G_OS_WIN32
  gtk_clipboard_request_text(gtk_clipboard_get(GDK_NONE), 
			     received_clipboard_handler, ddisp);
#else
  gtk_clipboard_request_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), 
			     received_clipboard_handler, ddisp);
#endif
}

void
edit_delete_callback (GtkAction *action)
{
  GList *delete_list;
  DDisplay *ddisp;

  ddisp = ddisplay_active();
  if (!ddisp) return;
  if (textedit_mode(ddisp)) {
    ObjectChange *change = NULL;
    Focus *focus = get_active_focus((DiagramData *) ddisp->diagram);
    if (!text_delete_key_handler(focus, &change)) {
      return;
    }
    object_add_updates(focus->obj, ddisp->diagram);    
  } else {
    Change *change = NULL;
    diagram_selected_break_external(ddisp->diagram);
    
    delete_list = diagram_get_sorted_selected(ddisp->diagram);
    change = undo_delete_objects_children(ddisp->diagram, delete_list);
    g_list_free(delete_list);
    (change->apply)(change, ddisp->diagram);
  }
  diagram_modified(ddisp->diagram);
  diagram_update_extents(ddisp->diagram);
  
  ddisplay_do_update_menu_sensitivity(ddisp);
  diagram_flush(ddisp->diagram);
  
  undo_set_transactionpoint(ddisp->diagram->undo);
} 

void
edit_undo_callback (GtkAction *action)
{
  Diagram *dia;
  
  dia = ddisplay_active_diagram();
  if (!dia) return;

/* Handle text undo edit here! */
  undo_revert_to_last_tp(dia->undo);
  diagram_modified(dia);
  diagram_update_extents(dia);

  diagram_flush(dia);
} 

void
edit_redo_callback (GtkAction *action)
{
  Diagram *dia;
  
/* Handle text undo edit here! */
  dia = ddisplay_active_diagram();
  if (!dia) return;

  undo_apply_to_next_tp(dia->undo);
  diagram_modified(dia);
  diagram_update_extents(dia);

  diagram_flush(dia);
} 

void
help_manual_callback (GtkAction *action)
{
#ifdef GNOME
  gnome_help_display("dia", NULL, NULL);
#else
  char *helpdir, *helpindex = NULL, *command;
  guint bestscore = G_MAXINT;
  GDir *dp;
  const char *dentry;
  GError *error = NULL;

  helpdir = dia_get_data_directory("help");
  if (!helpdir) {
    message_warning(_("Could not find help directory"));
    return;
  }

  /* search through helpdir for the helpfile that matches the user's locale */
  dp = g_dir_open (helpdir, 0, &error);
  if (!dp) {
    message_warning(_("Could not open help directory:\n%s"),
                    error->message);
    g_error_free (error);
    return;
  }
  
  while ((dentry = g_dir_read_name(dp)) != NULL) {
    guint score;

    score = intl_score_locale(dentry);
    if (score < bestscore) {
      if (helpindex)
	g_free(helpindex);
#ifdef G_OS_WIN32
      /* use HTML Help on win32 if available */
      helpindex = g_strconcat(helpdir, G_DIR_SEPARATOR_S, dentry,
			      G_DIR_SEPARATOR_S "dia-manual.chm", NULL);
      if (!g_file_test(helpindex, G_FILE_TEST_EXISTS)) {
	helpindex = g_strconcat(helpdir, G_DIR_SEPARATOR_S, dentry,
			      G_DIR_SEPARATOR_S "index.html", NULL);
      }
#else
      helpindex = g_strconcat(helpdir, G_DIR_SEPARATOR_S, dentry,
			      G_DIR_SEPARATOR_S "index.html", NULL);
#endif
      bestscore = score;
    }
  }
  g_dir_close (dp);
  g_free(helpdir);
  if (!helpindex) {
    message_warning(_("Could not find help directory"));
    return;
  }

#ifdef G_OS_WIN32
# define SW_SHOWNORMAL 1
  ShellExecuteA (0, "open", helpindex, NULL, helpdir, SW_SHOWNORMAL);
#else
  command = getenv("BROWSER");
  command = g_strdup_printf("%s 'file://%s' &", command ? command : "xdg-open", helpindex);
  system(command);
  g_free(command);
#endif

  g_free(helpindex);
#endif
}

static void 
activate_url (GtkAboutDialog *about,
              const gchar    *link,
	      gpointer        data)
{
#ifdef G_OS_WIN32
  ShellExecuteA (0, "open", link, NULL, NULL, SW_SHOWNORMAL);
#else
  gchar *command = getenv("BROWSER");
  command = g_strdup_printf("%s '%s' &", command ? command : "xdg-open", link);
  system(command);
  g_free(command);
#endif
}

void
help_about_callback (GtkAction *action)
{
  const gchar *translators = _("translator_credits-PLEASE_ADD_YOURSELF_HERE");
  const gchar *license = _(
	"This program is free software; you can redistribute it and/or modify\n"
	"it under the terms of the GNU General Public License as published by\n"
	"the Free Software Foundation; either version 2 of the License, or\n"
	"(at your option) any later version.\n"
	"\n"
	"This program is distributed in the hope that it will be useful,\n"
	"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	"GNU General Public License for more details.\n"
	"\n"
	"You should have received a copy of the GNU General Public License\n"
	"along with this program; if not, write to the Free Software\n"
	"Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.\n");

  gchar *dirname = dia_get_data_directory("");
  gchar *filename = g_build_filename (dirname, "dia-splash.png", NULL);
  GdkPixbuf *logo = gdk_pixbuf_new_from_file(filename, NULL);

  gtk_about_dialog_set_url_hook (activate_url, NULL, NULL);
  gtk_show_about_dialog (NULL,
#if 0
	"logo", logo,
#endif
        "name", "Monpe",
	"version", VERSION,
	"comments", _("MONTSUQI Printing Environment"),
	"copyright", "(C) http://www.montsuqi.org",
	"website", "http://www.montsuqi.org",
#if 0
	"authors", authors,
	"documenters", documentors,
	"translator-credits", strcmp (translators, "translator_credits-PLEASE_ADD_YOURSELF_HERE")
			? translators : NULL,
#endif
	"license", license,
	NULL);
  g_free (dirname);
  g_free (filename);
  if (logo)
    g_object_unref (logo);
}

void
view_zoom_in_callback (GtkAction *action)
{
  DDisplay *ddisp;
  Point middle;
  Rectangle *visible;

  ddisp = ddisplay_active();
  if (!ddisp) return;
  visible = &ddisp->visible;
  middle.x = visible->left*0.5 + visible->right*0.5;
  middle.y = visible->top*0.5 + visible->bottom*0.5;
  
  ddisplay_zoom(ddisp, &middle, M_SQRT2);
}

void
view_zoom_out_callback (GtkAction *action)
{
  DDisplay *ddisp;
  Point middle;
  Rectangle *visible;
  
  ddisp = ddisplay_active();
  if (!ddisp) return;
  visible = &ddisp->visible;
  middle.x = visible->left*0.5 + visible->right*0.5;
  middle.y = visible->top*0.5 + visible->bottom*0.5;
  
  ddisplay_zoom(ddisp, &middle, M_SQRT1_2);
}

void 
view_zoom_set_callback (GtkAction *action)
{
  int factor;
  /* HACK the actual factor is a suffix to the action name */
  factor = atoi (gtk_action_get_name (action) + strlen ("ViewZoom"));
  view_zoom_set (factor);
}

void
view_show_cx_pts_callback (GtkToggleAction *action)
{
  DDisplay *ddisp;
  int old_val;

  ddisp = ddisplay_active();
  if (!ddisp) return;

  old_val = ddisp->show_cx_pts;
  ddisp->show_cx_pts = gtk_toggle_action_get_active (action);
  
  if (old_val != ddisp->show_cx_pts) {
    ddisplay_add_update_all(ddisp);
    ddisplay_flush(ddisp);
  }
}

void 
view_unfullscreen (void)
{
  DDisplay *ddisp;
  GtkToggleAction *item;

  ddisp = ddisplay_active();
  if (!ddisp) return;

  /* find the menuitem */
  item = GTK_TOGGLE_ACTION (menus_get_action ("ViewFullscreen"));
  if (item && gtk_toggle_action_get_active (item)) {
    gtk_toggle_action_set_active (item, FALSE);
  }
}

void
view_fullscreen_callback (GtkToggleAction *action)
{
  DDisplay *ddisp;
  int fs;

  ddisp = ddisplay_active();
  if (!ddisp) return;

  fs = gtk_toggle_action_get_active (action);
  
  if (fs) /* it is already toggled */
    gtk_window_fullscreen(GTK_WINDOW(ddisp->shell));
  else
    gtk_window_unfullscreen(GTK_WINDOW(ddisp->shell));
}

void
view_aa_callback (GtkToggleAction *action)
{
  DDisplay *ddisp;
  int aa;

  ddisp = ddisplay_active();
  if (!ddisp) return;
 
  aa = gtk_toggle_action_get_active (action);
  
  if (aa != ddisp->aa_renderer) {
    ddisplay_set_renderer(ddisp, aa);
    ddisplay_add_update_all(ddisp);
    ddisplay_flush(ddisp);
  }
}

void
view_visible_grid_callback (GtkToggleAction *action)
{
  DDisplay *ddisp;
  guint old_val;

  ddisp = ddisplay_active();
  if (!ddisp) return;
  
  old_val = ddisp->grid.visible;
  ddisp->grid.visible = gtk_toggle_action_get_active (action); 

  if (old_val != ddisp->grid.visible) {
    ddisplay_add_update_all(ddisp);
    ddisplay_flush(ddisp);
  }
}

void
view_snap_to_grid_callback (GtkToggleAction *action)
{
  DDisplay *ddisp;

  ddisp = ddisplay_active();
  if (!ddisp) return;
  
  ddisplay_set_snap_to_grid(ddisp, gtk_toggle_action_get_active (action));
}

void
view_snap_to_objects_callback (GtkToggleAction *action)
{
  DDisplay *ddisp;

  ddisp = ddisplay_active();
  if (!ddisp) return;
  
  ddisplay_set_snap_to_objects(ddisp, gtk_toggle_action_get_active (action));
}

void 
view_toggle_rulers_callback (GtkToggleAction *action)
{
  DDisplay *ddisp;
  
  ddisp = ddisplay_active();
  if (!ddisp) return;

  if (!gtk_toggle_action_get_active (action)) {
    if (display_get_rulers_showing(ddisp)) {
      display_rulers_hide (ddisp);
    }
  } else {
    if (!display_get_rulers_showing(ddisp)) {
      display_rulers_show (ddisp);
    }
  }
}

extern void
view_new_view_callback (GtkAction *action)
{
  Diagram *dia;

  dia = ddisplay_active_diagram();
  if (!dia) return;
  
  new_display(dia);
}

extern void
view_clone_view_callback (GtkAction *action)
{
  DDisplay *ddisp;

  ddisp = ddisplay_active();
  if (!ddisp) return;
  
  copy_display(ddisp);
}

void
view_show_all_callback (GtkAction *action)
{
  DDisplay *ddisp;

  ddisp = ddisplay_active();
  if (!ddisp) return;
  
  ddisplay_show_all (ddisp);
}

void
view_redraw_callback (GtkAction *action)
{
  DDisplay *ddisp;
  ddisp = ddisplay_active();
  if (!ddisp) return;
  ddisplay_add_update_all(ddisp);
  ddisplay_flush(ddisp);
}

void
view_diagram_properties_callback (GtkAction *action)
{
  DDisplay *ddisp;

  ddisp = ddisplay_active();
  if (!ddisp) return;
  diagram_properties_show(ddisp->diagram);
}

void
view_main_toolbar_callback (GtkAction *action)
{
  if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION(action)) == TRUE)
  {
    integrated_ui_main_toolbar_show ();
  }
  else
  {
    integrated_ui_main_toolbar_hide ();
  }
}

void
view_main_statusbar_callback (GtkAction *action)
{
  if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION(action)) == TRUE)
  {
    integrated_ui_main_statusbar_show ();
  }
  else
  {
    integrated_ui_main_statusbar_hide ();
  }
}

void
view_layers_callback (GtkAction *action)
{
  if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION(action)) == TRUE)
  {
    integrated_ui_layer_view_show ();
  }
  else
  {
    integrated_ui_layer_view_hide ();
  }
}

void 
layers_add_layer_callback (GtkAction *action)
{
  Diagram *dia;

  dia = ddisplay_active_diagram();
  if (!dia) return;

  diagram_edit_layer (dia, NULL);
}

void 
layers_rename_layer_callback (GtkAction *action)
{
  Diagram *dia;

  dia = ddisplay_active_diagram();
  if (!dia) return;

  diagram_edit_layer (dia, dia->data->active_layer);
}

void
objects_place_over_callback (GtkAction *action)
{
  diagram_place_over_selected(ddisplay_active_diagram());
}

void
objects_place_under_callback (GtkAction *action)
{
  diagram_place_under_selected(ddisplay_active_diagram());
}

void
objects_place_up_callback (GtkAction *action)
{
  diagram_place_up_selected(ddisplay_active_diagram());
}

void
objects_place_down_callback (GtkAction *action)
{
  DDisplay *ddisp = ddisplay_active();
  if (!ddisp || textedit_mode(ddisp)) return;

  diagram_place_down_selected(ddisplay_active_diagram());
}

void
objects_parent_callback (GtkAction *action)
{
  DDisplay *ddisp = ddisplay_active();
  if (!ddisp || textedit_mode(ddisp)) return;

  diagram_parent_selected(ddisplay_active_diagram());
}

void
objects_unparent_callback (GtkAction *action)
{
  DDisplay *ddisp = ddisplay_active();
  if (!ddisp || textedit_mode(ddisp)) return;

  diagram_unparent_selected(ddisplay_active_diagram());
}

void
objects_unparent_children_callback (GtkAction *action)
{
  DDisplay *ddisp = ddisplay_active();
  if (!ddisp || textedit_mode(ddisp)) return;

  diagram_unparent_children_selected(ddisplay_active_diagram());
}

void
objects_group_callback (GtkAction *action)
{
  DDisplay *ddisp = ddisplay_active();
  if (!ddisp || textedit_mode(ddisp)) return;

  diagram_group_selected(ddisplay_active_diagram());
  ddisplay_do_update_menu_sensitivity(ddisp);
} 

void
objects_ungroup_callback (GtkAction *action)
{
  DDisplay *ddisp = ddisplay_active();
  if (!ddisp || textedit_mode(ddisp)) return;

  diagram_ungroup_selected(ddisplay_active_diagram());
  ddisplay_do_update_menu_sensitivity(ddisp);
} 

void
dialogs_properties_callback (GtkAction *action)
{
  Diagram *dia;
  DiaObject *selected;

  dia = ddisplay_active_diagram();
  if (!dia || textedit_mode(ddisplay_active())) return;

  if (dia->data->selected != NULL) {
    object_list_properties_show(dia, dia->data->selected);
  } else {
    diagram_properties_show(dia);
  } 
}

void
dialogs_layers_callback (GtkAction *action)
{
  layer_dialog_set_diagram(ddisplay_active_diagram());
  layer_dialog_show();
}

void
dialogs_dic_callback (GtkAction *action)
{
  dic_dialog_set_diagram(ddisplay_active_diagram());
  dic_dialog_show();
}

void
dialogs_pos_callback (GtkAction *action)
{
  pos_dialog_set_diagram(ddisplay_active_diagram());
  pos_dialog_show();
}


void
objects_align_h_callback (GtkAction *action)
{
  const gchar *a;
  int align = DIA_ALIGN_LEFT;
  Diagram *dia;
  GList *objects;

  DDisplay *ddisp = ddisplay_active();
  if (!ddisp || textedit_mode(ddisp)) return;

  /* HACK align is suffix to action name */
  a = gtk_action_get_name (action) + strlen ("ObjectsAlign");
  if (0 == strcmp ("Left", a)) {
	align = DIA_ALIGN_LEFT;
  } 
  else if (0 == strcmp ("Center", a)) {
	align = DIA_ALIGN_CENTER;
  } 
  else if (0 == strcmp ("Right", a)) {
	align = DIA_ALIGN_RIGHT;
  } 
  else if (0 == strcmp ("Spreadouthorizontally", a)) {
	align = DIA_ALIGN_EQUAL;
  } 
  else if (0 == strcmp ("Adjacent", a)) {
	align = DIA_ALIGN_ADJACENT;
  }
  else {
	g_warning ("objects_align_v_callback() called without appropriate align");
	return;
  }

  dia = ddisplay_active_diagram();
  if (!dia) return;
  objects = dia->data->selected;
  
  object_add_updates_list(objects, dia);
  object_list_align_h(objects, dia, align);
  diagram_update_connections_selection(dia);
  object_add_updates_list(objects, dia);
  diagram_modified(dia);
  diagram_flush(dia);     

  undo_set_transactionpoint(dia->undo);
}

void
objects_align_v_callback (GtkAction *action)
{
  const gchar *a;
  int align;
  Diagram *dia;
  GList *objects;

  DDisplay *ddisp = ddisplay_active();
  if (!ddisp || textedit_mode(ddisp)) return;

  /* HACK align is suffix to action name */
  a = gtk_action_get_name (action) + strlen ("ObjectsAlign");
  if (0 == strcmp ("Top", a)) {
	align = DIA_ALIGN_TOP;
  } 
  else if (0 == strcmp ("Middle", a)) {
	align = DIA_ALIGN_CENTER;
  } 
  else if (0 == strcmp ("Bottom", a)) {
	align = DIA_ALIGN_BOTTOM;
  } 
  else if (0 == strcmp ("Spreadoutvertically", a)) {
	align = DIA_ALIGN_EQUAL;
  } 
  else if (0 == strcmp ("Stacked", a)) {
	align = DIA_ALIGN_ADJACENT;
  }
  else {
	g_warning ("objects_align_v_callback() called without appropriate align");
	return;
  }

  dia = ddisplay_active_diagram();
  if (!dia) return;
  objects = dia->data->selected;

  object_add_updates_list(objects, dia);
  object_list_align_v(objects, dia, align);
  diagram_update_connections_selection(dia);
  object_add_updates_list(objects, dia);
  diagram_modified(dia);
  diagram_flush(dia);     

  undo_set_transactionpoint(dia->undo);
}


