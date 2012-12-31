/*
 * Copyright 2012 Ole Loots <ole@monochrom.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/browser_private.h"
#include "desktop/mouse.h"
#include "desktop/plot_style.h"
#include "desktop/plotters.h"
#include "desktop/tree.h"
#include "desktop/options.h"
#include "utils/utf8.h"
#include "atari/clipboard.h"
#include "atari/gui.h"
#include "atari/toolbar.h"
#include "atari/rootwin.h"

#include "atari/clipboard.h"
#include "atari/misc.h"
#include "atari/browser.h"
#include "atari/plot/plot.h"
#include "cflib.h"
#include "atari/res/netsurf.rsh"

#include "desktop/textarea.h"
#include "desktop/textinput.h"
#include "content/hlcache.h"


#define TB_BUTTON_WIDTH 32
#define THROBBER_WIDTH 32
#define THROBBER_MIN_INDEX 1
#define THROBBER_MAX_INDEX 12
#define THROBBER_INACTIVE_INDEX 13

#define TOOLBAR_URL_MARGIN_LEFT 	2
#define TOOLBAR_URL_MARGIN_RIGHT 	2
#define TOOLBAR_URL_MARGIN_TOP		2
#define TOOLBAR_URL_MARGIN_BOTTOM	2

enum e_toolbar_button_states {
        button_on = 0,
        button_off = 1
};
#define TOOLBAR_BUTTON_NUM_STATES   2

struct s_toolbar;

struct s_tb_button
{
	short rsc_id;
	void (*cb_click)(struct s_toolbar *tb);
	hlcache_handle *icon[TOOLBAR_BUTTON_NUM_STATES];
	struct s_toolbar *owner;
    enum e_toolbar_button_states state;
    short index;
    GRECT area;
};


extern char * option_homepage_url;
extern void * h_gem_rsrc;
extern struct gui_window * input_window;
extern long atari_plot_flags;
extern int atari_plot_vdi_handle;
extern EVMULT_OUT aes_event_out;

static OBJECT * aes_toolbar = NULL;
static OBJECT * throbber_form = NULL;
static char * toolbar_image_folder = (char *)"default";
static uint32_t toolbar_bg_color = 0xFFFFFF;
static hlcache_handle * toolbar_image;
static hlcache_handle * throbber_image;
static bool toolbar_image_ready = false;
static bool throbber_image_ready = false;
static bool init = false;

static plot_font_style_t font_style_url = {
    .family = PLOT_FONT_FAMILY_SANS_SERIF,
    .size = 14*FONT_SIZE_SCALE,
    .weight = 400,
    .flags = FONTF_NONE,
    .background = 0xffffff,
    .foreground = 0x0
 };


/* prototypes & order for button widgets: */


static struct s_tb_button tb_buttons[] =
{
	{
        TOOLBAR_BT_BACK,
        toolbar_back_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{
        TOOLBAR_BT_HOME,
        toolbar_home_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{
        TOOLBAR_BT_FORWARD,
        toolbar_forward_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{
        TOOLBAR_BT_STOP,
        toolbar_stop_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{
        TOOLBAR_BT_RELOAD,
        toolbar_reload_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{ 0, 0, {0,0}, 0, -1, 0, {0,0,0,0}}
};

struct s_toolbar_style {
	int font_height_pt;
	int height;
	int icon_width;
	int icon_height;
	int button_hmargin;
	int button_vmargin;
	/* RRGGBBAA: */
	uint32_t icon_bgcolor;
};

static struct s_toolbar_style toolbar_styles[] =
{
	/* small (18 px height) */
	{ 9, 18, 16, 16, 0, 0, 0 },
	/* medium (default - 26 px height) */
	{14, 26, 24, 24, 1, 4, 0 },
	/* large ( 49 px height ) */
	{18, 34, 64, 64, 2, 0, 0 },
	/* custom style: */
	{18, 34, 64, 64, 2, 0, 0 }
};

static const struct redraw_context toolbar_rdrw_ctx = {
				.interactive = true,
				.background_images = true,
				.plot = &atari_plotters
			};

static void tb_txt_request_redraw(void *data, int x, int y, int w, int h );
static nserror toolbar_icon_callback( hlcache_handle *handle,
		const hlcache_event *event, void *pw );

/**
*   Find a button for a specific resource ID
*/
static struct s_tb_button *find_button(struct s_toolbar *tb, int rsc_id)
{
	int i = 0;
	while (i < tb->btcnt) {
		if (tb->buttons[i].rsc_id == rsc_id) {
			return(&tb->buttons[i]);
		}
		i++;
	}
	return(NULL);
}

/**
*   Callback for textarea redraw
*/
static void tb_txt_request_redraw(void *data, int x, int y, int w, int h)
{

    GRECT area;
	struct s_toolbar * tb = (struct s_toolbar *)data;

	if (tb->attached == false) {
        return;
    }

	toolbar_get_grect(tb, TOOLBAR_URL_AREA, &area);
	area.g_x += x;
	area.g_y += y;
	area.g_w = w;
	area.g_h = h;
	//dbg_grect("tb_txt_request_redraw", &area);
	window_schedule_redraw_grect(tb->owner, &area);
    return;
}

static struct s_tb_button *button_init(struct s_toolbar *tb, OBJECT * tree, int index,
							struct s_tb_button * instance)
{
	*instance = tb_buttons[index];
	instance->owner = tb;

	instance->area.g_w = toolbar_styles[tb->style].icon_width + \
		( toolbar_styles[tb->style].button_vmargin * 2);

    return(instance);
}


void toolbar_init( void )
{
	int i=0, n;
	short vdicolor[3];
	uint32_t rgbcolor;

    aes_toolbar = get_tree(TOOLBAR);
    throbber_form = get_tree(THROBBER);
}


void toolbar_exit(void)
{
	if (toolbar_image)
		hlcache_handle_release(toolbar_image);
	if (throbber_image)
		hlcache_handle_release(throbber_image);
}


struct s_toolbar *toolbar_create(struct s_gui_win_root *owner)
{
	int i;

	LOG((""));

	struct s_toolbar *t = calloc(sizeof(struct s_toolbar), 1);

	assert(t);

	t->owner = owner;
	t->style = 1;

	/* create the root component: */
	t->area.g_h = toolbar_styles[t->style].height;

	/* count buttons and add them as components: */
	i = 0;
	while(tb_buttons[i].rsc_id > 0) {
		i++;
	}
	t->btcnt = i;
	t->buttons = malloc(t->btcnt * sizeof(struct s_tb_button));
	memset( t->buttons, 0, t->btcnt * sizeof(struct s_tb_button));
	for (i=0; i < t->btcnt; i++ ) {
		button_init(t, aes_toolbar, i, &t->buttons[i]);
	}

	/* create the url widget: */
	font_style_url.size =
		toolbar_styles[t->style].font_height_pt * FONT_SIZE_SCALE;

	int ta_height = toolbar_styles[t->style].height;
	ta_height -= (TOOLBAR_URL_MARGIN_TOP + TOOLBAR_URL_MARGIN_BOTTOM);
	t->url.textarea = textarea_create(300, ta_height, 0, &font_style_url,
                                   tb_txt_request_redraw, t);

	/* create the throbber widget: */
	t->throbber.area.g_h = toolbar_styles[t->style].height;
	t->throbber.area.g_w = toolbar_styles[t->style].icon_width + \
		(2*toolbar_styles[t->style].button_vmargin );
	t->throbber.index = THROBBER_INACTIVE_INDEX;
	t->throbber.max_index = THROBBER_MAX_INDEX;
	t->throbber.running = false;

	LOG(("created toolbar: %p, root: %p, textarea: %p, throbber: %p", t,
        owner, t->url.textarea, t->throbber));
	return( t );
}


void toolbar_destroy(struct s_toolbar *tb)
{
    free(tb->buttons);
	textarea_destroy( tb->url.textarea );
	free(tb);
}

static void toolbar_reflow(struct s_toolbar *tb)
{
    int i;

    // position toolbar areas:
    aes_toolbar->ob_x = tb->area.g_x;
    aes_toolbar->ob_y = tb->area.g_y;
    aes_toolbar->ob_width = tb->area.g_w;
    aes_toolbar->ob_height = tb->area.g_h;

    aes_toolbar[TOOLBAR_THROBBER_AREA].ob_x = tb->area.g_w
        - aes_toolbar[TOOLBAR_THROBBER_AREA].ob_width;

    aes_toolbar[TOOLBAR_URL_AREA].ob_width = tb->area.g_w
       - (aes_toolbar[TOOLBAR_NAVIGATION_AREA].ob_width
       + aes_toolbar[TOOLBAR_THROBBER_AREA].ob_width + 1);


    // position throbber image:
    throbber_form[tb->throbber.index].ob_x = tb->area.g_x +
        aes_toolbar[TOOLBAR_THROBBER_AREA].ob_x;

    throbber_form[tb->throbber.index].ob_x = tb->area.g_x
        + aes_toolbar[TOOLBAR_THROBBER_AREA].ob_x +
        ((aes_toolbar[TOOLBAR_THROBBER_AREA].ob_width
        - throbber_form[tb->throbber.index].ob_width) >> 1);

    throbber_form[tb->throbber.index].ob_y = tb->area.g_y +
        ((aes_toolbar[TOOLBAR_THROBBER_AREA].ob_height
        - throbber_form[tb->throbber.index].ob_height) >> 1);

    // set button states:
    for (i=0; i < tb->btcnt; i++ ) {
        if (tb->buttons[i].state == button_off) {
            aes_toolbar[tb->buttons[i].rsc_id].ob_state |= OS_DISABLED;
        }
        else if (tb->buttons[i].state == button_on) {
            aes_toolbar[tb->buttons[i].rsc_id].ob_state &= ~OS_DISABLED;
        }
	}
    tb->reflow = false;
    // TODO: iterate through all other toolbars and set reflow = true
}

void toolbar_redraw(struct s_toolbar *tb, GRECT *clip)
{
    GRECT area;

    if (tb->attached == false) {
        return;
    }

    if(tb->reflow == true)
        toolbar_reflow(tb);


    objc_draw_grect(aes_toolbar,0,8,clip);

    objc_draw_grect(&throbber_form[tb->throbber.index], 0, 1, clip);

    GRECT url_area;
    toolbar_get_grect(tb, TOOLBAR_URL_AREA, &area);
    url_area = area;
    if (rc_intersect(clip, &area)) {

        struct rect r = {
							.x0 = 0,
							.y0 = 0,
							.x1 = url_area.g_w,
							.y1 = url_area.g_h
						};

        r.x0 = area.g_x - url_area.g_x;
        r.x1 = r.x0 + area.g_w;
        plot_set_dimensions(url_area.g_x, url_area.g_y, url_area.g_w,
                            url_area.g_h);
        textarea_redraw(tb->url.textarea, 0, 0, &r, &toolbar_rdrw_ctx);
    }
}


void toolbar_update_buttons(struct s_toolbar *tb, struct browser_window *bw,
                       short button)
{
    LOG((""));

	struct s_tb_button * bt;
	bool enable = false;
	GRECT area;

	assert(bw != NULL);

	if (button == TOOLBAR_BT_BACK || button <= 0 ) {
	    bt = find_button(tb, TOOLBAR_BT_BACK);
		enable = browser_window_back_available(bw);
        if (enable) {
            bt->state = button_on;
        } else {
            bt->state = button_off;
        }
	}

	if (button == TOOLBAR_BT_HOME || button <= 0 ){

	}

	if( button == TOOLBAR_BT_FORWARD || button <= 0 ){
		bt = find_button(tb, TOOLBAR_BT_FORWARD);
		enable = browser_window_forward_available(bw);
        if (enable) {
            bt->state = button_on;
        } else {
            bt->state = button_off;
        }
	}

	if( button == TOOLBAR_BT_RELOAD || button <= 0 ){
	    bt = find_button(tb, TOOLBAR_BT_RELOAD);
		enable = browser_window_reload_available(bw);
        if (enable) {
            bt->state = button_on;
        } else {
            bt->state = button_off;
        }
	}

	if (button == TOOLBAR_BT_STOP || button <= 0) {
	    bt = find_button(tb, TOOLBAR_BT_STOP);
		enable = browser_window_stop_available(bw);
        if (enable) {
            bt->state = button_on;
        } else {
            bt->state = button_off;
        }
	}

    if (tb->attached) {
        if (button > 0) {
            toolbar_get_grect(tb, button, &area);
            window_schedule_redraw_grect(tb->owner, &area);
        }
        else {
            toolbar_get_grect(tb, TOOLBAR_NAVIGATION_AREA, &area);
            window_schedule_redraw_grect(tb->owner, &area);
        }
    }
}


void toolbar_set_dimensions(struct s_toolbar *tb, GRECT *area)
{


    if (area->g_w != tb->area.g_w || area->g_h != tb->area.g_h)  {
        tb->area = *area;
        /* reflow now, just for url input calucation: */
        toolbar_reflow(tb);
        /* this will request an textarea redraw: */
        textarea_set_dimensions(tb->url.textarea,
                                aes_toolbar[TOOLBAR_URL_AREA].ob_width,
                                20);
    }
    else {
        tb->area = *area;
    }
         /* reflow for next redraw: */
        /* TODO: that's only required because we do not reset others toolbars reflow
            state on reflow */
    tb->reflow = true;
}


void toolbar_set_url(struct s_toolbar *tb, const char * text)
{
    LOG((""));
    textarea_set_text(tb->url.textarea, text);

    if (tb->attached) {
        GRECT area;
        toolbar_get_grect(tb, TOOLBAR_URL_AREA, &area);
        window_schedule_redraw_grect(tb->owner, &area);
        struct gui_window * gw = window_get_active_gui_window(tb->owner);
        assert(gw != NULL);
        toolbar_update_buttons(tb, gw->browser->bw , 0);
	}
}

void toolbar_set_throbber_state(struct s_toolbar *tb, bool active)
{
    GRECT throbber_area;

    tb->throbber.running = active;
    if (active) {
        tb->throbber.index = THROBBER_MIN_INDEX;
    } else {
        tb->throbber.index = THROBBER_INACTIVE_INDEX;
    }

    tb->reflow = true;
    toolbar_get_grect(tb, TOOLBAR_THROBBER_AREA, &throbber_area);
    window_schedule_redraw_grect(tb->owner, &throbber_area);
}

void toolbar_set_attached(struct s_toolbar *tb, bool attached)
{
    tb->attached = attached;

}

void toolbar_throbber_progress(struct s_toolbar *tb)
{
    GRECT throbber_area;

    assert(tb->throbber.running == true);

    if(tb->throbber.running == false)
        return;

    tb->throbber.index++;
    if(tb->throbber.index > THROBBER_MAX_INDEX)
        tb->throbber.index = THROBBER_MIN_INDEX;

    tb->reflow = true;
    toolbar_get_grect(tb, TOOLBAR_THROBBER_AREA, &throbber_area);
    window_schedule_redraw_grect(tb->owner, &throbber_area);
}

bool toolbar_text_input(struct s_toolbar *tb, char *text)
{
    bool handled = true;

    LOG((""));

    return(handled);
}

bool toolbar_key_input(struct s_toolbar *tb, short nkc)
{

	assert(tb!=NULL);
	GRECT work;
	bool ret = false;

	struct gui_window *gw = window_get_active_gui_window(tb->owner);

	assert( gw != NULL );

	long ucs4;
	long ik = nkc_to_input_key(nkc, &ucs4);

	if (ik == 0) {
		if ((nkc&0xFF) >= 9) {
			ret = textarea_keypress(tb->url.textarea, ucs4);
		}
	}
	else if (ik == KEY_CR || ik == KEY_NL) {
		char tmp_url[PATH_MAX];
		if ( textarea_get_text( tb->url.textarea, tmp_url, PATH_MAX) > 0 ) {
			window_set_focus(tb->owner, BROWSER, gw->browser);
			browser_window_go(gw->browser->bw, (const char*)&tmp_url, 0, true);
			ret = true;
		}
	}
	else if (ik == KEY_COPY_SELECTION) {
		// copy whole text
		char * text;
		int len;
		len = textarea_get_text( tb->url.textarea, NULL, 0 );
		text = malloc( len+1 );
		if (text){
			textarea_get_text( tb->url.textarea, text, len+1 );
			scrap_txt_write(text);
			free( text );
		}
	}
	else if ( ik == KEY_PASTE) {
		char * clip = scrap_txt_read();
		if ( clip != NULL ){
			int clip_length = strlen( clip );
			if ( clip_length > 0 ) {
				char *utf8;
				utf8_convert_ret res;
				/* Clipboard is in local encoding so
				 * convert to UTF8 */
				res = utf8_from_local_encoding( clip, clip_length, &utf8 );
				if ( res == UTF8_CONVERT_OK ) {
					toolbar_set_url(tb, utf8);
					free(utf8);
					ret = true;
				}
			}
			free( clip );
		}
	}
	else if (ik == KEY_ESCAPE) {
		textarea_keypress( tb->url.textarea, KEY_SELECT_ALL );
		textarea_keypress( tb->url.textarea, KEY_DELETE_LEFT );
	}
	else {
		ret = textarea_keypress( tb->url.textarea, ik );
	}

	return( ret );
}


void toolbar_mouse_input(struct s_toolbar *tb, short obj)
{
    LOG((""));
    GRECT work;
	short mx, my, mb, kstat;
	int old;

    if (obj==TOOLBAR_URL_AREA){

        graf_mkstate( &mx, &my, &mb,  &kstat );
        toolbar_get_grect(tb, TOOLBAR_URL_AREA, &work);
        mx -= work.g_x;
        my -= work.g_y;

	    /* TODO: reset mouse state of browser window? */
	    /* select whole text when newly focused, otherwise set caret to
	        end of text */
        if (!window_url_widget_has_focus(tb->owner)) {
            window_set_focus(tb->owner, URL_WIDGET, (void*)&tb->url );
        }
        /* url widget has focus and mouse button is still pressed... */
        else if (mb & 1) {

            textarea_mouse_action(tb->url.textarea, BROWSER_MOUSE_DRAG_1,
									mx, my );
			short prev_x = mx;
			short prev_y = my;
			do {
				if (abs(prev_x-mx) > 5 || abs(prev_y-my) > 5) {
					textarea_mouse_action( tb->url.textarea,
										BROWSER_MOUSE_HOLDING_1, mx, my );
					prev_x = mx;
					prev_y = my;
					window_schedule_redraw_grect(tb->owner, &work);
					window_process_redraws(tb->owner);
				}
				graf_mkstate( &mx, &my, &mb,  &kstat );
				mx = mx - (work.g_x + TOOLBAR_URL_MARGIN_LEFT);
				my = my - (work.g_y + TOOLBAR_URL_MARGIN_TOP);
            } while (mb & 1);

			textarea_drag_end( tb->url.textarea, 0, mx, my );
        }
        else {
            /* when execution reaches here, mouse input is a click or dclick */
            /* TODO: recognize click + shitoolbar_update_buttonsft key */
			int mstate = BROWSER_MOUSE_PRESS_1;
			if( (kstat & (K_LSHIFT|K_RSHIFT)) != 0 ){
				mstate = BROWSER_MOUSE_MOD_1;
			}
            if( aes_event_out.emo_mclicks == 2 ){
				textarea_mouse_action( tb->url.textarea,
						BROWSER_MOUSE_DOUBLE_CLICK | BROWSER_MOUSE_CLICK_1, mx,
						my);
                toolbar_get_grect(tb, TOOLBAR_URL_AREA, &work);
                window_schedule_redraw_grect(tb->owner, &work);
			} else {
				textarea_mouse_action(tb->url.textarea,
						BROWSER_MOUSE_PRESS_1, mx, my );
			}
        }
    } else {
        struct s_tb_button *bt = find_button(tb, obj);
        if (bt != NULL && bt->state != button_off) {
            bt->cb_click(tb);
            struct gui_window * gw = window_get_active_gui_window(tb->owner);
            toolbar_update_buttons(tb, gw->browser->bw,
                                   0);
        }

    }
}


/**
* Receive a specific region of the toolbar.
* @param tb - the toolbar pointer
* @param which - the area to retrieve:  TOOLBAR_URL_AREA,
*                                       TOOLBAR_NAVIGATION_AREA,
*                                       TOOLBAR_THROBBER_AREA
* @param dst - GRECT pointer receiving the area.
*/

void toolbar_get_grect(struct s_toolbar *tb, short which, GRECT *dst)
{
    if (tb->reflow == true) {
        toolbar_reflow(tb);
    }

    objc_offset(aes_toolbar, which, &dst->g_x, &dst->g_y);
    dst->g_w = aes_toolbar[which].ob_width;
    dst->g_h = aes_toolbar[which].ob_height;

    //printf("Toolbar get grect (%d): ", which);
    //dbg_grect("", dst);
}


struct text_area *toolbar_get_textarea(struct s_toolbar *tb,
                                       enum toolbar_textarea which)
{
    return(tb->url.textarea);
}


/* public event handler */
void toolbar_back_click(struct s_toolbar *tb)
{
    struct browser_window * bw;
    struct gui_window * gw;

    gw = window_get_active_gui_window(tb->owner);
    assert(gw != NULL);
    bw = gw->browser->bw;
    assert(bw != NULL);

    if( history_back_available(bw->history) )
		history_back(bw, bw->history);
}

void toolbar_reload_click(struct s_toolbar *tb)
{
    struct browser_window * bw;
    struct gui_window * gw;

    gw = window_get_active_gui_window(tb->owner);
    assert(gw != NULL);
    bw = gw->browser->bw;
    assert(bw != NULL);

	browser_window_reload(bw, true);
}

void toolbar_forward_click(struct s_toolbar *tb)
{
    struct browser_window * bw;
    struct gui_window * gw;

    gw = window_get_active_gui_window(tb->owner);
    assert(gw != NULL);
    bw = gw->browser->bw;
    assert(bw != NULL);

	if (history_forward_available(bw->history))
		history_forward(bw, bw->history);
}

void toolbar_home_click(struct s_toolbar *tb)
{
    struct browser_window * bw;
    struct gui_window * gw;

    gw = window_get_active_gui_window(tb->owner);
    assert(gw != NULL);
    bw = gw->browser->bw;
    assert(bw != NULL);
	browser_window_go(bw, option_homepage_url, 0, true);
}


void toolbar_stop_click(struct s_toolbar *tb)
{
    struct browser_window * bw;
    struct gui_window * gw;

    gw = window_get_active_gui_window(tb->owner);
    assert(gw != NULL);
    bw = gw->browser->bw;
    assert(bw != NULL);

	browser_window_stop(bw);
}

