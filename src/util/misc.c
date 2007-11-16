/* ASE - Allegro Sprite Editor
 * Copyright (C) 2001-2005, 2007  David A. Capello
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#ifndef USE_PRECOMPILED_HEADER

#include <allegro.h>
#include <string.h>

#include "jinete/list.h"
#include "jinete/manager.h"
#include "jinete/system.h"
#include "jinete/widget.h"

#include "core/app.h"
#include "core/cfg.h"
#include "core/dirs.h"
#include "modules/color.h"
#include "modules/editors.h"
#include "modules/gui.h"
#include "modules/palette.h"
#include "modules/sprites.h"
#include "modules/tools2.h"
#include "raster/blend.h"
#include "raster/cel.h"
#include "raster/image.h"
#include "raster/layer.h"
#include "raster/mask.h"
#include "raster/sprite.h"
#include "raster/stock.h"
#include "raster/undo.h"
#include "util/misc.h"
#include "widgets/editor.h"
#include "widgets/statebar.h"

#endif

Image *GetImage(void)
{
  Sprite *sprite = current_sprite;
  Image *image = NULL;

  if (sprite && sprite->layer && layer_is_image (sprite->layer)) {
    Cel *cel = layer_get_cel(sprite->layer, sprite->frpos);

    if (cel) {
      if ((cel->image >= 0) &&
	  (cel->image < sprite->layer->stock->nimage))
	image = sprite->layer->stock->image[cel->image];
    }
  }

  return image;
}

Image *GetImage2(Sprite *sprite, int *x, int *y, int *opacity)
{
  Image *image = NULL;

  if (sprite && sprite->layer && layer_is_image (sprite->layer)) {
    Cel *cel = layer_get_cel (sprite->layer, sprite->frpos);

    if (cel) {
      if ((cel->image >= 0) &&
	  (cel->image < sprite->layer->stock->nimage))
	image = sprite->layer->stock->image[cel->image];

      if (x) *x = cel->x;
      if (y) *y = cel->y;
      if (opacity) *opacity = MID(0, cel->opacity, 255);
    }
  }

  return image;
}

void LoadPalette(const char *filename)
{
  if (current_sprite) {
    DIRS *dir, *dirs;
    char buf[512];

    dirs = dirs_new ();
    dirs_add_path (dirs, filename);

    usprintf (buf, "palettes/%s", filename);
    dirs_cat_dirs (dirs, filename_in_datadir (buf));

    for (dir=dirs; dir; dir=dir->next) {
      if (exists (dir->path)) {
	RGB *pal = palette_load (dir->path);
	if (pal) {
	  /* just one palette */
	  sprite_reset_palettes(current_sprite);
	  sprite_set_palette(current_sprite, pal, 0);

	  /* set the palette calling the hooks */
	  set_current_palette(pal, FALSE);

	  /* redraw the entire screen */
	  jmanager_refresh_screen();

	  /* free the memory */
	  jfree(pal);
	}
	break;
      }
    }

    dirs_free(dirs);
  }
}

/* clears the mask region in the current sprite with the BG color */
void ClearMask(void)
{
  Sprite *sprite = current_sprite;
  int x, y, u, v, putx, puty;
  unsigned char *address;
  Image *image;
  div_t d;
  int color;

  if (sprite) {
    image = GetImage2 (sprite, &x, &y, NULL);
    if (image) {
      color = get_color_for_image (sprite->imgtype, get_bg_color ());

      if (mask_is_empty (sprite->mask)) {
	if (undo_is_enabled (sprite->undo))
	  undo_image (sprite->undo, image, 0, 0, image->w, image->h);

	/* clear all */
	image_clear (image, color);
      }
      else {
	int x1 = MAX (0, sprite->mask->x);
	int y1 = MAX (0, sprite->mask->y);
	int x2 = MIN (image->w-1, sprite->mask->x+sprite->mask->w-1);
	int y2 = MIN (image->h-1, sprite->mask->y+sprite->mask->h-1);

	/* do nothing */
	if (x1 > x2 || y1 > y2)
	  return;

	if (undo_is_enabled (sprite->undo))
	  undo_image (sprite->undo, image, x1, y1, x2-x1+1, y2-y1+1);

	/* clear the masked zones */
	for (v=0; v<sprite->mask->h; v++) {
	  d = div (0, 8);
	  address = ((unsigned char **)sprite->mask->bitmap->line)[v]+d.quot;

	  for (u=0; u<sprite->mask->w; u++) {
	    if ((*address & (1<<d.rem))) {
	      putx = u+sprite->mask->x-x;
	      puty = v+sprite->mask->y-y;
	      image_putpixel (image, putx, puty, color);
	    }

	    _image_bitmap_next_bit (d, address);
	  }
	}
      }
    }
  }
}

/* returns a new layer created from the current mask in the current
   sprite, the layer isn't added to the sprite */
Layer *NewLayerFromMask(void)
{
  Sprite *sprite = current_sprite;
  unsigned char *address;
  int x, y, u, v, getx, gety;
  Image *dst, *src = GetImage2 (sprite, &x, &y, NULL);
  Layer *layer;
  Cel *cel;
  div_t d;

  if (!sprite || !sprite->mask || !sprite->mask->bitmap || !src)
    return NULL;

  dst = image_new(sprite->imgtype, sprite->mask->w, sprite->mask->h);
  if (!dst)
    return NULL;

  /* clear the new image */
  image_clear(dst, 0);

  /* copy the masked zones */
  for (v=0; v<sprite->mask->h; v++) {
    d = div (0, 8);
    address = ((unsigned char **)sprite->mask->bitmap->line)[v]+d.quot;

    for (u=0; u<sprite->mask->w; u++) {
      if ((*address & (1<<d.rem))) {
	getx = u+sprite->mask->x-x;
	gety = v+sprite->mask->y-y;

	if ((getx >= 0) && (getx < src->w) &&
	    (gety >= 0) && (gety < src->h))
	  dst->method->putpixel (dst, u, v,
				 src->method->getpixel (src, getx, gety));
      }

      _image_bitmap_next_bit (d, address);
    }
  }

  layer = layer_new(sprite->imgtype);
  if (!layer) {
    image_free (dst);
    return NULL;
  }

  layer_set_blend_mode(layer, BLEND_MODE_NORMAL);

  cel = cel_new(sprite->frpos, stock_add_image(layer->stock, dst));
  cel_set_position(cel, sprite->mask->x, sprite->mask->y);

  layer_add_cel(layer, cel);

  return layer;
}

Image *GetLayerImage(Layer *layer, int *x, int *y, int frpos)
{
  Image *image = NULL;

  if (layer_is_image (layer)) {
    Cel *cel = layer_get_cel(layer, frpos);

    if (cel) {
      if ((cel->image >= 0) &&
	  (cel->image < layer->stock->nimage))
	image = layer->stock->image[cel->image];

      if (x) *x = cel->x;
      if (y) *y = cel->y;
    }
  }

  return image;
}

/* Gives to the user the possibility to move the sprite's layer in the
   current editor, returns TRUE if the position was changed.  */

int interactive_move_layer (int mode, int use_undo, int (*callback) (void))
{
  JWidget editor = current_editor;
  Sprite *sprite = editor_get_sprite (editor);
  Layer *layer = sprite->layer;
  Cel *cel = layer_get_cel(layer, sprite->frpos);
  int start_x, new_x;
  int start_y, new_y;
  int start_b;
  int ret;
  int update = FALSE;
  int quiet_clock = -1;
  int first_time = TRUE;
  int begin_x;
  int begin_y;
  int delay;

  if (!cel)
    return FALSE;

  begin_x = cel->x;
  begin_y = cel->y;

  delay = get_config_int ("Options", "MoveDelay", 250);
  delay = MID (0, delay, 1000);
  delay = JI_TICKS_PER_SEC * delay / 1000;

  hide_drawing_cursor(editor);
  jmouse_set_cursor(JI_CURSOR_MOVE);

  editor_click_start(editor, mode, &start_x, &start_y, &start_b);

  do {
    if (update) {
      cel->x = begin_x - start_x + new_x;
      cel->y = begin_y - start_y + new_y;

      /* update layer-bounds */
      jmouse_hide();
      editor_update_layer_boundary(editor);
      editor_draw_layer_boundary_safe(editor);
      jmouse_show();

      /* update status bar */
      status_bar_set_text
	(app_get_status_bar(), 0,
	 "Pos %3d %3d Offset %3d %3d",
	 (int)cel->x,
	 (int)cel->y,
	 (int)(cel->x - begin_x),
	 (int)(cel->y - begin_y));
      jwidget_flush_redraw(app_get_status_bar());
      jmanager_dispatch_messages();

      /* update clock */
      quiet_clock = ji_clock;
      first_time = FALSE;
    }

    /* call the user's routine */
    if (callback) {
      if ((*callback)())
	quiet_clock = delay;
    }

    /* this control the redraw of the sprite when the cursor is quiet
       for some time */
    if ((quiet_clock >= 0) && (ji_clock-quiet_clock >= delay)) {
      quiet_clock = -1;
      jwidget_dirty(editor);
      jwidget_flush_redraw(editor);
      jmanager_dispatch_messages();
    }

    gui_feedback();
  } while (editor_click(editor, &new_x, &new_y, &update, NULL));

  /* the position was changed */
  if (!editor_click_cancel(editor)) {
    if (use_undo && undo_is_enabled(sprite->undo)) {
      new_x = cel->x;
      new_y = cel->y;

      undo_open(sprite->undo);
      cel->x = begin_x;
      cel->y = begin_y;
      undo_int(sprite->undo, (GfxObj *)cel, &cel->x);
      undo_int(sprite->undo, (GfxObj *)cel, &cel->y);
      cel->x = new_x;
      cel->y = new_y;
      undo_close(sprite->undo);
    }

    ret = TRUE;
  }
  /* the position wasn't changed */
  else {
    cel->x = begin_x;
    cel->y = begin_y;

    ret = FALSE;
  }

  /* redraw the sprite in all editors */
  GUI_Refresh(sprite);

  /* restore the cursor */
  show_drawing_cursor(editor);

  editor_click_done(editor);

  return ret;
}

