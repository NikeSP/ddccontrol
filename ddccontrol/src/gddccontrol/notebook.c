/***************************************************************************
 *   Copyright (C) 2004 by Nicolas Boichat                                 *
 *   nicolas@boichat.ch                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "config.h"
  
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "notebook.h"

#define GTK_FILL_EXPAND (GtkAttachOptions)(GTK_FILL|GTK_EXPAND)

#define RETRYS 3 // number of read retrys

struct monitor mon;

enum
{
    TITLE_COL,
    ID_COL,
    WIDGET_COL,
    N_COLS,
};


// Globals
////////////////////

int modified = 0;
GSList *all_controls=0;

// Helpers
////////////////////

void get_default_and_max(struct control_db *control,unsigned short *currentDefault, unsigned short *currentMaximum)
{
    int retry;
    if (control->type != command)
    {
        for(retry = RETRYS; retry; retry--)
        {
            if (ddcci_readctrl(&mon, control->address, currentDefault, currentMaximum) >= 0)
            {
                break;
            }
        }
    }
    if(!retry)
    {
        GtkWidget* dialog = gtk_message_dialog_new (NULL,
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_CLOSE,
                "%s: %s",_("Error while getting value"), control->name);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
    }
}



void refresh_control(gpointer data,gpointer user_data)
{
    struct control_db *control;
    control = (struct control_db*)g_object_get_data(G_OBJECT(data),"ddcc_control");
   
    
    unsigned short currentDefault = 1;
    unsigned short currentMaximum = 1;
    get_default_and_max(control,&currentDefault,&currentMaximum);

    switch(control->type)
    {
        case value:
            gtk_range_set_increments(GTK_RANGE(data), 100.0/(double)currentMaximum, 10.0*100.0/(double)currentMaximum);
            gtk_range_set_value(GTK_RANGE(data), (double)100.0*currentDefault/(double)currentMaximum);
            break;
        case command:
            break;
        case list:
            break; // TODO
            {
                struct value_db* value;
                for (value = control->value_list; value != NULL; value = value->next)
                {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data), (value->value == currentDefault));
                }
                break;
            }
    }
}

// Callbacks
////////////////////

static void range_callback(GtkWidget *widget, gpointer data)
{
    struct control_db *control;
    control = (struct control_db*)g_object_get_data(G_OBJECT(widget),"ddcc_control");

	double val = gtk_range_get_value(GTK_RANGE(widget))/100.0;
    
    unsigned short currentDefault = 1;
    unsigned short currentMaximum = 1;
    get_default_and_max(control,&currentDefault,&currentMaximum);
    
 /*   printf("Would change %#x to %#x (%f, %f, %#x)\n", control->address, (unsigned char)round(val*(double)currentMaximum), val*(double)currentMaximum, val, currentMaximum);*/

	if (control)
	{
		unsigned short nval = (unsigned short)round(val*(double)currentMaximum);
		ddcci_writectrl(&mon, control->address, nval);
		
		gtk_range_set_value(GTK_RANGE(widget), (double)100.0*nval/(double)currentMaximum);
		//gtk_widget_set_sensitive(restorevalue, TRUE);
		modified = 1;
	}
}

static void group_callback(GtkWidget *widget, gpointer data)
{
    struct value_db *value = (struct value_db*) data;
    struct control_db *control;
    control = (struct control_db*)g_object_get_data(G_OBJECT(widget),"ddcc_control");

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		unsigned short val = value->value;
		
		/*printf("Would change %#x to %#x\n", currentControl->address, val);*/
		
		if (control)
		{
			ddcci_writectrl(&mon, control->address, val);
			
//			gtk_widget_set_sensitive(restorevalue, TRUE);
			modified = 1;
		}
	}
    g_slist_foreach(all_controls,refresh_control,0);
}

static void command_callback(GtkWidget *widget, gpointer data)
{
    struct value_db *value = (struct value_db*) data;
    struct control_db *control;
    control = (struct control_db*)g_object_get_data(G_OBJECT(widget),"ddcc_control");

	unsigned short val;
    
    val = value->value;
	
	/*printf("Would change %#x to %#x\n", currentControl->address, val);*/
	
	if (control)
	{
		ddcci_writectrl(&mon, control->address, val);
		
		modified = 1;
	}
    g_slist_foreach(all_controls,refresh_control,0);
}

static void restore_callback(GtkWidget *widget, gpointer data)
{
#if 0
	if (currentControl)
	{
		switch (currentControl->type) {
		case value:
			ddcci_writectrl(&mon, currentControl->address, currentDefault);
			gtk_range_set_value(GTK_RANGE(valuerange), (double)100.0*currentDefault/(double)currentMaximum);
			gtk_widget_set_sensitive(restorevalue, FALSE);
			break;
		case command:
			g_critical(_("restore_callback should not be called for command controls."));
			break;
		case list: {
				GSList* group = valuegroup;
				while(TRUE) {
					if (group == NULL) {
						g_critical(_("Cannot find current control value in value list."));
						return;
					}
					
					printf("group->data@value_db->value == currentDefault (%d == %d)\n",
						((struct value_db*)g_object_get_data(G_OBJECT(group->data), "value_db"))->value, currentDefault);
					if (((struct value_db*)g_object_get_data(G_OBJECT(group->data), "value_db"))->value == currentDefault) {
						gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(group->data), TRUE);
						break;
					}
					
					group = group->next;
				}
				
				ddcci_writectrl(&mon, currentControl->address, currentDefault);
			}
			break;
		}
	}
#endif
}

static void tree_selection_changed_callback(GtkTreeSelection *selection, gpointer data)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    gint  id;
    GtkWidget *notebook;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        gtk_tree_model_get(model,&iter,ID_COL,&id,-1);
        gtk_tree_model_get(model,&iter,WIDGET_COL,&notebook,-1);
        
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),id);
    }
}


// Initializers
////////////////////

void createControl(GtkWidget *parent,struct control_db *control)
{
    unsigned short currentDefault = 1;
    unsigned short currentMaximum = 1;
    get_default_and_max(control,&currentDefault,&currentMaximum);

    GtkWidget *widget;
    
    switch (control->type)
    {
        case value:
            {
                widget = gtk_hscale_new_with_range(0.0, 100.0, 1.0);
                gtk_scale_set_digits(GTK_SCALE(widget), 1);
                g_signal_connect_after(G_OBJECT(widget), "value-changed", G_CALLBACK(range_callback), NULL);
                g_object_set_data(G_OBJECT(widget), "ddcc_control", control);
                
                gtk_range_set_increments(GTK_RANGE(widget), 100.0/(double)currentMaximum, 10.0*100.0/(double)currentMaximum);
                gtk_range_set_value(GTK_RANGE(widget), (double)100.0*currentDefault/(double)currentMaximum);
//              gtk_widget_show(restorevalue);
            }
            break;
        case command:
            {
                widget = gtk_vbox_new(TRUE, 0);
                struct value_db* value;
                for (value = control->value_list; value != NULL; value = value->next)
                {
                    GtkWidget* button = gtk_button_new_with_label(value->name);
                    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(command_callback),value);
                    g_object_set_data(G_OBJECT(button),"ddcc_control",control);
                    gtk_widget_show(button);
                    gtk_box_pack_start(GTK_BOX(widget), button, FALSE, FALSE, 5);
                }
//              gtk_widget_hide(restorevalue);
            }
            break;
        case list:
            {
                widget = gtk_vbox_new(TRUE, 0);
                struct value_db* value;
                GSList* group = NULL;
                for (value = control->value_list; value != NULL; value = value->next)
                {
                    GtkWidget* radio = gtk_radio_button_new_with_label(group, value->name);
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (value->value == currentDefault));
                    g_signal_connect(G_OBJECT(radio), "toggled", G_CALLBACK(group_callback), value);
                    g_object_set_data(G_OBJECT(radio), "ddcc_control", control);
                    gtk_widget_show(radio);
                    gtk_box_pack_start(GTK_BOX(widget), radio, TRUE, TRUE, 0);
                    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
                }
             //   gtk_widget_show(restorevalue);
                
                break;
            }
    }    
    all_controls = g_slist_append(all_controls,widget);
    g_object_set_data(G_OBJECT(widget), "ddcc_control", control);
    g_print("%i - %i\n",all_controls,g_slist_length(all_controls));
    gtk_widget_show(widget);
    gtk_container_add(GTK_CONTAINER(parent),widget);
}
    
void createPage(GtkWidget* notebook, struct subgroup_db* subgroup)
{
    int i=0;
	GtkWidget* vbox = gtk_vbox_new(FALSE,10);
    GtkWidget* frame;

	struct control_db* control;
    for (control = subgroup->control_list; control != NULL; control = control->next)
	{
        frame = gtk_frame_new(control->name);
        gtk_container_set_border_width(GTK_CONTAINER(frame),5);
        createControl(frame,control);
        gtk_widget_show(frame);
        gtk_box_pack_start(GTK_BOX(vbox),frame,0,0,0);
        i++;
    }

	GtkWidget* label;
	label = gtk_label_new(subgroup->name);
	gtk_widget_show (label);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
	gtk_widget_show (vbox);
}


GtkWidget* createTree(GtkWidget *notebook)
{
    GtkTreeStore *store = gtk_tree_store_new(N_COLS,G_TYPE_STRING,G_TYPE_INT,G_TYPE_POINTER);
    
    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(G_OBJECT(store));
    
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Section"),renderer,"text",TITLE_COL,NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree),column);

    GtkTreeIter top_iter;
    GtkTreeIter sub_iter;
    
    struct group_db* group;
	struct subgroup_db* subgroup;

    if (mon.db)
	{
        int id = 1;
		for (group = mon.db->group_list; group != NULL; group = group->next)
        {
            gtk_tree_store_append(store,&top_iter,NULL);
            gtk_tree_store_set(store,&top_iter,TITLE_COL,group->name,ID_COL,0,WIDGET_COL,notebook,-1);
			for (subgroup = group->subgroup_list; subgroup != NULL; subgroup = subgroup->next)
            {
                gtk_tree_store_append(store,&sub_iter,&top_iter);
                gtk_tree_store_set(store,&sub_iter,TITLE_COL,subgroup->name,ID_COL,id,WIDGET_COL,notebook,-1);
                id++;
			}
		}
	}

    gtk_tree_view_expand_all(GTK_TREE_VIEW(tree));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree),0);
   // gtk_container_set_border_width(GTK_CONTAINER(tree),1);
    
    GtkTreeSelection *select;
    
    select = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
    gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (select), "changed",
            G_CALLBACK (tree_selection_changed_callback),
            NULL);

    return tree;
}

GtkWidget* createNotebook(struct monitorlist* monitor)
{
	ddcci_open(&mon, monitor->filename);
	
	GtkWidget *notebook = gtk_notebook_new();
	
	struct group_db* group;
	struct subgroup_db* subgroup;

    GtkWidget *tree = createTree(notebook);

//////////////////////////////	
    GtkWidget* none = gtk_table_new(1, 1, TRUE);
    
	GtkWidget *valuelabel = gtk_label_new (_("Please click on a parameter on the left to change it."));
	gtk_table_attach(GTK_TABLE(none), valuelabel, 0, 1, 0, 1, GTK_FILL_EXPAND, GTK_EXPAND, 5, 5);
    
	GtkWidget *label = gtk_label_new("zero");
    
    gtk_widget_show(valuelabel);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), none,label);
    gtk_widget_show(none);
/////////////////////////////

	if (mon.db)
	{
		for (group = mon.db->group_list; group != NULL; group = group->next)
        {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook),0);
		//	gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook),0);
			
			for (subgroup = group->subgroup_list; subgroup != NULL; subgroup = subgroup->next)
				createPage(notebook, subgroup);
		}
	}
	
	GtkWidget* table = gtk_table_new(2, 1, FALSE);
	gtk_widget_show (tree);
	gtk_table_attach(GTK_TABLE(table), tree, 0, 1, 0, 1, 0, GTK_FILL_EXPAND, 3, 0);
	gtk_widget_show (notebook);
	gtk_table_attach(GTK_TABLE(table), notebook, 1, 2, 0, 1, 0, GTK_FILL_EXPAND, 3, 0);
	gtk_widget_show (table);
	
	return table;
}

// Cleanup
////////////////////

void deleteNotebook() {
	if (modified) {
		ddcci_save(&mon);
	}
	ddcci_close(&mon);
}