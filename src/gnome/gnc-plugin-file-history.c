/* 
 * gnc-plugin-file-history.c -- 
 * Copyright (C) 2003,2005 David Hampton hampton@employees.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652
 * Boston, MA  02111-1307,  USA       gnu@gnu.org
 */

/** @addtogroup GUI
    @{ */
/** @internal
    @file gnc-plugin-file-history.h
    @brief Utility functions for writing import modules.
    @author Copyright (C) 2002 David Hampton <hampton@empployees.org>
*/

#include "config.h"

#include <string.h>
#include <glib/gprintf.h>
#include <libgnome/libgnome.h>

#include "gnc-file.h"
#include "gnc-file-history.h"
#include "gnc-main-window.h"
#include "gnc-plugin-file-history.h"
#include "gnc-window.h"
#include "gnc-trace.h"
#include "messages.h"
#include "gnc-gconf-utils.h"

static GList *active_plugins = NULL;
static GObjectClass *parent_class = NULL;

#define FILENAME_STRING "filename"

static void gnc_plugin_file_history_class_init (GncPluginFileHistoryClass *klass);
static void gnc_plugin_file_history_init (GncPluginFileHistory *plugin);
static void gnc_plugin_file_history_finalize (GObject *object);

static void gnc_plugin_file_history_add_to_window (GncPlugin *plugin, GncMainWindow *window, GQuark type);
static void gnc_plugin_file_history_remove_from_window (GncPlugin *plugin, GncMainWindow *window, GQuark type);

static short module = MOD_GUI;

/* Command callbacks */
static void gnc_plugin_file_history_cmd_open_file (GtkAction *action, GncMainWindowActionData *data);


#define PLUGIN_ACTIONS_NAME "gnc-plugin-file-history-actions"
#define PLUGIN_UI_FILENAME  "gnc-plugin-file-history-ui.xml"

static GtkActionEntry gnc_plugin_actions [] = {
	{ "FileOpenRecentAction", NULL, N_("Open _Recent"), NULL, NULL, NULL },
	{ "RecentFile0Action", NULL, "", NULL, NULL, G_CALLBACK (gnc_plugin_file_history_cmd_open_file) },
	{ "RecentFile1Action", NULL, "", NULL, NULL, G_CALLBACK (gnc_plugin_file_history_cmd_open_file) },
	{ "RecentFile2Action", NULL, "", NULL, NULL, G_CALLBACK (gnc_plugin_file_history_cmd_open_file) },
	{ "RecentFile3Action", NULL, "", NULL, NULL, G_CALLBACK (gnc_plugin_file_history_cmd_open_file) },
	{ "RecentFile4Action", NULL, "", NULL, NULL, G_CALLBACK (gnc_plugin_file_history_cmd_open_file) },
	{ "RecentFile5Action", NULL, "", NULL, NULL, G_CALLBACK (gnc_plugin_file_history_cmd_open_file) },
	{ "RecentFile6Action", NULL, "", NULL, NULL, G_CALLBACK (gnc_plugin_file_history_cmd_open_file) },
	{ "RecentFile7Action", NULL, "", NULL, NULL, G_CALLBACK (gnc_plugin_file_history_cmd_open_file) },
	{ "RecentFile8Action", NULL, "", NULL, NULL, G_CALLBACK (gnc_plugin_file_history_cmd_open_file) },
	{ "RecentFile9Action", NULL, "", NULL, NULL, G_CALLBACK (gnc_plugin_file_history_cmd_open_file) },
};
static guint gnc_plugin_n_actions = G_N_ELEMENTS (gnc_plugin_actions);


struct GncPluginFileHistoryPrivate
{
	gpointer dummy;
};


/************************************************************
 *                     Other Functions                      *
 ************************************************************/

/** This routine takes a filename and modifies it so that it will
 * display correctly in a GtkLabel.  It also adds a mnemonic to
 * the start of the menu item.
 *
 *  @filename A pointer to the filename to mangle.
 *
 *  @return A pointer to the mangled filename.  The Caller is
 *  responsible for freeing this memory.
 */
static gchar *
gnc_history_generate_label (int index, const gchar *filename)
{
	const gchar *src;
	gchar *result, *dst;
	gunichar  unichar;

	/* raw byte length, not num characters */
	result = g_malloc(strlen(filename) * 2);

	dst = result + g_sprintf(result, "_%d ", index);
	for (src = filename; *src; src = g_utf8_next_char(src)) {
	  unichar = g_utf8_get_char(src);
	  dst += g_unichar_to_utf8 (unichar, dst);

	  if (unichar == '_')
	    dst += g_unichar_to_utf8 ('_', dst);
	}

	*dst = '\0';
	return result;
}


/** Update one entry in the file history menu.  This function is
 *  called by either the gnc_plugin_history_list_changed function or
 *  the gnc_history_update_menus function.  It updates the specified
 *  file history item in the specified window.
 *
 *  This routine attaches the actual filename to the menu_item (via
 *  g_object_set_data) for later retrieval.  It also massages the
 *  filename so that it will display correctly in the menu, and also
 *  add a mnemonic for the menu item.
 *
 *  @window A pointer to window whose file history should be updated.
 *
 *  @index Update this item in the menu (base-0).
 *
 *  @filename The new filename to associate with this menu item.
 */
static void
gnc_history_update_action (GncMainWindow *window,
			   gint index,
			   const gchar *filename)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	gchar *action_name, *label_name, *old_filename;

	ENTER("window %p, index %d, filename %s", window, index, filename);
	/* Get the action group */
	action_group =
	  gnc_main_window_get_action_group(window, PLUGIN_ACTIONS_NAME);

	action_name = g_strdup_printf("RecentFile%dAction", index);
	action = gtk_action_group_get_action (action_group, action_name);

	if (filename && (strlen(filename) > 0)) {
	  /* set the menu label (w/accelerator) */
	  label_name = gnc_history_generate_label(index, filename);
	  g_object_set(G_OBJECT(action), "label", label_name, "visible", TRUE, NULL);
	  g_free(label_name);

	  /* set the filename for the callback function */
	  old_filename = g_object_get_data(G_OBJECT(action), FILENAME_STRING);
	  if (old_filename)
	    g_free(old_filename);
	  g_object_set_data(G_OBJECT(action), FILENAME_STRING, g_strdup(filename));
	} else {
	  g_object_set(G_OBJECT(action), "visible", FALSE, NULL);
	}
	g_free(action_name);
	LEAVE("");
}


/** Update an entry in the file history menu because a gconf entry
 *  changed.  This function is called whenever an item in the gconf
 *  history section is changed.  It is responsible for updating the
 *  menu item that corresponds to that key.
 *
 *  @client A pointer to gconf client that noticed an entry change.
 *
 *  @cnxn_id Unused.
 *
 *  @entry A pointer to gconf entry that changed.
 *
 *  @user_data A pointer to the window that this gconf client is
 *  associated with.
 */
static void
gnc_plugin_history_list_changed (GConfClient *client,
				 guint cnxn_id,
				 GConfEntry *entry,
				 gpointer user_data)
{
	GncMainWindow *window;
	GConfValue *value;
	const gchar *key, *filename;
	gint index;

	ENTER("");
	key = gconf_entry_get_key(entry);
	index = gnc_history_gconf_key_to_index(key);
	if (index < 0)
	  return;

	window = GNC_MAIN_WINDOW(user_data);
	value = gconf_entry_get_value(entry);
	if (!value) {
	  LEAVE("No gconf value");
	  return;
	}
	filename = gconf_value_get_string(value);
	gnc_history_update_action (window, index, filename);

	gnc_main_window_actions_updated (window);
	LEAVE("");
}

/** Update the file history menu for a window.  This function walks
 *  the list of all possible gconf keys for the file history and
 *  forces a read/menu update on each key.  It should only be called
 *  once when the window is created.
 *
 *  @window A pointer to the window whose file history menu should be
 *  updated.
 */
static void
gnc_history_update_menus (GncMainWindow *window)
{
	gchar *filename, *key;
	guint i;

	ENTER("");
	for (i = 0; i < MAX_HISTORY_FILES; i++) {
	  key = g_strdup_printf(HISTORY_STRING_FILE_N, i);
	  filename = gnc_gconf_get_string(HISTORY_STRING_SECTION, key, NULL);
	  gnc_history_update_action(window, i, filename);
	  g_free(filename);
	  g_free(key);
	}
	LEAVE("");
}


/************************************************************
 *                  Object Implementation                   *
 ************************************************************/

GType
gnc_plugin_file_history_get_type (void)
{
	static GType gnc_plugin_file_history_type = 0;

	if (gnc_plugin_file_history_type == 0) {
		static const GTypeInfo our_info = {
			sizeof (GncPluginFileHistoryClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gnc_plugin_file_history_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GncPluginFileHistory),
			0,
			(GInstanceInitFunc) gnc_plugin_file_history_init
		};

		gnc_plugin_file_history_type = g_type_register_static (GNC_TYPE_PLUGIN,
								       "GncPluginFileHistory",
								       &our_info, 0);
	}

	return gnc_plugin_file_history_type;
}

#if DEBUG_REFERENCE_COUNTING
static void
dump_model (GncPluginFileHistory *plugin, gpointer dummy)
{
    g_warning("GncPluginFileHistory %p still exists.", plugin);
}

static gint
gnc_plugin_file_history_report_references (void)
{
  g_list_foreach(active_plugins, (GFunc)dump_model, NULL);
  return 0;
}
#endif

static void
gnc_plugin_file_history_class_init (GncPluginFileHistoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GncPluginClass *plugin_class = GNC_PLUGIN_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gnc_plugin_file_history_finalize;

	/* plugin info */
	plugin_class->plugin_name   = GNC_PLUGIN_FILE_HISTORY_NAME;

	/* function overrides */
	plugin_class->add_to_window = gnc_plugin_file_history_add_to_window;
	plugin_class->remove_from_window = gnc_plugin_file_history_remove_from_window;

	/* widget addition/removal */
	plugin_class->actions_name  = PLUGIN_ACTIONS_NAME;
	plugin_class->actions       = gnc_plugin_actions;
	plugin_class->n_actions     = gnc_plugin_n_actions;
	plugin_class->ui_filename   = PLUGIN_UI_FILENAME;

	plugin_class->gconf_section = HISTORY_STRING_SECTION;
	plugin_class->gconf_notifications = gnc_plugin_history_list_changed;

#if DEBUG_REFERENCE_COUNTING
	gtk_quit_add (0,
		      (GtkFunction)gnc_plugin_file_history_report_references,
		      NULL);
#endif
}

static void
gnc_plugin_file_history_init (GncPluginFileHistory *plugin)
{
	ENTER("plugin %p", plugin);
	plugin->priv = g_new0 (GncPluginFileHistoryPrivate, 1);

	active_plugins = g_list_append (active_plugins, plugin);
	LEAVE("");
}

static void
gnc_plugin_file_history_finalize (GObject *object)
{
	GncPluginFileHistory *plugin;

	g_return_if_fail (GNC_IS_PLUGIN_FILE_HISTORY (object));

	plugin = GNC_PLUGIN_FILE_HISTORY (object);
	ENTER("plugin %p", plugin);
	active_plugins = g_list_remove (active_plugins, plugin);

	g_return_if_fail (plugin->priv != NULL);
	g_free (plugin->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
	LEAVE("");
}

GncPlugin *
gnc_plugin_file_history_new (void)
{
	GncPlugin *plugin_page = NULL;

	ENTER("");
	plugin_page = GNC_PLUGIN (g_object_new (GNC_TYPE_PLUGIN_FILE_HISTORY, NULL));
	LEAVE("plugin %p", plugin_page);
	return plugin_page;
}

/************************************************************
 *              Plugin Function Implementation              *
 ************************************************************/

/** Initialize the file history menu for a window.  This function is
 *  called as part of the initialization of a window, after all the
 *  plugin menu items have been added to the menu structure.  Its job
 *  is to correctly initialize the file history menu.  It does this by
 *  first calling a function that initializes the menu to the current
 *  as maintained in gconf.  It then creates a gconf client that will
 *  listens for any changes to the file history menu, and will update
 *  the meny when they are signalled.
 *
 *  @param plugin A pointer to the gnc-plugin object responsible for
 *  adding/removing the file history menu.
 *
 *  @param window A pointer the gnc-main-window that is being initialized.
 *
 *  @param type Unused
 */
static void
gnc_plugin_file_history_add_to_window (GncPlugin *plugin,
				       GncMainWindow *window,
				       GQuark type)
{
	gnc_history_update_menus(window);
}


/** Finalize the file history menu for this window.  This function is
 *  called as part of the destruction of a window.
 *
 *  @param plugin A pointer to the gnc-plugin object responsible for
 *  adding/removing the file history menu.  It stops the gconf
 *  notifications for this window, and destroys the gconf client
 *  object.
 *
 *  @param window A pointer the gnc-main-window that is being destroyed.
 *
 *  @param type Unused
 */
static void
gnc_plugin_file_history_remove_from_window (GncPlugin *plugin,
					    GncMainWindow *window,
					    GQuark type)
{
}

/************************************************************
 *                    Command Callbacks                     *
 ************************************************************/

/** The user has selected one of the items in the File History menu.
 *  Close down the current session and start up a new one with the
 *  requested file.
 *
 *  @param action A pointer to the action selected by the user.  This
 *  action represents one of the items in the file history menu.
 *
 *  @param data A pointer to the gnc-main-window data to be used by
 *  this function.  This is mainly to find out which window it was
 *  that had a menu selected.  That's not really important for this
 *  function and we're about to close all the windows anyway.
 */
static void
gnc_plugin_file_history_cmd_open_file (GtkAction *action,
				       GncMainWindowActionData *data)
{
	gchar *filename;

	g_return_if_fail(GTK_IS_ACTION(action));
	g_return_if_fail(data != NULL);

	/* DRH - Do we need to close all open windows but the first?
	 * Which progressbar should we be using? One in a window, or 
	 * in a new "file loading" dialog???
	 */
	filename = g_object_get_data(G_OBJECT(action), FILENAME_STRING);
	gnc_window_set_progressbar_window (GNC_WINDOW(data->window));
	gnc_file_open_file (filename); /* also opens new account page */
	gnc_window_set_progressbar_window (NULL);
	gnc_main_window_update_title (data->window);
	/* FIXME GNOME 2 Port (update the title etc.) */
}

/** @} */
