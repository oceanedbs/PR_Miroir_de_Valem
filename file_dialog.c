/*

Copyright (C) 2000  Paul Wilkins

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/* file_dialog.c by Paul Wilkins 1/2/2000 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
/* #include <gdk_imlib.h> */

#include "globals.h"
#include "options.h"
#include "render.h"
#include "display.h"
#include "cursor.h"
#include "draw_image.h"
#include "file_dialog.h"


/*   1 == regular
 *   0 == not regular
 *  -1 == does not exist
 */
int
file_type (const char *fname)
{
  struct stat statb;

  /* get the file's size */
  if (stat (fname, &statb) == -1)
    {
      // perror("stat");
      return -1;
    }
  if (S_ISREG (statb.st_mode))
    {
      return 1;
    }
  return 0;
}

int
check_valid_types (char *type)
{
  GdkPixbufFormat *pixbuf_format;
  gchar *fmtname;
  GSList *formats, *l;

  formats = gdk_pixbuf_get_formats ();
  for (l = formats; l; l = g_slist_next (l))
    {
      pixbuf_format = (GdkPixbufFormat *) l->data;

      fmtname = gdk_pixbuf_format_get_name (pixbuf_format);

      if (type)
	{
	  if (0 == strcmp (type, fmtname))
	    return 1;
	}
      else
	{
	  printf ("%s\n", fmtname);
	}
    }

  g_slist_free (formats);
  return 0;
}

char *
get_extension (const char *fname)
{
  char *p1 = strrchr (fname, '.');
  if (p1 == NULL)
    {
      fprintf (stderr, "Error: Can not find file extension\n");
      return NULL;
    }
  return p1 + 1;
}

int
save_image (const char *fname)
{
  char *extension;
  GError *error;

  if (fname == NULL || *fname == '\0' || 0 == file_type (fname))
    {
      fprintf (stderr, "Error: Invalid file name.\n");
      return 0;
    }

  if (globals.out_im == NULL)
    {
      fprintf (stderr, "Error: render an image first.\n");
      return 0;
    }

  extension = get_extension (fname);

  if (0 == check_valid_types (extension))
    {
      fprintf (stderr, "Error: Can not determine file type from extension\n");
      fprintf (stderr, "Valid extension types are:\n");
      check_valid_types (NULL);
      return (0);
    }

  error = NULL;
  if (1 != gdk_pixbuf_save (globals.out_im, fname, extension, &error, NULL))
    {
      fprintf (stderr, "Error: Can't write %s: %s\n", fname, error->message);
      return (0);
    }

  return 1;
}

int
open_image (const char *fname)
{
  GdkPixbuf *im;
  GError *gerror;

  if (fname == NULL || *fname == '\0' || 1 != file_type (fname))
    {
      fprintf (stderr, "Error: Invalid file name.\n");
      return 0;
    }

  /* free old data */
  if (globals.in_im != NULL)
    g_object_unref (globals.in_im);
  if (globals.in_im_scaled != NULL)
    g_object_unref (globals.in_im_scaled);
  globals.in_im = NULL;
  globals.in_im_scaled = NULL;
  free_image_data ();


  /* Load the image specified as the first argument */
  gerror = NULL;
  if (NULL == (im = gdk_pixbuf_new_from_file (fname, &gerror)))
    {
      fprintf (stderr, "Error: Can't load image %s: %s\n", fname,
	       gerror->message);
      return 0;
    }



  /* add an alpha channel */
  globals.in_im = gdk_pixbuf_add_alpha (im, 0, 0, 0, 0);
  globals.in_scaled_im=globals.in_im;
  g_object_unref (im);

  if (NULL == (globals.in_fname = malloc (strlen (fname) + 1)))
    {
      perror ("malloc");
      exit (1);
    }
  strcpy (globals.in_fname, fname);

  if (0 == calc_dimensions (&(globals.new_opt)))
    {
      fprintf (stderr, "Invalid option value.\n");
    }
  copy_opt_data ();
  refresh_options_win (&(globals.new_opt));

  /* scale the image */
  if (NULL == (globals.in_im_scaled = gdk_pixbuf_scale_simple (globals.in_im,
							       globals.
							       cur_opt.pixW *
							       globals.
							       cur_opt.nPixW,
							       globals.
							       cur_opt.pixH *
							       globals.
							       cur_opt.nPixH,
							       //600,600,      
							       GDK_INTERP_BILINEAR)))
    {
      fprintf (stderr, "Error: Unable to scale image: %s\n", fname);
      return 0;
    }

  resize_window ();

  return 1;
}


/* save the rendered image */
void
filesel_save (GtkWidget * w, GtkFileSelection * fs)
{
  cursor_busy ();
  if (1 ==
      save_image (gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs))))
    {
      gtk_widget_hide (GTK_WIDGET (GTK_FILE_SELECTION (fs)));
    }
  cursor_normal ();
}

/* open an image */
void
filesel_open (GtkWidget * w, GtkFileSelection * fs)
{
  cursor_busy ();
  if (1 ==
      open_image (gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs))))
    {
      gtk_widget_hide (GTK_WIDGET (GTK_FILE_SELECTION (fs)));
    }
  cursor_normal ();
}


int
file_open_dialog (char *fname)
{

  static GtkWidget *dialog = NULL;

  if (dialog == NULL)
    {

      /* Create a new file selection widget */
      dialog = gtk_file_selection_new ("Open Image");
      gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (dialog));

      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (dialog)), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed), &dialog);

      /* Connect the ok_button to file_ok_sel function */
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (dialog)->ok_button),
			  "clicked", (GtkSignalFunc) filesel_open, dialog);

      /* Connect the cancel_button to destroy the widget */
      gtk_signal_connect_object (GTK_OBJECT
				 (GTK_FILE_SELECTION (dialog)->cancel_button),
				 "clicked", (GtkSignalFunc) gtk_widget_hide,
				 GTK_OBJECT (dialog));

    }

  if (fname != NULL)
    {
      gtk_file_selection_set_filename (GTK_FILE_SELECTION (dialog), fname);
    }

  if (!GTK_WIDGET_VISIBLE (dialog))
    {
      gtk_widget_show (dialog);
    }

  return 0;
}


int
file_save_dialog (char *fname)
{

  static GtkWidget *dialog = NULL;

  if (dialog == NULL)
    {

      /* Create a new file selection widget */
      dialog = gtk_file_selection_new ("Save Image");
      gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (dialog));

      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (dialog)), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed), &dialog);

      /* Connect the ok_button to file_ok_sel function */
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (dialog)->ok_button),
			  "clicked", (GtkSignalFunc) filesel_save, dialog);

      /* Connect the cancel_button to destroy the widget */
      gtk_signal_connect_object (GTK_OBJECT
				 (GTK_FILE_SELECTION (dialog)->cancel_button),
				 "clicked", (GtkSignalFunc) gtk_widget_hide,
				 GTK_OBJECT (dialog));

    }

  if (fname != NULL)
    {
      gtk_file_selection_set_filename (GTK_FILE_SELECTION (dialog), fname);
    }

  if (!GTK_WIDGET_VISIBLE (dialog))
    {
      gtk_widget_show (dialog);
    }

  return 0;
}
