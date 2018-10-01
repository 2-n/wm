#include <xcb/xcb.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

static xcb_connection_t   *c;
static xcb_screen_t       *scr;
static xcb_window_t       focwin;

enum { INACTIVE, ACTIVE };

static void   subscribe(xcb_window_t);
static int    start(void);
static void   cleanexit(void);
static void   foc(xcb_window_t, int);

static void
cleanexit(void)
{
   /* clean exit */
   if (c != NULL)
      xcb_disconnect(c);
}

static int
start(void)
{
   uint32_t values[2];
   int mask;

   if (xcb_connection_has_error(c = xcb_connect(NULL, NULL)))
      return -1;

   scr = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
   focwin = scr->root;

   mask = XCB_CW_EVENT_MASK;
   values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
   xcb_change_window_attributes_checked(c, scr->root, mask, values);
   
   xcb_grab_button(c, 0, scr->root, XCB_EVENT_MASK_BUTTON_PRESS | 
      XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
      XCB_GRAB_MODE_ASYNC, scr->root, XCB_NONE, 1, XCB_MOD_MASK_4);

   xcb_grab_button(c, 0, scr->root, XCB_EVENT_MASK_BUTTON_PRESS | 
      XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
      XCB_GRAB_MODE_ASYNC, scr->root, XCB_NONE, 3, XCB_MOD_MASK_4); 

   xcb_flush(c);

   return 0;
}

static void
foc(xcb_window_t win, int mode)
{
   if (mode == ACTIVE) {
      xcb_set_input_focus(c, XCB_INPUT_FOCUS_POINTER_ROOT, win, XCB_CURRENT_TIME);
      if (win != focwin) {
         foc(focwin, INACTIVE);
         focwin = win;
      }
   }
}

static void
subscribe(xcb_window_t win)
{
   uint32_t values[2];

   values[0] = XCB_EVENT_MASK_ENTER_WINDOW;
   values[1] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
   xcb_change_window_attributes(c, win, XCB_CW_EVENT_MASK, values);
}

static void
loop(void)
{
   uint32_t values[3];

   xcb_generic_event_t        *ev;
   xcb_get_geometry_reply_t   *geo;
   xcb_window_t               win = 0;

   for (;;) {
      ev = xcb_wait_for_event(c);

      if (!ev)
         errx(1, "xcb connection broken, :c");

      switch (ev->response_type & ~0x80) {
         case XCB_CREATE_NOTIFY: {
            xcb_create_notify_event_t *e;
            e = (xcb_create_notify_event_t *)ev;
         
            if (!e->override_redirect) {
             subscribe(e->window);
             foc(e->window, ACTIVE);
            }
           } break;
         
           case XCB_DESTROY_NOTIFY: {
            xcb_destroy_notify_event_t *e;
            e = (xcb_destroy_notify_event_t *)ev;
         
            xcb_kill_client(c, e->window);
         } break;

         /* sloppy focus */
         
         case XCB_ENTER_NOTIFY: {
            xcb_enter_notify_event_t *e;
            e = (xcb_enter_notify_event_t *)ev;
            foc(e->event, ACTIVE);
         } break;

         /* mouse move + resize */

         case XCB_BUTTON_PRESS: {
            xcb_button_press_event_t *e;
            e = ( xcb_button_press_event_t *)ev;
            win = e->child;
         
            if (!win || win == scr->root)
             break;
         
            values[0] = XCB_STACK_MODE_ABOVE;
            xcb_configure_window(c, win,
              XCB_CONFIG_WINDOW_STACK_MODE, values);
            geo = xcb_get_geometry_reply(c,
              xcb_get_geometry(c, win), NULL);
            if (e->detail == 1) {
             values[2] = 1;
             xcb_warp_pointer(c, XCB_NONE, win, 0, 0, 0,
              0, geo->width/2, geo->height/2);
            } else {
             values[2] = 3;
             xcb_warp_pointer(c, XCB_NONE, win, 0, 0, 0,
               0, geo->width, geo->height);
            }
            xcb_grab_pointer(c, 0, scr->root,
             XCB_EVENT_MASK_BUTTON_RELEASE
             | XCB_EVENT_MASK_BUTTON_MOTION
             | XCB_EVENT_MASK_POINTER_MOTION_HINT,
             XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
             scr->root, XCB_NONE, XCB_CURRENT_TIME);
            xcb_flush(c);
           } break;
         
           case XCB_MOTION_NOTIFY: {
            xcb_query_pointer_reply_t *pointer;
            pointer = xcb_query_pointer_reply(c,
              xcb_query_pointer(c, scr->root), 0);
            if (values[2] == 1) {
             geo = xcb_get_geometry_reply(c,
              xcb_get_geometry(c, win), NULL);
             if (!geo)
              break;
         
             values[0] = (pointer->root_x + geo->width / 2
              > scr->width_in_pixels)
              ? scr->width_in_pixels - geo->width
              : pointer->root_x - geo->width / 2;
             values[1] = (pointer->root_y + geo->height / 2
              > scr->height_in_pixels)
              ? (scr->height_in_pixels - geo->height)
              : pointer->root_y - geo->height / 2;
         
             if (pointer->root_x < geo->width/2)
              values[0] = 0;
             if (pointer->root_y < geo->height/2)
              values[1] = 0;
         
             xcb_configure_window(c, win,
              XCB_CONFIG_WINDOW_X
              | XCB_CONFIG_WINDOW_Y, values);
             xcb_flush(c);
            } else if (values[2] == 3) {
             geo = xcb_get_geometry_reply(c,
              xcb_get_geometry(c, win), NULL);
             values[0] = pointer->root_x - geo->x;
             values[1] = pointer->root_y - geo->y;
             xcb_configure_window(c, win,
              XCB_CONFIG_WINDOW_WIDTH
              | XCB_CONFIG_WINDOW_HEIGHT, values);
             xcb_flush(c);
            }
           } break;
           
           case XCB_BUTTON_RELEASE:
            foc(win, ACTIVE);
            xcb_ungrab_pointer(c, XCB_CURRENT_TIME);
         break;

         case XCB_CONFIGURE_NOTIFY: {
            xcb_configure_notify_event_t *e;
            e = (xcb_configure_notify_event_t *)ev;
         
            if (e->window != focwin)
             foc(e->window, INACTIVE);
         
            foc(focwin, ACTIVE);
         } break;
      }
      xcb_flush(c);
      free(ev);
   }
}

int
main(void)
{
   /* clean exit */
   atexit(cleanexit);

   if (start() < 0)
      errx(EXIT_FAILURE, "couldnt connect to x, :c");
      
   for (;;)
      loop();

   return EXIT_FAILURE;
}
