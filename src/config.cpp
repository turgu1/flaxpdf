/*
Copyright (C) 2016 Guy Turcotte

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

//#define TEST 1

#ifdef TEST
  #include "config.h"
#else
  #include "main.h"
#endif

#ifdef TEST
  #define fl_alert printf
  #define fl_message printf
  #define xmalloc malloc
  #define _(a) a
#endif

std::string CONFIG_VERSION("1.1");
std::string version("ERROR");

recent_file_struct *recent_files = NULL;

void save_to_config(
    char * filename,
    int columns,
    int title_page_count,
    float xoff,
    float yoff,
    float zoom,
    int view_mode,
    int x,
    int y,
    int w,
    int h,
    bool full,
    my_trim_struct &my_trim) {

  recent_file_struct *prev = NULL;
  recent_file_struct *rf   = recent_files;

  while ((rf != NULL) && (rf->filename.compare(filename) != 0)) {

    prev = rf;
    rf   = rf->next;
  }

  if (rf) {
    rf->columns          = columns;
    rf->title_page_count = title_page_count;
    rf->xoff             = xoff;
    rf->yoff             = yoff;
    rf->zoom             = zoom;
    rf->view_mode        = (view_mode_enum) view_mode;
    rf->x                = x;
    rf->y                = y;
    rf->width            = w;
    rf->height           = h;
    rf->fullscreen       = full;
    rf->my_trim          = my_trim;

    // prev is NULL if it's already the first entry in the list
    if (prev) {
      prev->next   = rf->next;
      rf->next     = recent_files;
      recent_files = rf;
    }
  }
  else {
    rf = new recent_file_struct;

    rf->filename         = filename;
    rf->columns          = columns;
    rf->title_page_count = title_page_count;
    rf->xoff             = xoff;
    rf->yoff             = yoff;
    rf->zoom             = zoom;
    rf->view_mode        = (view_mode_enum) view_mode;
    rf->x                = x;
    rf->y                = y;
    rf->width            = w;
    rf->height           = h;
    rf->fullscreen       = full;
    rf->my_trim          = my_trim;

    rf->next       = recent_files;
    recent_files   = rf;
  }
}

bool file_exists (const char * name) {
  struct stat buffer;   
  return (stat (name, &buffer) == 0); 
}

void load_config() {

  Config cfg;

  static char config_filename[301];
  char * homedir;

  if ((homedir = getenv("HOME")) == NULL) {
    homedir = getpwuid(getuid())->pw_dir;
  }

  strncpy(config_filename, homedir, 300);
  strncat(config_filename, "/.updf", 300 - strlen(config_filename) - 1);

  try {
    cfg.readFile(config_filename);

    Setting &root = cfg.getRoot();

    if (root.exists("version")){

      version = root["version"].c_str();

      if (version.compare(CONFIG_VERSION) != 0) {
        fl_alert(_("Error: Config file version is expected to be %s."), CONFIG_VERSION.c_str());
        return;
      }
    }
    else {
      fl_alert(_("Error: Config file structure error."));
      return;
    }

    if (!root.exists("recent_files"))
      root.add("recent_files", Setting::TypeList);

    Setting &rf_setting = root["recent_files"];
    recent_file_struct *prev = NULL;

    for (int i = 0; i < rf_setting.getLength(); i++) {

      Setting &s = rf_setting[i];
      std::string filename = s["filename"];
      
      if (file_exists(filename.c_str())) {
        recent_file_struct *rf = new recent_file_struct;

        rf->title_page_count = 0;

        u32 mode;

        if (!(s.lookupValue("filename"        , rf->filename        ) &&
              s.lookupValue("columns"         , rf->columns         ) &&
              s.lookupValue("title_page_count", rf->title_page_count) &&
              s.lookupValue("xoff"            , rf->xoff            ) &&
              s.lookupValue("yoff"            , rf->yoff            ) &&
              s.lookupValue("zoom"            , rf->zoom            ) &&
              s.lookupValue("view_mode"       , mode                ) &&
              s.lookupValue("x"               , rf->x               ) &&
              s.lookupValue("y"               , rf->y               ) &&
              s.lookupValue("width"           , rf->width           ) &&
              s.lookupValue("height"          , rf->height          ) &&
              s.lookupValue("fullscreen"      , rf->fullscreen      ))) {
          fl_alert(_("Configuration file content error: %s."), config_filename);
          return;
        }
        
        rf->view_mode = (view_mode_enum) mode;

        if (s.exists("my_trim")) {
  
          Setting &mt = s.lookup("my_trim");

          if (!(mt.lookupValue("initialized", rf->my_trim.initialized) &&
                mt.lookupValue("similar"    , rf->my_trim.similar    ))) {
            fl_alert(_("Configuration file content error: %s."), config_filename);
            return;
          }

          if (mt.exists("odd") && mt.exists("even")) {
            Setting &mto = mt.lookup("odd");

            if (!(mto.lookupValue("x", rf->my_trim.odd.X) &&
                  mto.lookupValue("y", rf->my_trim.odd.Y) &&
                  mto.lookupValue("w", rf->my_trim.odd.W) &&
                  mto.lookupValue("h", rf->my_trim.odd.H))) {

              fl_alert(_("Configuration file content error: %s."), config_filename);
              return;
            }

            Setting &mte = mt.lookup("even");

            if (!(mte.lookupValue("x", rf->my_trim.even.X) &&
                  mte.lookupValue("y", rf->my_trim.even.Y) &&
                  mte.lookupValue("w", rf->my_trim.even.W) &&
                  mte.lookupValue("h", rf->my_trim.even.H))) {

              fl_alert(_("Configuration file content error: %s."), config_filename);
              return;
            }
          }
          else {
            rf->my_trim.initialized = false;
          }

        }
        else {
          rf->my_trim.initialized = false;
          rf->my_trim.similar = true;
        }

        if (prev == NULL) {
          prev = rf;
          recent_files = rf;
        }
        else {
          prev->next = rf;
          prev = rf;
        }
        rf->next = NULL;
      }
    }
  }
  catch (const FileIOException &e) {
  }
  catch (const ParseException &e) {
    fl_alert(_("Parse Exception (config file %s error): %s"), config_filename, e.getError());
  }
  catch (const ConfigException &e) {
    fl_alert(_("Load Config Exception: %s"), e.what());
  }
}

void save_config() {

  Config cfg;

  char   config_filename[301];
  char * homedir;

  if ((homedir = getenv("HOME")) == NULL) {
    homedir = getpwuid(getuid())->pw_dir;
  }

  strncpy(config_filename, homedir, 300);
  strncat(config_filename, "/.updf", 300 - strlen(config_filename) - 1);

  try {
    Setting &root = cfg.getRoot();
    if (root.exists("recent_files"))
      root.remove("recent_files");

    root.add("version", Setting::TypeString) = CONFIG_VERSION;

    Setting &rf_setting = root.add("recent_files", Setting::TypeList);

    recent_file_struct *rf = recent_files;

    int cnt = 0;
    while ((rf != NULL) && (cnt < 10)) {
      Setting &gr = rf_setting.add(Setting::TypeGroup);

      gr.add("filename",         Setting::TypeString ) = rf->filename;
      gr.add("columns",          Setting::TypeInt    ) = rf->columns;
      gr.add("title_page_count", Setting::TypeInt    ) = rf->title_page_count;
      gr.add("xoff",             Setting::TypeFloat  ) = rf->xoff;
      gr.add("yoff",             Setting::TypeFloat  ) = rf->yoff;
      gr.add("zoom",             Setting::TypeFloat  ) = rf->zoom;
      gr.add("view_mode",        Setting::TypeInt    ) = (int)rf->view_mode;
      gr.add("x",                Setting::TypeInt    ) = rf->x;
      gr.add("y",                Setting::TypeInt    ) = rf->y;
      gr.add("width",            Setting::TypeInt    ) = rf->width;
      gr.add("height",           Setting::TypeInt    ) = rf->height;
      gr.add("fullscreen",       Setting::TypeBoolean) = rf->fullscreen;

      Setting &mt = gr.add("my_trim", Setting::TypeGroup);

      mt.add("initialized", Setting::TypeBoolean) = rf->my_trim.initialized;
      mt.add("similar", Setting::TypeBoolean) = rf->my_trim.similar;

      if (rf->my_trim.initialized) {
        Setting &mto = mt.add("odd", Setting::TypeGroup);

        mto.add("x", Setting::TypeInt) = rf->my_trim.odd.X;
        mto.add("y", Setting::TypeInt) = rf->my_trim.odd.Y;
        mto.add("w", Setting::TypeInt) = rf->my_trim.odd.W;
        mto.add("h", Setting::TypeInt) = rf->my_trim.odd.H;

        Setting &mte = mt.add("even", Setting::TypeGroup);

        mte.add("x", Setting::TypeInt) = rf->my_trim.even.X;
        mte.add("y", Setting::TypeInt) = rf->my_trim.even.Y;
        mte.add("w", Setting::TypeInt) = rf->my_trim.even.W;
        mte.add("h", Setting::TypeInt) = rf->my_trim.even.H;
      }

      rf = rf->next;
      cnt += 1;
    }

    cfg.writeFile(config_filename);
  }
  catch (const FileIOException &e) {
    fl_alert(_("File IO Error: Unable to create or write config file %s."), config_filename);
  }
  catch (const ConfigException &e) {
    fl_alert(_("Save Config Exception: %s."), e.what());
  }
}

#ifdef TEST

main()
{
  printf("Begin. Config version: %s\n", CONFIG_VERSION.c_str());

  load_config();

  printf("Config file:\nVersion: %s\n\n", version.c_str());

  printf("recent file descriptors:\n\n");
  
  recent_file_struct *rf = recent_files;
  while (rf != NULL) {
    printf("filename: %s, columns: %d, title_page_count: %d, xoff: %f, yoff: %f, zoom: %f, mode: %d\n",
      rf->filename.c_str(),
      rf->columns,
      rf->title_page_count,
      rf->xoff,
      rf->yoff,
      rf->zoom,
      rf->view_mode);
    rf = rf->next;
  }
  printf("End\n");
}

#endif

