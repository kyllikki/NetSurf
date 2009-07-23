/*
 * Copyright 2008-9 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AMIGA_GUI_H
#define AMIGA_GUI_H
#include <graphics/rastport.h>
#include "amiga/object.h"
#include <intuition/classusr.h>
#include "desktop/browser.h"
#include <dos/dos.h>
#include "desktop/gui.h"
#include "amiga/plotters.h"

enum
{
    GID_MAIN=0,
	GID_TABLAYOUT,
	GID_BROWSER,
	GID_STATUS,
	GID_URL,
	GID_STOP,
	GID_RELOAD,
	GID_HOME,
	GID_BACK,
	GID_FORWARD,
	GID_THROBBER,
	GID_CLOSETAB,
	GID_TABS,
	GID_USER,
	GID_PASS,
	GID_LOGIN,
	GID_CANCEL,
	GID_TREEBROWSER,
	GID_OPEN,
	GID_LEFT,
	GID_UP,
	GID_DOWN,
	GID_NEWF,
	GID_NEWB,
	GID_DEL,
	GID_NEXT,
	GID_PREV,
	GID_SEARCHSTRING,
    GID_LAST
};

enum
{
    OID_MAIN=0,
	OID_VSCROLL,
	OID_HSCROLL,
	OID_MENU,
    OID_LAST
};

#define AMI_GUI_POINTER_BLANK GUI_POINTER_PROGRESS+1
#define AMI_GUI_POINTER_DRAG  GUI_POINTER_PROGRESS+2
#define AMI_LASTPOINTER AMI_GUI_POINTER_DRAG

struct find_window;
struct history_window;

struct gui_window_2 {
	struct Window *win;
	Object *objects[OID_LAST];
	struct Gadget *gadgets[GID_LAST];
	struct nsObject *node;
	struct browser_window *bw;
	bool redraw_required;
	int throbber_frame;
	struct List tab_list;
	ULONG tabs;
	ULONG next_tab;
	struct Hook scrollerhook;
	struct Hook popuphook;
	struct form_control *control;
	browser_mouse_state mouse_state;
	browser_mouse_state key_state;
	ULONG throbber_update_count;
	struct find_window *searchwin;
	ULONG oldh;
	ULONG oldv;
	bool redraw_scroll;
	bool new_content;
	char *svbuffer;
};

struct gui_window
{
	struct gui_window_2 *shared;
	int tab;
	struct Node *tab_node;
	int c_x;
	int c_y;
	int c_h;
	int scrollx;
	int scrolly;
	struct history_window *hw;
	struct List dllist;
};

void ami_get_msg(void);
void ami_update_pointer(struct Window *win, gui_pointer_shape shape);
void ami_close_all_tabs(struct gui_window_2 *gwin);
void ami_quit_netsurf(void);
void ami_get_theme_filename(char *filename,char *themestring);
void ami_clearclipreg(struct RastPort *rp);
void ami_do_redraw(struct gui_window_2 *g);
STRPTR ami_locale_langs(void);

struct TextFont *origrpfont;
struct MinList *window_list;
struct Screen *scrn;
STRPTR nsscreentitle;
struct FileRequester *filereq;
struct FileRequester *savereq;
struct MsgPort *sport;
bool win_destroyed;
struct browser_window *curbw;
struct gui_globals browserglob;
#endif
