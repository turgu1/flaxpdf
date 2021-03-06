/*
Copyright (C) 2015 Lauri Kasanen

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 3 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.


Modifications Copyright (C) 2016 Guy Turcotte
*/

#ifndef MAIN_H
#define MAIN_H

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lzo/lzo1x.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Double_Window.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input_Choice.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Light_Button.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Select_Browser.H>
#include <FL/Fl_File_Icon.H>
#include <FL/x.H>

#include <PDFDoc.h>

#define DEBUGGING 1

#include "globals.h"
#include "autoconfig.h"
#include "gettext.h"
#include "lrtypes.h"
#include "macros.h"
#include "config.h"
#include "view.h"
#include "helpers.h"

extern Fl_Box * debug1, 
              * debug2, 
              * debug3, 
              * debug4, 
              * debug5, 
              * debug6, 
              * debug7;

extern u8 details;

extern int writepipe;

bool loadfile(const char *, recent_file_struct * recent_files);

const int MAX_COLUMNS_COUNT = 5;

struct cachedpage {
  u8  * data;
  u32   size;
  u32   uncompressed;

  u32   w, h;
  u16   left, right, top, bottom;

  bool  ready;
};

enum msg {
  MSG_REFRESH = 0,
  MSG_READY
};

struct openfile {
  char       * filename;
  cachedpage * cache;
  PDFDoc     * pdf;
  u32          maxw, maxh;

  u32          pages;

  u32          first_visible;
  u32          last_visible;

  pthread_t    tid;
};

extern openfile * file;

void cb_hide_show_buttons(Fl_Widget *, void *);
void update_buttons();

#define FL_NONO_FONT FL_FREE_FONT

#endif
