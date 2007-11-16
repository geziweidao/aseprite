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

#ifndef UTIL_THMBNAIL_H
#define UTIL_THMBNAIL_H

struct Cel;
struct BITMAP;

#define THUMBNAIL_W	32
#define THUMBNAIL_H	32

typedef struct Thumbnail {
  struct Cel *cel;
  struct BITMAP *bmp;
} Thumbnail;

void destroy_thumbnails(void);
struct BITMAP *generate_thumbnail(struct Cel *cel, struct Layer *layer);

#endif /* UTIL_THMBNAIL_H */

