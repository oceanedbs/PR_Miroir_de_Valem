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
/* draw_image.c by Paul Wilkins 1/2/2000 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <gtk/gtk.h>
/* #include <gdk_imlib.h> */

#include "globals.h"
#include "render_image.h"
#include "read_db.h"
#include "draw_image.h"


int
check_options (struct IMAGE_OPTIONS *im_opt)
{
  int ret = 1;
  if (im_opt->opt_alg == PIX_SIZE)
    {
      if (im_opt->pixW < 1)
	{
	  printf ("Image size X: invalid number\n");
	  ret = 0;
	}
      if (im_opt->pixH < 1)
	{
	  printf ("Image size Y: invalid number\n");
	  ret = 0;
	}
    }
  else
    {
      if (im_opt->nPixW < 1)
	{
	  printf ("Image count X: invalid number\n");
	  ret = 0;
	}
      if (im_opt->nPixH < 1)
	{
	  printf ("Image count Y: invalid number\n");
	  ret = 0;
	}
    }
  if (-1 == im_opt->proximity)
    {
      printf ("Image proximity: invalid number\n");
      ret = 0;
    }

  return ret;
}

int
calc_dimensions (struct IMAGE_OPTIONS *im_opt)
{

  if (0 == check_options (im_opt))
    {
      printf ("error with options\n");
      return 0;
    }

  if (im_opt->opt_alg == PIX_SIZE)
    {
      if (globals.in_im == NULL)
	{
	  im_opt->nPixW = 0;
	  im_opt->nPixH = 0;
	}
      else
	{
	  im_opt->nPixW =
	    ceil ((double) gdk_pixbuf_get_width (globals.in_im) /
		  (double) im_opt->pixW);
	  im_opt->nPixH =
	    ceil ((double) gdk_pixbuf_get_height (globals.in_im) /
		  (double) im_opt->pixH);
	}
    }
  else if (im_opt->opt_alg == PIX_COUNT)
    {
      if (globals.in_im == NULL)
	{
	  im_opt->pixW = 0;
	  im_opt->pixH = 0;
	}
      else
	{
	  im_opt->pixW =
	    ceil ((double) gdk_pixbuf_get_width (globals.in_im) /
		  (double) im_opt->nPixW);
	  im_opt->pixH =
	    ceil ((double) gdk_pixbuf_get_height (globals.in_im) /
		  (double) im_opt->nPixH);
	}
    }
  else
    {
      fprintf (stderr, "calc_dimensions: I shouldn't get here.\n");
      exit (1);
    }

  im_opt->width = im_opt->nPixW * im_opt->pixW;
  im_opt->height = im_opt->nPixH * im_opt->pixH;

  return 1;
}

int
copy_opt_data ()
{
  int ret = 0;
  if (globals.cur_opt.width != globals.new_opt.width)
    ret = 1;
  if (globals.cur_opt.height != globals.new_opt.height)
    ret = 1;

  globals.cur_opt.pixW = globals.new_opt.pixW;
  globals.cur_opt.pixH = globals.new_opt.pixH;
  globals.cur_opt.nPixW = globals.new_opt.nPixW;
  globals.cur_opt.nPixH = globals.new_opt.nPixH;
  globals.cur_opt.width = globals.new_opt.width;
  globals.cur_opt.height = globals.new_opt.height;
  globals.cur_opt.opt_alg = globals.new_opt.opt_alg;
  globals.cur_opt.proximity = globals.new_opt.proximity;

  return ret;
}

void
redraw_screen (int x, int y, int w, int h)
{
  int width, height;
  GtkStyle *style;

  /* return if o image has been opened */
  if (globals.in_im == NULL)
    return;

  /* copy it to the screen */
  style = gtk_widget_get_style (GTK_WIDGET (globals.picDA));

  width =
    gdk_pixbuf_get_width ((globals.image
			   && globals.show_rendered) ? globals.
			  out_im : globals.in_im_scaled);
  height =
    gdk_pixbuf_get_height ((globals.image
			    && globals.show_rendered) ? globals.
			   out_im : globals.in_im_scaled);

  // modify width and height such that we don't ask for portions of the image that don't exist
  if (x + w > width)
    w = width - x;
  if (w < 0)
    w = 0;
  if (y + h > height)
    h = height - y;
  if (h < 0)
    h = 0;

  gdk_draw_pixbuf (GTK_WIDGET (globals.picDA)->window, style->fg_gc[GTK_STATE_NORMAL], (globals.image && globals.show_rendered) ? globals.out_im : globals.in_im_scaled, x, y,	// src x, y
		   x, y,	// dst x, y
		   w, h, GDK_RGB_DITHER_NORMAL, 0, 0	// dither offset
    );
}


int
change_small_image (int xx, int yy)
{
  struct PIC_DB *db;
  struct IMAGE_INFO *inf;
  GError *gerror;
  GdkPixbuf *im;
  GdkPixbuf *scale_im;
  GdkPixbuf *scale_im_alpha;

  if (globals.image != NULL)
    {
      inf = &(globals.image[yy][xx]);
      db = inf->matches[inf->match_no];

      /* load the image */
      gerror = NULL;
      if (NULL == (im = gdk_pixbuf_new_from_file (db->fname, &gerror)))
	{
	  fprintf (stderr, "Error: Can't load image %s: %s\n", db->fname,
		   gerror->message);
	  return 0;
	}

      /* scale the image */
      if (NULL ==
	  (scale_im =
	   gdk_pixbuf_scale_simple (im, globals.cur_opt.pixW,
				    globals.cur_opt.pixH,
				    GDK_INTERP_BILINEAR)))
	{
	  fprintf (stderr, "Error: Unable to scale image %s\n", db->fname);
	  return 0;
	}

      /* add an alpha channel */
      scale_im_alpha = gdk_pixbuf_add_alpha (scale_im, 0, 0, 0, 0);

      draw_small_image (scale_im_alpha, xx, yy);

      copy_image_RGB (scale_im_alpha, globals.out_im, xx, yy);

      g_object_unref (im);
      g_object_unref (scale_im);
      g_object_unref (scale_im_alpha);

    }
  return 1;
}

int
draw_small_image (GdkPixbuf * scale_im, int x, int y)
{
  GtkStyle *style;

  style = gtk_widget_get_style (GTK_WIDGET (globals.picDA));

  gdk_draw_pixbuf (GTK_WIDGET (globals.picDA)->window, style->fg_gc[GTK_STATE_NORMAL], scale_im, 0, 0,	// src x, y
		   globals.cur_opt.pixW * x, globals.cur_opt.pixH * y, globals.cur_opt.pixW, globals.cur_opt.pixH, GDK_RGB_DITHER_NORMAL, 0, 0	// dither offset
    );

  return 1;
}
