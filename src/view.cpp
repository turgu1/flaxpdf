/*
Copyright (C) 2015 Lauri Kasanen
Modifications Copyright (C) 2016 Guy Turcotte

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
The PDFView class has been modified extensively to allow for multiple
columns visualization of a document. You can think of this view as a
matrix of pages, each line of this matrix composed of as many columns
as required by the user.

*/

#include "view.h"
#include <TextOutputDev.h>

// Quarter inch in double resolution
#define MARGIN 36
#define MARGINHALF 18

PDFView::PDFView(int x, int y, int w, int h): Fl_Widget(x, y, w, h),
    text_selection(false), 
    trim_zone_selection(false), 
    single_page_trim(false),
    view_mode(Z_PAGE), 
    yoff(0), xoff(0),
    selx(0), sely(0), selx2(0), sely2(0),
    columns(1), 
    title_pages(0)
{
  cachedsize = 7 * 1024 * 1024; // 7 megabytes

  my_trim.initialized = false;
  my_trim.similar = true;

  u32 i;
  for (i = 0; i < CACHE_MAX; i++) {
    cache[i] = (u8 *) xcalloc(cachedsize, 1);
    cachedpage[i] = USHRT_MAX;
    pix[i] = None;
  }
}

// User requested trimming zone selection (or not if do_select is false).
// This is in support of the Z_MYTRIM view_mode
void PDFView::trim_zone_select(bool do_select)
{
  static float saved_xoff;
  static float saved_yoff;
  static u32   saved_columns;
  static bool  initialized = false;

  trim_zone_selection = (view_mode == Z_MYTRIM) && do_select;

  if (view_mode == Z_MYTRIM) {
  
    if (trim_zone_selection) {
    
      saved_xoff     = xoff;
      saved_yoff     = yoff;
      saved_columns  = columns;
      initialized    = true;
      columns        = 1;
      text_selection = false;
      yoff           = floorf(yoff);

      if (!my_trim.initialized) {
        s32 page = yoff;

        my_trim.initialized = true;
        my_trim.similar     = true;
        my_trim.singles     = NULL;

        // Insure that the initial trim zone rectangle will be visible on screen
        // We offset it of 25 pixels from the visible edges of the page
        my_trim.odd.X = my_trim.even.X = file->cache[page].left + 25;
        my_trim.odd.Y = my_trim.even.Y = file->cache[page].top  + 25;
        my_trim.odd.W = my_trim.even.W = file->cache[page].w - 50;
        my_trim.odd.H = my_trim.even.H = file->cache[page].h - 50;
      }

      reset_selection();
      redraw();
    }
    else if (initialized) {
    
      xoff    = saved_xoff;
      yoff    = saved_yoff;
      columns = saved_columns;

      reset_selection();
      redraw();
    }
  }
}

void PDFView::trim_zone_different(bool diff)
{
  bool was_similar = my_trim.similar;
  my_trim.similar = !diff;
  
  if (!was_similar && my_trim.similar) {
    s32 page = yoff;

    if (page & 1) {
      my_trim.odd = my_trim.even;
    }
    else {
      my_trim.even = my_trim.odd;
    }
  }
}

void PDFView::add_single_page_trim(s32 page, s32 X, s32 Y, s32 W, s32 H)
{
  single_page_trim_struct *prev, *curr, *s;

  prev = NULL;
  curr = my_trim.singles;
  while (curr && (curr->page < page)) {
    prev = curr;
    curr = curr->next;
  }
  
  if ((curr == NULL) || (curr->page != page)) {
    s = (single_page_trim_struct *) xmalloc(sizeof(single_page_trim_struct));
    s->next = curr;
    if (prev)
      prev->next = s;
    else
      my_trim.singles = s;
    curr = s;
  }
  
  curr->page        = page;
  curr->page_trim.X = X;
  curr->page_trim.Y = Y;
  curr->page_trim.W = W;
  curr->page_trim.H = H;
}

void PDFView::this_page_trim(bool this_page)
{
  single_page_trim = this_page;
}

// User requested text selection to copy to clipboard (or not if do_select is false)
void PDFView::text_select(bool do_select)
{
  text_selection = do_select;
  trim_zone_selection = false;
  reset_selection();
  redraw();
}

void PDFView::reset_selection() 
{
  if (!trim_zone_selection) {
    selx = sely = selx2 = sely2 = 0;
  }
}

void PDFView::compute_screen_size()
{
  screen_x      = x();
  screen_y      = y();
  screen_height = h();
  screen_width  = w();
}


void PDFView::go(const float page) 
{
  yoff = page;
  adjust_yoff(0);
  reset_selection();
  redraw();
}

void PDFView::set_columns(u32 count) 
{
  if ((count >= 1) && (count <= 5)) {
    columns = count;
    reset_selection();
    redraw();
  }
}

void PDFView::set_title_page_count(u32 count) 
{
  if (count <= 4) {
    title_pages = count;
    reset_selection();
    redraw();
  }
}

void PDFView::clear_my_trim()
{
  if (my_trim.initialized) {
    my_trim.initialized = false;
    my_trim.similar = true;

    clear_singles(my_trim);
  }
  my_trim.singles = NULL;
}


void PDFView::new_file_loaded()
{
  clear_my_trim();
}

void PDFView::show_trim()
{
  #if DEBUGGING
    debug("---\nTrim: Initialized: %s, Similar: %s, Singles: %s\n",
      my_trim.initialized ? "Yes" : "No",
      my_trim.similar ? "Yes" : "No",
      my_trim.singles ? "Yes" : "No");
    debug("Odd: %d %d %d %d\n",
      my_trim.odd.X,
      my_trim.odd.Y,
      my_trim.odd.W,
      my_trim.odd.H);
    debug("Even: %d %d %d %d\n",
      my_trim.even.X,
      my_trim.even.Y,
      my_trim.even.W,
      my_trim.even.H);

    single_page_trim_struct * curr = my_trim.singles;

    while (curr) {
      debug("Single page %d: %d %d %d %d\n",
        curr->page,
        curr->page_trim.X,
        curr->page_trim.Y,
        curr->page_trim.W,
        curr->page_trim.H);
      curr = curr->next;
    }
    debug("---\n");
  #endif
}

void PDFView::set_params(recent_file_struct &recent) 
{
  yoff        = recent.yoff;
  xoff        = recent.xoff;
  view_mode   = recent.view_mode;
  title_pages = recent.title_page_count;

  clear_my_trim();
  my_trim     = recent.my_trim;

  copy_singles(recent.my_trim, my_trim);

  show_trim();

  set_columns(recent.columns); 
  go(yoff);
}

void PDFView::reset() 
{
  yoff = 0;
  xoff = 0;

  adjust_yoff(0);

  reset_selection();

  u32 i;
  for (i = 0; i < CACHE_MAX; i++) {
    cachedpage[i] = USHRT_MAX;
  }
}

const trim_struct * PDFView::get_trimming_for_page(s32 page, bool update_screen) const;
{
  single_page_trim_struct * curr = my_trim.singles;
  const trim_struct * result;

  debug("Finding trimming data for page %d\n", page);
  while (curr && (curr->page < page)) curr = curr->next;

  if (curr && (curr->page == page)) {
    debug("Specific trim for page: %d\n", page);
    result = &curr->page_trim;
  } 
  else {
    if (page & 1) {
      debug("Even...\n");
      result = &my_trim.even;
    }
    else {
      debug("Odd...\n");
      result = &my_trim.odd;
    }
  }

  return result;
}

u32 PDFView::pageh(u32 page) const 
{
  if (!file->cache[page].ready) page = 0;

  if (view_mode == Z_TRIM || view_mode == Z_PGTRIM) {
    return file->cache[page].h;
  }
  else if (view_mode == Z_MYTRIM && !trim_zone_selection) {
    if (my_trim.initialized) {
      const trim_struct * the_trim = get_trimming_for_page(page, false);
      return  the_trim->H;
    }
    else
      return file->cache[page].h;
  }
  else { // Z_PAGE || Z_WIDTH || Z_CUSTOM || (Z_MYTRIM && trim_zone_selection)
    return  file->cache[page].h +
        file->cache[page].top   +
        file->cache[page].bottom;
  }
}

u32 PDFView::pagew(u32 page) const 
{
  if (!file->cache[page].ready) page = 0;

  if (view_mode == Z_TRIM || view_mode == Z_PGTRIM) {
    return file->cache[page].w;
  }
  else if (view_mode == Z_MYTRIM && !trim_zone_selection) {
    if (my_trim.initialized) {
      const trim_struct * the_trim = get_trimming_for_page(page, false);
      return  the_trim->W;
    }
    else {
      return file->cache[page].w;
    }
  }
  else { // Z_PAGE || Z_WIDTH || Z_CUSTOM || (Z_MYTRIM && trim_zone_selection)
    return file->cache[page].w +
        file->cache[page].left  +
        file->cache[page].right;
  }
}

// Compute the height of a line of pages, selecting the page that is the
// largest in height.
u32 PDFView::fullh(u32 page) const 
{
  if (!file->cache[page].ready) page = 0;

  u32 fh = 0;
  u32 h;
  u32 i, limit;

  if ((title_pages > 0) && (title_pages < columns) && (page < title_pages)) {
    limit = title_pages;
  }
  else {
    limit = page + columns;
  }

  if (limit > file->pages) limit = file->pages;

  for (i = page; i < limit; i++) {
    h = pageh(i);
    if (h > fh) fh = h;
  }
  
  // if (view_mode == Z_TRIM || view_mode == Z_PGTRIM || (view_mode == Z_MYTRIM && !trim_zone_selection))
  //   for (i = page; i < limit; i++) {
  //     if (file->cache[i].ready && (file->cache[i].h > fh))
  //       fh = file->cache[i].h;
  //   }
  // else 
  //   for (i = page; i < limit; i++) {
  //     if (file->cache[i].ready) {
  //       u32 h = file->cache[i].h +
  //           file->cache[i].top   +
  //           file->cache[i].bottom;
  //       if (h > fh)
  //         fh = h;
  //     }
  //   }               

  return fh;
}

// Compute the width of a line of pages. The page number is the one that is
// on the first on the left of the line.
u32 PDFView::fullw(u32 page) const 
{
  u32 fw = 0;
  u32 i, limit;

  if ((title_pages > 0) && (title_pages < columns) && (page < title_pages)) {
    limit = title_pages;
  }
  else {
    limit = page + columns;
  }

  if (limit > file->pages) limit = file->pages;

  for (i = page; i < (page + columns); i++) {
    fw += pagew(i < limit ? i : 0);
  }
  
  // if (view_mode == Z_TRIM || view_mode == Z_PGTRIM || (view_mode == Z_MYTRIM && !trim_zone_selection)) {
  //   for (i = page; i < (page + columns); i++) {
  //     if (file->cache[i].ready && (i < limit))
  //       fw += file->cache[i].w;
  //     else
  //       fw += file->cache[0].w;
  //   }
  // }
  // else {
  //   for (i = page; i < (page + columns); i++) {
  //     if (file->cache[i].ready && (i < limit)) 
  //       fw += file->cache[i].w    +
  //           file->cache[i].left +
  //           file->cache[i].right;
  //     else
  //       fw += file->cache[0].w    +
  //           file->cache[0].left +
  //           file->cache[0].right;
  //   }
  // }

  // Add the margins between columns
  return fw + (columns - 1) * MARGINHALF;
}

static bool hasmargins(const u32 page) 
{
  if (!file->cache[page].ready) {
    return
      file->cache[0].left   > MARGIN ||
      file->cache[0].right  > MARGIN ||
      file->cache[0].top    > MARGIN ||
      file->cache[0].bottom > MARGIN;
  }

  return
    file->cache[page].left   > MARGIN ||
    file->cache[page].right  > MARGIN ||
    file->cache[page].top    > MARGIN ||
    file->cache[page].bottom > MARGIN;
}

// Compute the required zoom factor to fit the line of pages on the screen,
// According to the zoom mode parameter if not a custom zoom.
float PDFView::line_zoom_factor(const u32 first_page, u32 &width, u32 &height) const 
{
  const u32 line_width  = fullw(first_page);
  const u32 line_height = fullh(first_page);

  float zoom_factor;

  switch (view_mode) {
    case Z_TRIM:
    case Z_WIDTH:
      zoom_factor = (float)screen_width / line_width;
      break;
    case Z_PAGE:
    case Z_PGTRIM:
    case Z_MYTRIM:
      if (((float)line_width / line_height) > ((float)screen_width / screen_height)) {
        zoom_factor = (float)screen_width / line_width;
      }
      else {
        zoom_factor = (float)screen_height / line_height;
      }
      break;
    default:
      zoom_factor = file->zoom;
      break;
  }

  width  = line_width;
  height = line_height;

  file->zoom = zoom_factor;

  return zoom_factor;
}

void PDFView::update_visible(const bool fromdraw) const 
{
  // From the current zoom mode and view offset, update the visible page info
  // Will adjust the following parameters:
  //
  // - file->first_visible
  // - file->last_visible
  // - page_input->value <- currently visibale page number in the upper left of the screen
  //
  // This method as been extensively modified to take into account multicolumns
  // and the fact that no page will be expected to be of the same size as the others,
  // both for vertical and horizontal limits. Because of these constraints, the
  // zoom factor cannot be applied to the whole screen but for each "line" of
  // pages.

  const u32 prev = file->first_visible;

  // Those are very very conservative numbers to compute last_visible. 
  // The end of the draw process will adjust precisely the value
  static const u32 max_lines_per_screen[MAX_COLUMNS_COUNT] = { 2, 3, 4, 5, 6 };

  // Adjust file->first_visible

  file->first_visible = yoff < 0 ? 0 : yoff;
  if (file->first_visible > file->pages - 1) {
    file->first_visible = file->pages - 1;
  }

  // Adjust file->last_visible

  u32 new_last_visible = file->first_visible + max_lines_per_screen[columns - 1] * columns;
  if (new_last_visible >= file->pages) {
    file->last_visible = file->pages - 1;
  }
  else {
    file->last_visible = new_last_visible;
  }

  // If position has changed:
  //    - update current page visible number in the page_input
  //    - request a redraw (if not already called by draw...)
  if (prev != file->first_visible) {
    char buf[10];
    snprintf(buf, 10, "%u", file->first_visible + 1);
    page_input->value(buf);

    if (!fromdraw) page_input->redraw();

    Fl::check();
  }
}

// Compute the vertical screen size of a line of pages
u32 PDFView::pxrel(u32 page) const 
{
  float zoom;
  u32 line_width, line_height;
  
  zoom = line_zoom_factor(page, line_width, line_height);
  return line_height * zoom;
}

void PDFView::draw() 
{
  if (!file->cache) return;

  compute_screen_size();
  update_visible(true);

  const Fl_Color pagecol = FL_WHITE;
  int X, Y, W, H;
  int Xs, Ys, Ws, Hs; // Saved values

  fl_clip_box(screen_x, screen_y, screen_width, screen_height, X, Y, W, H);

  // If nothing is visible on screen, nothing to do
  if (W == 0 || H == 0)
    return;

  fl_overlay_clear();

  // Paint the background with the page separation color
  fl_rectf(X, Y, W, H, FL_GRAY + 1);

  struct cachedpage *cur = &file->cache[file->first_visible];

  if (!cur->ready) return;

  // insure that nothing will be drawned outside the clipped area
  fl_push_clip(X, Y, W, H);

  if (view_mode != Z_CUSTOM) xoff = 0.0f;

  // As the variables are used for calculations, reset them to full widget dimensions.
  // (doesn´t seems to be needed... they are adjusted to something else in the code
  //  that follow...)

  s32 current_screen_vpos = 0; // Current vertical position in the screen space
  u32 first_page_in_line = file->first_visible;

  bool first_line = true;
  bool first_page = true;

  const float invisibleY = yoff - floorf(yoff);

  // pp will acquire all topological information required to identify the selection
  // made by the user with mouse movements
  page_pos_struct *pp = page_pos_on_screen;
  page_pos_count = 0;

  u32 page = file->first_visible;

  // Do the following for each line of pages
  while (current_screen_vpos < screen_height && (first_page_in_line < file->pages)) {

    float zoom;
    u32   line_width, line_height;

    zoom = line_zoom_factor(
      /* in  */ first_page_in_line, 
      /* out */ line_width, 
      /* out */ line_height);

    const int zoomedmargin     = zoom * MARGIN;
    const int zoomedmarginhalf = zoomedmargin / 2;

    H = line_height * zoom; // Line of pages height in screen pixels

    if (first_line) {
      Y = screen_y - invisibleY * H;
    }

    X = screen_x + 
        screen_width / 2 - 
        zoom * line_width / 2 + 
        (zoom * xoff * line_width);

    // Do the following for each page in the line

    s32 column = 0;
    s32 limit = columns;
    page = first_page_in_line;

    if ((title_pages > 0) && (title_pages < columns) && (page == 0)) 
      limit = title_pages;

    // Do the following for each page in the line
    while ((column < limit) && (page < file->pages)) {

      cur = &file->cache[page];
      if (!cur->ready)
        break;

      H = pageh(page) * zoom;
      W = pagew(page) * zoom;

      // Paint the page backgroud rectangle, save coordinates for next loop
      fl_rectf(Xs = X, Ys = Y, Ws = W, Hs = H, pagecol);

      #if DEBUGGING
        if (first_page) {
          debug("Zoom factor: %f\n", zoom);
          debug("Page data: Left: %d Right: %d Top: %d Bottom: %d W: %d H: %d\n",
            cur->left, cur->right, cur->top, cur-> bottom, cur->w, cur->h);
          debug("Page screen rectangle: X: %d Y: %d W: %d H: %d\n", X, Y, W, H);
        }
      #endif

      const bool margins = hasmargins(page);
      const bool trimmed = 
        (margins && ((view_mode == Z_TRIM) || (view_mode == Z_PGTRIM))) || 
        ((view_mode == Z_MYTRIM) && !trim_zone_selection);

      float ratio_x = 1.0f;
      float ratio_y = 1.0f;

      if ((view_mode == Z_MYTRIM) && 
          !trim_zone_selection && 
          my_trim.initialized) {

        s32 a, b;

        H = zoom * cur->h;
        W = zoom * cur->w;

        const trim_struct * the_trim = get_trimming_for_page(page);

        X -= zoom * (the_trim->X - cur->left);
    	  Y -= zoom * (the_trim->Y - cur->top);  

        fl_push_clip(
          Xs + zoomedmarginhalf, 
          Ys + zoomedmarginhalf, 
          a = zoom * the_trim->W - zoomedmargin, 
          b = zoom * the_trim->H - zoomedmargin);
          
        #if DEBUGGING
          if (first_page) {
            debug("My Trim: X: %d Y: %d W: %d H: %d\n",
              the_trim->X,
              the_trim->Y,
              the_trim->W,
              the_trim->H);
            debug("Clipping: X: %d, Y: %d, W: %d, H: %d\n", Xs, Ys, a, b);
          }
        #endif
      }

      if (trimmed) {
        // If the page was trimmed, have the real one a bit smaller
        X += zoomedmarginhalf;
        Y += zoomedmarginhalf;
        W -= zoomedmargin;
        H -= zoomedmargin;

        // These changes in size have an impact on the zoom factor
        // previously used. We need to keep track of these changes
        // to help into the selection processes.
        ratio_x = W / (float)(W + zoomedmargin);
        ratio_y = H / (float)(H + zoomedmargin);
      } 
      else if (margins) {
        // Restore the full size with empty borders
        X +=  cur->left                * zoom;
        Y +=  cur->top                 * zoom;
        W -= (cur->left + cur->right ) * zoom;
        H -= (cur->top  + cur->bottom) * zoom;
      }

      // Render real content
      #if DEBUGGING
        if (first_page) {
          debug("Drawing page %d: X: %d, Y: %d, W: %d, H: %d\n", page, X, Y, W, H);
        }
      #endif

      content(page, X, Y, W, H);

      if (view_mode == Z_MYTRIM && 
          !trim_zone_selection && 
          my_trim.initialized) {
        fl_pop_clip();
      }
      
      if (first_page && (view_mode == Z_MYTRIM) && trim_zone_selection) {

        if (my_trim.initialized) {

          s32 w, h;

          const trim_struct * the_trim = get_trimming_for_page(page);
          
          fl_overlay_rect(
            selx = Xs + (zoom * the_trim->X),
            sely = Ys + (zoom * the_trim->Y),
            w    = (zoom * the_trim->W),
            h    = (zoom * the_trim->H));

          selx2 = selx + w;
          sely2 = sely + h;

          //fl_message("selx: %d, sely: %d, w: %d, h: %d", selx, sely, w, h);
        }
      }

      // For each displayed page, we keep those parameters
      // to permit the localization of the page on screen when
      // a selection is made to retrieve the text underneath
      if (page_pos_count < PAGES_ON_SCREEN_MAX) {

        pp->page    = page;
        pp->X0      = Xs;
        pp->Y0      = Ys; 
        pp->W0      = Ws;
        pp->H0      = Hs;              
        pp->X       = X;
        pp->Y       = Y;
        pp->W       = W;
        pp->H       = H;
        pp->zoom    = zoom;
        pp->ratio_x = ratio_x;
        pp->ratio_y = ratio_y;

        // if (page_pos_count == 0) 
        //  printf("Copy X0:%d Y0:%d X:%d Y:%d W:%d H:%d Zoom: %6.3f page: %d\n", 
        //      pp->X0, pp->Y0, pp->X, pp->Y, pp->W, pp->H, pp->zoom, pp->page + 1);

        page_pos_count++;
        pp++;
      }

      // Restore page coordinates for next loop.
      X = Xs;
      Y = Ys;
      W = Ws;
      H = Hs;

      X += zoomedmarginhalf + (pagew(page) * zoom);
      page++; column++;

      first_page = false;
    }

    // Prepare for next line of pages

    Y += (line_height * zoom) + zoomedmarginhalf;

    first_page_in_line += limit;
    current_screen_vpos = Y - screen_y;
    
    first_line = false;
  }

  file->last_visible = page;

  fl_pop_clip();
}

// Compute the maximum yoff value, taking care of the number of 
// columns displayed, title pages and the screen size. Start at the end of the
// document, getting up to the point where the line of pages will be
// out of reach on screen.
//
// Pages = 8, columns = 3, title pages = 1
// last = 7 - (7 % 3) - (columns - title_pages) = 7 - 1 - 2 = 4 + 3 = 7

// Pages = 9, columns = 3, title pages = 2
// last = 8 - (8 % 3) - (columns - title_pages) = 8 - 2 - 1 = 5 + 3

// Pages = 13, columns = 4, title pages = 1
// last = 12 - (12 % 4) - (columns - title_pages) = 12 - 0 - 3 = 9 
float PDFView::maxyoff() const 
{
  float zoom, f;
  u32   line_width, line_height, h;

  s32 pages = file->pages;
  s32 last  = pages - 1;
  last     -= (last % columns);
  if ((title_pages > 0) && (title_pages < columns)) {
    last -= (columns - title_pages);
    if (last < (pages - (s32)columns)) {
      last += columns;
    }
  }

  if (last < 0) last = 0;

  if (!file->cache[last].ready)
    f = last + 0.5f;
  else {
    s32 H = screen_height;

    while (true) {
      zoom = line_zoom_factor(last, line_width, line_height);
      H   -= (h = zoom * (line_height + MARGINHALF));

      if (H <= 0) {
        H += (MARGINHALF * zoom);
        f = last + (float)(-H) / (zoom * line_height);
        break;
      } 

      last -= columns;
      if (last < 0) {
        f = 0.0f;
        break;
      }
    }
  }
  return f;
}

// Advance the yoff position by an offset, taking into account the number
// of columns and the title_pages count. Yoff is adjusted to correspond to
// the first page offset to be disblayed on a line of pages. For examples:
// 
//    columns   title_pages   offset    yoff    new_yoff
//
//       3           1          1         0        1
//       3           2          1         0        2
//       3           2          2         0        5
//       3           1         -1         3        1

void PDFView::adjust_yoff(float offset) 
{
  float new_yoff = yoff;

  if (offset != 0.0f) {
  
    new_yoff += offset;
    s32 diff = floorf(new_yoff) - floorf(yoff); // Number of absolute pages of difference

    if ((diff != 0) && (new_yoff >= 0.0f)) {
      // The displacement is to another line of pages
      new_yoff += (float)diff * (columns - 1);
      if ((title_pages > 0) && (title_pages < columns)) {
        // There is title pages to take care of
        if (new_yoff < 1.0f) {
          new_yoff += (columns - title_pages);
        }
        else if (yoff < 1.0f) {
          new_yoff -= (columns - title_pages);
        }
      }
    }
    
    //printf("diff: %d, yoff: %f new_yoff: %f\n", diff, yoff, new_yoff);

    //  float y = floorf(yoff); 
    //  yoff += offset;
    //  float diff = floorf(yoff) - y; // diff is the number of "line of pages" to advance
    //  yoff = yoff + (diff * columns) - diff; 
  }

  if (new_yoff < 0.0f)
    new_yoff = 0.0f;
  else {
    float y = maxyoff();

    if (new_yoff > y) new_yoff = y;
  }

  yoff = new_yoff;
}

void PDFView::adjust_floor_yoff(float offset) 
{
  float new_yoff = floorf(yoff);

  // From here, almost same code as adjust_yoff...'

  if (offset != 0.0f) {
  
    new_yoff += offset;
    s32 diff = new_yoff - floorf(yoff);

    if ((diff != 0) && (new_yoff >= 0.0f)) {
      if ((title_pages > 0) && (title_pages < columns) && (yoff < 1.0f))
        new_yoff += ((float)diff * (columns - 1)) - (columns - title_pages);
      else
        new_yoff += (float)diff * (columns - 1);
    }

    // float y = floorf(yoff); 
    // yoff = y + offset;
    // float diff = floorf(yoff) - y;
    // yoff = yoff + (diff * columns) - diff; 
  }

  if (new_yoff < 0.0f)
    new_yoff = 0.0f;
  else {
    float max = maxyoff();

    if (new_yoff > max) new_yoff = max;
  }

  yoff = new_yoff;
}

void PDFView::end_of_selection() 
{
  s32 X, Y, W, H;
  if (selx < selx2) {
    X = selx;
    W = selx2 - X;
  } 
  else {
    X = selx2;
    W = selx - X;
  }

  if (sely < sely2) {
    Y = sely;
    H = sely2 - Y;
  } 
  else {
    Y = sely2;
    H = sely - Y;
  }

  // Search for the page caracteristics saved before with
  // the draw method.
  u32 idx = 0;
  page_pos_struct *pp = page_pos_on_screen;

  while (idx < page_pos_count) {
    if ((X >= pp->X0)       &&
      (Y >= pp->Y0)         &&
      (X < (pp->X + pp->W)) &&
      (Y < (pp->Y + pp->H)))
      break;
    idx++;
    pp++;
  }
  if (idx >= page_pos_count)
    return; // Not found

  if (text_selection) {
    // Adjust the selection rectangle to be inside the real page data
    if ((X + W) > (pp->X + pp->W))
      W = pp->X + pp->W - X;
    if ((Y + H) > (pp->Y + pp->H))
      H = pp->Y + pp->H - Y;
    if (((X + W) < pp->X) ||
      ((Y + H) < pp->Y))
      return; // Out of the printed area
    if (X < pp->X) {
      W -= (pp->X - X);
      X = pp->X;
    }
    if (Y < pp->Y) {
      H -= (pp->Y - Y);
      Y = pp->Y;
    }
  }
  else if (trim_zone_selection) {
    // Adjust the selection rectangle to be inside the extended page data
    if ((X + W) > (pp->X0 + pp->W0))
      W = pp->X0 + pp->W0 - X;
    if ((Y + H) > (pp->Y0 + pp->H0))
      H = pp->Y0 + pp->H0 - Y;
    if (((X + W) < pp->X0) ||
      ((Y + H) < pp->Y0))
      return; // Out of the printed area
    if (X < pp->X0) {
      W -= (pp->X0 - X);
      X = pp->X0;
    }
    if (Y < pp->Y0) {
      H -= (pp->Y0 - Y);
      Y = pp->Y0;
    }
  }

  // Convert to page coords
  if (view_mode == Z_TRIM   || 
      view_mode == Z_PGTRIM || 
     (view_mode == Z_MYTRIM && !trim_zone_selection)) {
    X = X - pp->X0 + (pp->zoom * file->cache[pp->page].left);
    Y = Y - pp->Y0 + (pp->zoom * file->cache[pp->page].top ) ;

    if (hasmargins(pp->page)) {
      X -= (pp->X - pp->X0);
      Y -= (pp->Y - pp->Y0);
    }
  }
  else {
    X -= pp->X0;
    Y -= pp->Y0;
  }

  // Convert to original page resolution
  X /= (pp->zoom * pp->ratio_x);
  Y /= (pp->zoom * pp->ratio_y);
  W /= (pp->zoom * pp->ratio_x);
  H /= (pp->zoom * pp->ratio_y);

  if (text_selection) {
    TextOutputDev * const dev = new TextOutputDev(NULL, true, 0, false, false);
    file->pdf->displayPage(dev, pp->page + 1, 144, 144, 0, true, false, false);
    GooString *str = dev->getText(X, Y, X + W, Y + H);
    const char * const cstr = str->getCString();

    // Put it to clipboard
    Fl::copy(cstr, strlen(cstr), 1);

    delete str;
    delete dev;
  }
  else if (trim_zone_selection) {
    if (single_page_trim) {
      debug("Adding a single page trim for page %d\n", pp->page);
      add_single_page_trim(pp->page, X, Y, W, H);	
    }
    else {
      if (my_trim.similar) {
        my_trim.odd.X = my_trim.even.X = X;
        my_trim.odd.Y = my_trim.even.Y = Y;
        my_trim.odd.W = my_trim.even.W = W;
        my_trim.odd.H = my_trim.even.H = H;
      }
      else if (pp->page & 1) {
        my_trim.even.X = X;
        my_trim.even.Y = Y;
        my_trim.even.W = W;
        my_trim.even.H = H;
      }
      else {
        my_trim.odd.X = X;
        my_trim.odd.Y = Y;
        my_trim.odd.W = W;
        my_trim.odd.H = H;
      }
      //fl_message("X: %d, Y: %d, W: %d, H: %d", X, Y, W, H);
      my_trim.initialized = true;
    }
    debug("Selection: X: %d, Y: %d, W: %d, H: %d\n", X, Y, W, H);
  }
}

void PDFView::page_up() 
{
  if (floorf(yoff) == yoff)
    adjust_floor_yoff(-1.0f);
  else
    adjust_floor_yoff(0.0f);
  redraw();
}

void PDFView::page_down() 
{
  adjust_floor_yoff(1.0f);
  redraw();
}

void PDFView::page_top() 
{
  yoff = 0.0f;
  redraw();
}

void PDFView::page_bottom() 
{
  yoff = maxyoff();
  redraw();
}

trim_zone_loc_enum PDFView::get_trim_zone_loc(s32 x, s32 y) const
{
	if (x > (selx - 5) && x < (selx + 5)) {
		if (y > (sely - 5) && y < (sely + 5)) {
			return TZL_NW;
		}
		else if (y > (sely2 - 5) && y < (sely2 + 5)) {
			return TZL_SW;
		}
		else {
			return TZL_W;
		}
	}
	else if (x > (selx2 - 5) && x < (selx2 + 5)) {
		if (y > (sely - 5) && y < (sely + 5)) {
			return TZL_NE;
		}
		else if (y > (sely2 - 5) && y < (sely2 + 5)) {
			return TZL_SE;
		}
		else {
			return TZL_E;
		}
	}
	else if (y > (sely - 5) && y < (sely + 5)) {
		return TZL_N;
	}
	else if (y > (sely2 - 5) && y < (sely2 + 5)) {
		return TZL_S;
	}
	else {
		return TZL_NONE;
	}
}

int PDFView::handle(int e) 
{
  static trim_zone_loc_enum trim_zone_loc = TZL_NONE;
  
  if (!file->cache)
    return Fl_Widget::handle(e);

  float zoom;
  u32 line_width, line_height;
  zoom = line_zoom_factor(yoff, line_width, line_height);

  static int lasty, lastx;
  const float move = 0.05f;

  switch (e) {
    case FL_RELEASE:
      // Was this a dragging text selection?
      if ((text_selection || trim_zone_selection) && Fl::event_button() == FL_LEFT_MOUSE &&
        selx && sely && selx2 && sely2 && selx != selx2 &&
        sely != sely2) {

		if (trim_zone_selection) {
			// fl_overlay_clear();
			s32 tmp;
			if (selx > selx2) {
			  tmp   = selx;
			  selx  = selx2;
			  selx2 = tmp;
			}
			if (sely > sely2) {
			  tmp   = sely;
			  sely  = sely2;
			  sely2 = tmp;
			}
		}
        end_of_selection();
      }
      break;
      
    case FL_PUSH:
      take_focus();
      lasty = Fl::event_y();
      lastx = Fl::event_x();

      reset_selection();
      if (text_selection && Fl::event_button() == FL_LEFT_MOUSE) {
        selx = lastx;
        sely = lasty;
        redraw();
      }
      else {
        if (trim_zone_selection && Fl::event_button() == FL_LEFT_MOUSE) {
          trim_zone_loc_enum trim_zone_loc = get_trim_zone_loc(Fl::event_x(), Fl::event_y());
          if (trim_zone_loc == TZL_NONE) {
            selx = lastx;
            sely = lasty;   	
          }
        }
      }


      // Fall-through
    case FL_FOCUS:
    case FL_ENTER:
      return 1;
      break;
    
    case FL_DRAG:
      {
        const int my = Fl::event_y();
        const int mx = Fl::event_x();
        const int movedy = my - lasty;
        const int movedx = mx - lastx;

        if (text_selection && Fl::event_button() == FL_LEFT_MOUSE) {
          selx2 = mx;
          sely2 = my;
          redraw();
          return 1;
        }
        else if (trim_zone_selection && Fl::event_button() == FL_LEFT_MOUSE) {
            switch (trim_zone_loc) {
              case TZL_NE: 	selx2 = mx; sely  = my; break;
              case TZL_NW:  selx  = mx; sely  = my; break;
              case TZL_SE:  selx2 = mx; sely2 = my; break;
              case TZL_SW:  selx  = mx; sely2 = my; break;
              case TZL_E:  	selx2 = mx;             break;
              case TZL_W:   selx  = mx;             break;
              case TZL_N:   sely  = my;             break;
              case TZL_S:   sely2 = my;             break;

              default:      selx2 = mx; sely2 = my; break;
            }	
            window()->make_current();	
          	fl_overlay_rect(selx, sely, selx2 - selx, sely2 - sely);
          	
            return 1;
        }

        fl_cursor(FL_CURSOR_MOVE);

        // I don't understand this...
        if (file->maxh)
          adjust_yoff(-((movedy / zoom) / file->maxh));
        else
          adjust_yoff(-((movedy / (float) screen_height) / zoom));

        if (view_mode == Z_CUSTOM) {
          xoff += ((float) movedx / zoom) / line_width;
          float maxchg = (((((float)screen_width / zoom) + line_width) / 2) / line_width) - 0.1f;
          if (xoff < -maxchg)
            xoff = -maxchg;
          if (xoff > maxchg)
            xoff = maxchg;
        }

        lasty = my;
        lastx = mx;
      }
      redraw();
      break;
      
    case FL_MOUSEWHEEL:
      if (Fl::event_ctrl())
        if (Fl::event_dy() > 0)
          cb_Zoomout(NULL, NULL);
        else
          cb_Zoomin(NULL, NULL);
      else
        adjust_yoff(move * Fl::event_dy());

      reset_selection();
      redraw();
      break;
      
    case FL_KEYDOWN:
    case FL_SHORTCUT:
      reset_selection();
      switch (Fl::event_key()) {
        case 'k':
          if (Fl::event_ctrl())
            yoff = 0;
          else {
            const u32 page = yoff;
            s32 sh;
            s32 shp = sh = pxrel(page);
            if (page >= columns)
              shp = pxrel(page - columns);
            if (screen_height >= sh) {
              /* scroll up like Page_Up */
              if (floorf(yoff) >= yoff)
                adjust_floor_yoff(-1.0f);
              else
                adjust_floor_yoff(0.0f);
            } 
            else {
              /* scroll up a bit less than one screen height */
              float d = (screen_height - 2 * MARGIN) / (float) sh;
              if (((u32) yoff) != ((u32) (yoff - d))) {
                /* scrolling over page border */
                d -= (yoff - floorf(yoff));
                yoff = floorf(yoff);
                /* ratio of prev page can be different */
                d = d * (float) sh / (float) shp;
              }
              adjust_yoff(-d);
            }
          }
          redraw();
          break;
          
        case 'j':
          if (Fl::event_ctrl())
            yoff = maxyoff();
          else {
            const s32 page = yoff;
            s32 sh;
            s32 shn = sh = pxrel(page);
            if (page + columns <= (u32)(file->pages - 1))
              shn = pxrel(page + columns);
            if (screen_height >= sh)
              /* scroll down like Page_Down */
              adjust_floor_yoff(1.0f);
            else {
              /* scroll down a bit less than one screen height */
              float d = (screen_height - 2 * MARGIN) / (float) sh;
              if (((u32) yoff) != ((u32) (yoff + d))) {
                /* scrolling over page border */
                d -= (ceilf(yoff) - yoff);
                yoff = floorf(yoff) + columns;
                /* ratio of next page can be different */
                d = d * (float) sh / (float) shn;
              }
              adjust_yoff(d);
            }
          }
          redraw();
          break;
          
        case FL_Up:        adjust_yoff(-move); redraw(); break;
          
        case FL_Down:      adjust_yoff( move); redraw(); break;
          
        case FL_Page_Up:   page_up();                    break;
          
        case FL_Page_Down: page_down();                  break;
          
        case FL_Home:
          if (Fl::event_ctrl())
            yoff = 0;
          else
            yoff = floorf(yoff);
          redraw();
          break;
          
        case FL_End:
          if (Fl::event_ctrl())
            yoff = maxyoff();
          else {
            u32 page = yoff;
            s32 sh = line_height * zoom;

            if (file->cache[page].ready) {
              const s32 hidden = sh - screen_height;
              float tmp = floorf(yoff) + hidden / (float) sh;
              if (tmp > yoff)
                yoff = tmp;
              adjust_yoff(0);
            }
            else
              yoff = ceilf(yoff) - 0.4f;
          }
          redraw();
          break;
          
        case FL_F + 8:
          cb_hide_show_buttons(NULL, NULL); // Hide toolbar
          break;
          
        default:
          return 0;
          
      } // switch (Fl::event_key())

      if (file->cache)
        update_visible(false);

      return 1;
      break;
      
    case FL_MOVE:
      // Set the cursor appropriately
      if (!file->maxw)
        fl_cursor(FL_CURSOR_WAIT);
      else if (text_selection)
        fl_cursor(FL_CURSOR_CROSS);
      else if (trim_zone_selection) {
        trim_zone_loc = get_trim_zone_loc(Fl::event_x(), Fl::event_y());
        switch (trim_zone_loc) {
          case TZL_NW: fl_cursor(FL_CURSOR_NW);    break;
          case TZL_SE: fl_cursor(FL_CURSOR_SE);    break;
          case TZL_NE: fl_cursor(FL_CURSOR_NE);    break;
          case TZL_SW: fl_cursor(FL_CURSOR_SW);    break;
          case TZL_E:
          case TZL_W:  fl_cursor(FL_CURSOR_WE);    break;
          case TZL_N:
          case TZL_S:  fl_cursor(FL_CURSOR_NS);    break;

          default:     fl_cursor(FL_CURSOR_CROSS); break;
        }	
      }
      else
        fl_cursor(FL_CURSOR_DEFAULT);
      break;
  }

  return Fl_Widget::handle(e);
}

u8 PDFView::iscached(const u32 page) const 
{
  u32 i;
  for (i = 0; i < CACHE_MAX; i++) {
    if (cachedpage[i] == page)
      return i;
  }

  return UCHAR_MAX;
}

void PDFView::docache(const u32 page) 
{
  // Insert it to cache. Pick the slot at random.
  const struct cachedpage * const cur = &file->cache[page];
  u32 i;

  if (cur->uncompressed > cachedsize) {
    cachedsize = cur->uncompressed;

    for (i = 0; i < CACHE_MAX; i++)
      cache[i] = (u8 *) realloc(cache[i], cachedsize);
  }

  // Be safe
  if (!cur->ready)
    return;

  const u32 dst = rand() % CACHE_MAX;

  lzo_uint dstsize = cachedsize;
  const int ret = lzo1x_decompress(cur->data,
          cur->size,
          cache[dst],
          &dstsize,
          NULL);
  if (ret != LZO_E_OK || dstsize != cur->uncompressed)
    die(_("Error decompressing\n"));

  cachedpage[dst] = page;

  // Create the Pixmap
  if (pix[dst] != None)
    XFreePixmap(fl_display, pix[dst]);

  pix[dst] = XCreatePixmap(fl_display, fl_window, cur->w, cur->h, 24);
  if (pix[dst] == None)
    return;

  fl_push_no_clip();

  XImage *xi = XCreateImage(fl_display, fl_visual->visual, 24, ZPixmap, 0,
          (char *) cache[dst], cur->w, cur->h,
          32, 0);
  if (xi == NULL) die("xi null\n");

  XPutImage(fl_display, pix[dst], fl_gc, xi, 0, 0, 0, 0, cur->w, cur->h);

  fl_pop_clip();

  xi->data = NULL;
  XDestroyImage(xi);
}

void PDFView::content(
  const u32 page, 
  const s32 X, 
  const s32 Y,
  const u32 W, 
  const u32 H) 
{
  // Do a gpu-accelerated bilinear blit
  u8 c = iscached(page);
  if (c == UCHAR_MAX)
    docache(page);
  c = iscached(page);

  const struct cachedpage * const cur = &file->cache[page];

  XRenderPictureAttributes srcattr;
  memset(&srcattr, 0, sizeof(XRenderPictureAttributes));
  XRenderPictFormat *fmt = XRenderFindStandardFormat(fl_display, PictStandardRGB24);

  // This corresponds to GL_CLAMP_TO_EDGE.
  srcattr.repeat = RepeatPad;

  Picture src = XRenderCreatePicture(fl_display, pix[c], fmt, CPRepeat, &srcattr);
  Picture dst = XRenderCreatePicture(fl_display, fl_window, fmt, 0, &srcattr);

  const Fl_Region clipr = fl_clip_region();
  XRenderSetPictureClipRegion(fl_display, dst, clipr);

  XRenderSetPictureFilter(fl_display, src, "bilinear", NULL, 0);
  XTransform xf;
  memset(&xf, 0, sizeof(XTransform));
  xf.matrix[0][0] = (65536 * cur->w) / W;
  xf.matrix[1][1] = (65536 * cur->h) / H;
  xf.matrix[2][2] = 65536;
  XRenderSetPictureTransform(fl_display, src, &xf);

  XRenderComposite(fl_display, PictOpSrc, src, None, dst, 0, 0, 0, 0, X, Y, W, H);
//  XCopyArea(fl_display, pix[c], fl_window, fl_gc, 0, 0, W, H, X, Y);
//  fl_draw_image(cache[c], X, Y, W, H, 4, file->cache[page].w * 4);

  if (text_selection && selx2 && sely2 && selx != selx2 && sely != sely2) {
    // Draw a selection rectangle over this area
    const XRenderColor col = {0, 0, 16384, 16384};

    u16 x, y, w, h;
    if (selx < selx2) {
      x = selx;
      w = selx2 - x;
    } 
    else {
      x = selx2;
      w = selx - x;
    }

    if (sely < sely2) {
      y = sely;
      h = sely2 - y;
    } 
    else {
      y = sely2;
      h = sely - y;
    }

    XRenderFillRectangle(fl_display, PictOpOver, dst, &col, x, y, w, h);
  }

  XRenderFreePicture(fl_display, src);
  XRenderFreePicture(fl_display, dst);
}
