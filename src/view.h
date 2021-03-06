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
*/

/*
The pdfview class has been modified extensively to allow for multiple
columns visualization of a document. You can think of this view as a
matrix of pages, each line of this matrix composed of as many columns
as required by the user.

Modifications Copyright (C) 2016 Guy Turcotte
*/

#ifndef VIEW_H
#define VIEW_H

#include "main.h"

#define CACHE_MAX 15
#define PAGES_ON_SCREEN_MAX 50

// Used to keep drawing postion of displayed pages to
// help in the identification of the selection zone.
// Used by the end_of_selection method.
struct page_pos_struct {
  float zoom, ratio_x, ratio_y;
  u32 page;
  int X0, Y0, W0, H0, X, Y, W, H;
};

enum trim_zone_loc_enum { 
  TZL_N = 0, 
  TZL_S, 
  TZL_E, 
  TZL_W, 
  TZL_NE, 
  TZL_NW, 
  TZL_SE, 
  TZL_SW, 
  TZL_NONE
};

class PDFView: public Fl_Widget {
public:
  PDFView(int x, int y, int w, int h);
  void draw();
  int  handle(int e);

  void goto_page(const float page);
  void reset();
  void text_select(bool do_select);
  void trim_zone_select(bool do_select);
  void trim_zone_different(bool diff);
  void this_page_trim(bool this_page);
  void clear_all_single_trims();
  void set_columns(u32 count);
  void set_title_page_count(u32 count);
  void set_params(recent_file_struct &recent);
  void new_file_loaded();
  void page_up();
  void page_down();
  void page_top();
  void page_bottom();

  void mode(view_mode_enum m);
  inline float zoom() { return view_zoom; };
  inline void  zoom(float zoom) { view_zoom = zoom > 0.1f ? (zoom < 10.0f ? zoom : 10.0f) : 0.1f; };
  inline view_mode_enum  mode() { return view_mode; };
  inline float get_xoff() { return xoff; };
  inline float get_yoff() { return yoff; };
  inline u32   get_columns() { return columns; };
  inline u32   get_title_page_count() { return title_pages; };
  inline my_trim_struct & get_my_trim() { return my_trim; };
  inline bool is_single_page_trim() { return single_page_trim; };

private:
  void  reset_selection(bool anyway = false);
  void  select_page_at(s32 X, s32 Y, bool right_dclick);
  void  page_changed();
  void  clear_my_trim();
  void  compute_screen_size();
  float line_zoom_factor(u32 first_page, u32 &width,u32 &height) const;
  void  update_visible() const;
  u8    iscached(const u32 page) const;
  void  docache(const u32 page);
  float maxyoff() const;
  u32   pxrel(u32 page) const;
  void  content(const u32 page, const s32 X, const s32 y, const u32 w, const u32 h);
  void  adjust_yoff(float offset);
  void  adjust_floor_yoff(float offset);
  void  end_of_selection();
  void  add_single_page_trim(s32 page, s32 X, s32 Y, s32 W, s32 H);
  void  remove_single_page_trim(s32 page);
  trim_zone_loc_enum get_trim_zone_loc(s32 x, s32 y) const;
  const trim_struct * get_trimming_for_page(s32 page) const;
  void  show_trim();
  u32   pageh(u32 page) const;
  u32   pagew(u32 page) const;
  u32   fullh(u32 page) const;
  u32   fullw(u32 page) const;

  float  view_zoom;
  u32    left_dclick_columns_count, right_dclick_columns_count;
  bool   text_selection;
  bool   trim_zone_selection;
  bool   single_page_trim;
  view_mode_enum view_mode;

  float  yoff, xoff;
  u32    cachedsize;
  u8   * cache[CACHE_MAX];
  u16    cachedpage[CACHE_MAX];
  Pixmap pix[CACHE_MAX];

  page_pos_struct page_pos_on_screen[PAGES_ON_SCREEN_MAX];
  u32    page_pos_count;

  // Text selection coords
  u16 selx, sely, selx2, sely2, savedx, savedy;
  u32 columns, title_pages;

  s32 screen_x, screen_y, screen_width, screen_height;
  my_trim_struct my_trim;
};

#endif
