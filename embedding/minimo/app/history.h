/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Doug Turner <dougt@meer.net>  Branched from TestGtkEmbed.cpp
 *
 *   The 10LE Team (in alphabetical order)
 *   -------------------------------------
 *
 *    Ilias Biris       <ext-ilias.biris@indt.org.br> - Coordinator
 *    Afonso Costa      <afonso.costa@indt.org.br>
 *    Antonio Gomes     <antonio.gomes@indt.org.br>
 *    Diego Gonzalez    <diego.gonzalez@indt.org.br>
 *    Raoni Novellino   <raoni.novellino@indt.org.br>
 *    Andre Pedralho    <andre.pedralho@indt.org.br>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "minimo_types.h"

#include "preference.h"

#include "gtkmozembed.h"

/* History callbacks */
void view_history(GtkWidget *embed);
static void clear_history(GtkWidget *b, HistoryWindow *hwin);
static void close_history_win(GtkWidget *button, HistoryWindow *hwin);
static void on_history_clist_button_go_cb(GtkWidget *button, HistoryWindow *hwin);
static void remove_from_history(GtkWidget *button, HistoryWindow *hwin);
static void remove_from_list(gchar *text);
static void on_history_destroy(GtkWidget *window);
static void on_search_button_cb(GtkWidget *button, HistoryWindow *hwin);

/* History functions*/
void add_to_history(const gchar *url);
void write_history(void);
void read_history(void);

/* Add an URL into History file */
void add_to_history(const gchar *url) {
  GSList *list;
  gint equals = 0;
  
  read_history();
  
  if (!url || url == NULL) return ;
  if (!g_strncasecmp(url,"about:",6) || !g_strcasecmp(url,"")) return;
  
  while (prot == 1) {
  }	
  
  prot = 1;
  
  for (list = history; list ; list = list->next) {
    if (g_strcasecmp((const gchar*)list->data,url) == 0){
      equals = 1;
      prot = 0;
    }
  }
  if (equals == 0) {
    history = g_slist_prepend(history,g_strdup(url));
    write_history();
  }
  prot = 0;
  
  
}

/* Create and show the History Window*/
void view_history(GtkWidget *embed) {
  
  HistoryWindow *hwin;
  GSList *list;
  GtkWidget *image;
  
  hwin = g_new0(HistoryWindow,1);
  
  read_history();
  
  hwin->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(G_OBJECT(hwin->window),"destroy", G_CALLBACK(on_history_destroy), NULL);
  hwin->title = g_strdup_printf(("Minimo - History (%d) items"), g_slist_length(history));
  gtk_window_set_title(GTK_WINDOW(hwin->window),hwin->title);
  gtk_widget_set_usize(hwin->window,230,300);
  gtk_window_set_resizable(GTK_WINDOW(hwin->window),FALSE);
  gtk_window_set_position (GTK_WINDOW(hwin->window),GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (hwin->window), TRUE);
  gtk_window_set_keep_above(GTK_WINDOW (hwin->window), TRUE);
  g_free(hwin->title);
  
  hwin->vbox = gtk_vbox_new(FALSE,5);
  gtk_container_add(GTK_CONTAINER(hwin->window),hwin->vbox);
  
  hwin->scrolled_window = gtk_scrolled_window_new(NULL,NULL);
  gtk_box_pack_start(GTK_BOX(hwin->vbox),hwin->scrolled_window,TRUE,TRUE,0);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(hwin->scrolled_window),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
  
  hwin->clist = gtk_clist_new(1);
  
  gtk_container_add(GTK_CONTAINER(hwin->scrolled_window),hwin->clist);
  
  hwin->search_label = gtk_label_new("Search: ");
  hwin->search_entry = gtk_entry_new();
  hwin->search_box = gtk_hbox_new(0,0);
  image = gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_SMALL_TOOLBAR);
  hwin->search_button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(hwin->search_button),image);
  gtk_tooltips_set_tip (gtk_tooltips_new (), GTK_WIDGET (hwin->search_button), "Find on history", NULL);
  image = gtk_image_new_from_stock (GTK_STOCK_YES, GTK_ICON_SIZE_SMALL_TOOLBAR);
  hwin->go_button = gtk_button_new ();
  gtk_container_add(GTK_CONTAINER(hwin->go_button),image);
  gtk_tooltips_set_tip (gtk_tooltips_new (), GTK_WIDGET (hwin->go_button), "Go to", NULL);
  
  hwin->embed = embed;
  
  g_signal_connect(G_OBJECT(hwin->search_button), "clicked", G_CALLBACK(on_search_button_cb), hwin);
  g_signal_connect(G_OBJECT(hwin->go_button), "clicked", G_CALLBACK(on_history_clist_button_go_cb), hwin);
  
  gtk_box_pack_start(GTK_BOX(hwin->search_box), hwin->search_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hwin->search_box), hwin->search_entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hwin->search_box), hwin->search_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hwin->search_box), hwin->go_button, FALSE, FALSE, 0);
  
  hwin->remove = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
  g_signal_connect(G_OBJECT(hwin->remove), "clicked", G_CALLBACK(remove_from_history), hwin);
  
  hwin->clear = gtk_button_new_from_stock(GTK_STOCK_CLEAR);
  g_signal_connect(G_OBJECT(hwin->clear), "clicked", G_CALLBACK(clear_history), hwin);
  
  hwin->close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
  g_signal_connect(G_OBJECT(hwin->close), "clicked", G_CALLBACK(close_history_win), hwin);
  
  hwin->btnbox = gtk_hbox_new(0,0);
  gtk_box_pack_start(GTK_BOX(hwin->vbox), hwin->search_box, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hwin->vbox), hwin->btnbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hwin->btnbox), hwin->remove, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hwin->btnbox), hwin->clear, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hwin->btnbox), hwin->close, TRUE, TRUE, 0);
  
  
  while (prot == 1) {
  }	
  
  prot = 1;
  for (list = history; list ; list = list->next) {
    gchar *clist_entry[1];
    clist_entry[0] = (gchar *)list->data;
    gtk_clist_append(GTK_CLIST(hwin->clist),clist_entry);
  }
  prot = 0;
  gtk_widget_show_all(hwin->window);
}

/* Go to URL selected in the history list */
static void on_history_clist_button_go_cb(GtkWidget *button, HistoryWindow *hwin) {
  GList *selection;
  gchar *location;
  gint row;
  
  selection = GTK_CLIST(hwin->clist)->selection;
  
  if (g_list_length(selection) == 0)  return;
  
  row = (gint)g_list_nth_data(selection,0);
  gtk_clist_get_text(GTK_CLIST(hwin->clist),row,0,&location);
  gtk_moz_embed_stop_load(GTK_MOZ_EMBED(hwin->embed));
  gtk_moz_embed_load_url(GTK_MOZ_EMBED(hwin->embed), location);
}

/* Close history window */
static void close_history_win(GtkWidget *button, HistoryWindow *hwin) {

  gtk_widget_destroy(hwin->window);
}

/* Remove an URL from history list */
static void remove_from_history(GtkWidget *button, HistoryWindow *hwin) {
  GList *selection = GTK_CLIST(hwin->clist)->selection;
  gchar *text;
  gint row;
  
  
  if (selection == NULL) return;
  
  gtk_clist_freeze(GTK_CLIST(hwin->clist));
  row = (gint) g_list_nth_data(selection,0);
  gtk_clist_get_text(GTK_CLIST(hwin->clist), row, 0, &text);
  remove_from_list(text);
  gtk_clist_remove(GTK_CLIST(hwin->clist), row);	
  gtk_clist_thaw(GTK_CLIST(hwin->clist));
  hwin->title = g_strdup_printf(("Minimo - History (%d) items"), g_slist_length(history));
  gtk_window_set_title(GTK_WINDOW(hwin->window),hwin->title);
  
}

/* Remove an URL from history file */
static void remove_from_list(gchar *text) {
  GSList *l;
  
  for (l = history; l ; l = l->next) {
    gchar *data = (gchar *)l->data;
    if (!strcmp(text,data)) {
      history = g_slist_remove(history,data);
      return;
    }
  }
  
}

/* Remove all URL from history list */
static void clear_history(GtkWidget *b, HistoryWindow *hwin) {
  gtk_clist_clear(GTK_CLIST(hwin->clist));
  g_slist_free(history);
  history = NULL;
  gtk_window_set_title(GTK_WINDOW(hwin->window),"Minimo - History (0) items");
}

/* Search an URL in the history list */
static void on_search_button_cb(GtkWidget *button, HistoryWindow *hwin) {
  gint rows, i;
  G_CONST_RETURN gchar *search_text;
  gint search_pos = 0;
  
  rows = GTK_CLIST(hwin->clist)->rows;
  if (!rows) return;
  search_text = gtk_entry_get_text(GTK_ENTRY(hwin->search_entry));
  if (!search_text || !strcmp(search_text,"")) return; 
  if (search_pos >= rows) search_pos = 0;
  if (search_pos) search_pos++;
  for (i = search_pos; i < rows ; i++) {
    gchar *tmp, *tmp2 = NULL;
    gtk_clist_get_text(GTK_CLIST(hwin->clist), i, 0, &tmp);
    g_return_if_fail(tmp != NULL);
    tmp2 = strstr(tmp,search_text);
    if (tmp2 != NULL) {
      gtk_clist_moveto(GTK_CLIST(hwin->clist), i, 0, 0.0, 0.0);
      gtk_clist_select_row(GTK_CLIST(hwin->clist), i, 0);
      GTK_CLIST(hwin->clist)->focus_row = i;
      search_pos = i;
    }	       		     	
    
  }
}

/* Close history window */
static void on_history_destroy(GtkWidget *window) {
  write_history();
}

/* Read the history file */
void read_history(void) {
  gchar *line;
  gchar *url;
  gchar *user_home = NULL;
  FILE   *g_history_file = NULL;
  gchar  *g_history_file_name = NULL;
  
  history = NULL;
  
  user_home = g_strconcat(g_get_home_dir(),NULL);
  g_history_file_name = g_strconcat(user_home,"/.Minimo/history",NULL);
  
  if ((g_history_file = fopen(g_history_file_name,"r"))!= NULL)
  {
    fclose(g_history_file);
    g_history_file = fopen(g_history_file_name,"r+");
  }
  else
    g_history_file = fopen(g_history_file_name,"w+");
  
  if (g_history_file == NULL) {
    printf("No History file to read!\n");
  }
  line = (gchar *)g_malloc(1024);
  
  while(fgets(line,1024,g_history_file) != NULL) {
    line[strlen(line)-1] = '\0';
    url = g_strdup(line);
    history = g_slist_append(history,url);
  }
  
  fclose(g_history_file);
  g_free(line);
  
}

/* Write the history file */
void write_history(void) {
  GSList *list;
  gchar *user_home= NULL;
  FILE   *g_history_file = NULL;
  gchar  *g_history_file_name = NULL;
  
  user_home = g_strconcat(g_get_home_dir(),NULL);
  g_history_file_name = g_strconcat(user_home,"/.Minimo/history",NULL);
  
  g_history_file = fopen(g_history_file_name,"w");
  if (g_history_file == NULL) {
    printf("Couldn't open history file for writing!\n");
    return;
  }
  
  for (list = history; list ; list = list->next) {
    fprintf(g_history_file,"%s\n",(gchar *)(list->data));
  }
  
  fflush(g_history_file);     
  fclose(g_history_file);
  g_free(g_history_file_name);
  
}
