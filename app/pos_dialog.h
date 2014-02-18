/* Dia -- an diagram creation/manipulation program
 * Copyright (C) 1998 Alexander Larsson
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef POS_DIALOG_H
#define POS_DIALOG_H

#include <glib.h>
#include <gtk/gtk.h>

#include "object.h"
#include "display.h"
#include "diagram.h"

void create_pos_dialog(void);
void pos_dialog_show(void);
void pos_dialog_update(void);
void pos_dialog_set_diagram(Diagram *dia);
Diagram* pos_dialog_get_diagram(void);

#endif /* POS_DIALOG_H */
