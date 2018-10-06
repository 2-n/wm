#include <xcb/xcb.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

static xcb_connection_t   *connection;
static xcb_screen_t       *screen;
static xcb_window_t       focused_window;

static void   subscribe(xcb_window_t);
static int    start(void);
static void   cleanexit(void);
static void   focus(xcb_window_t);

static void
cleanexit(void)
{
    /* clean exit */
    if (connection) {
        xcb_disconnect(connection);
    }
}

static int
start(void)
{
    unsigned int values[1] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
    };
    int event_mask = XCB_CW_EVENT_MASK;

    if (xcb_connection_has_error(connection = xcb_connect(NULL, NULL))) {
        return 0;
    }

    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    focused_window = screen->root;

    xcb_change_window_attributes_checked(connection, screen->root,
        event_mask, values);

    int buttons[2] = {1, 3};
    int index, button;

    for (index = 0; index < 2; ++index) {
        button = buttons[index];
        xcb_grab_button(connection, XCB_NONE, screen->root,
            XCB_EVENT_MASK_BUTTON_PRESS
            | XCB_EVENT_MASK_BUTTON_RELEASE,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
            screen->root, XCB_NONE, button, XCB_MOD_MASK_4);
    }

    xcb_flush(connection);
    return 1;
}

static void
focus(xcb_window_t window)
{
    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT,
        window, XCB_CURRENT_TIME);
    focused_window = window;
}

static void
subscribe(xcb_window_t window)
{
    unsigned int values[2] = {
        XCB_EVENT_MASK_ENTER_WINDOW,
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
    };

    xcb_change_window_attributes(connection, window,
        XCB_CW_EVENT_MASK, values);
}

static void
loop(void)
{
    unsigned int values[3];

    xcb_generic_event_t *generic_event;
    xcb_get_geometry_reply_t *geometry;
    xcb_window_t window = XCB_WINDOW_NONE;

    for (;;) {

        generic_event = xcb_wait_for_event(connection);

        if (!generic_event) {
            errx(EXIT_FAILURE, "X Connection broken :c");
        }

        switch (generic_event->response_type & ~0x80) {
            case XCB_CREATE_NOTIFY: {
                    xcb_create_notify_event_t *event = \
                        (xcb_create_notify_event_t *)generic_event;
                    window = event->window;

                    if (event->override_redirect) {
                        break;
                    }

                    subscribe(window);
                    focus(window);
                }
                break;

            case XCB_DESTROY_NOTIFY: {
                    xcb_destroy_notify_event_t *event = \
                        (xcb_destroy_notify_event_t *)generic_event;
                    window = event->window;

                    xcb_kill_client(connection, window);
                }
                break;

            case XCB_ENTER_NOTIFY: {
                    xcb_enter_notify_event_t *event = \
                        (xcb_enter_notify_event_t *)generic_event;
                    window = event->event;

                    focus(window);
                }
                break;

            case XCB_BUTTON_PRESS: {
                    xcb_button_press_event_t *event = \
                        (xcb_button_press_event_t *)generic_event;
                    window = event->child;

                    if (!window || window == screen->root) {
                        break;
                    }

                    geometry = xcb_get_geometry_reply(connection,
                        xcb_get_geometry(connection, window), NULL);

                    if (!geometry) {
                        break;
                    }

                    values[0] = XCB_STACK_MODE_ABOVE;
                    xcb_configure_window(connection, window,
                        XCB_CONFIG_WINDOW_STACK_MODE, values);

                    unsigned short width = geometry->width, \
                        height = geometry->height;

                    if (event->detail == 1) {
                        values[2] = 1;

                        width /= 2;
                        height /= 2;
                    } else {
                        values[2] = 3;
                    }

                    xcb_warp_pointer(connection, XCB_NONE, window, 0, 0, 0, 0,
                        width, height);
                    xcb_grab_pointer(connection, XCB_NONE, screen->root,
                        XCB_EVENT_MASK_BUTTON_RELEASE
                        | XCB_EVENT_MASK_BUTTON_MOTION
                        | XCB_EVENT_MASK_POINTER_MOTION_HINT,
                        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                        screen->root, XCB_NONE, XCB_CURRENT_TIME);
                }

            case XCB_MOTION_NOTIFY: {
                    xcb_query_pointer_reply_t *pointer = \
                        xcb_query_pointer_reply(connection,
                            xcb_query_pointer(connection, screen->root),
                            NULL);
                    geometry = xcb_get_geometry_reply(connection,
                        xcb_get_geometry(connection, window), NULL);

                    if (!geometry) {
                        break;
                    }

                    if (values[2] == 1) {
                        if (pointer->root_x + geometry->width / 2 >
                            screen->width_in_pixels) {
                            values[0] = screen->width_in_pixels - \
                                geometry->width;
                        } else {
                            values[0] = pointer->root_x - geometry->width / 2;
                        }

                        if (pointer->root_y + geometry->height / 2 >
                            screen->height_in_pixels) {
                            values[1] = screen->height_in_pixels - \
                                geometry->height;
                        } else {
                            values[1] = pointer->root_y - geometry->height / 2;
                        }

                        if (pointer->root_x < geometry->width / 2) {
                            values[0] = 0;
                        }

                        if (pointer->root_y < geometry->height / 2) {
                            values[1] = 0;
                        }

                        xcb_configure_window(connection, window,
                            XCB_CONFIG_WINDOW_X
                            | XCB_CONFIG_WINDOW_Y, values);
                    } else {
                        values[0] = pointer->root_x - geometry->x;
                        values[1] = pointer->root_y - geometry->y;

                        xcb_configure_window(connection, window,
                            XCB_CONFIG_WINDOW_WIDTH
                            | XCB_CONFIG_WINDOW_HEIGHT, values);
                    }

                }
                break;

            case XCB_BUTTON_RELEASE:
                focus(window);
                xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
                break;

            case XCB_CONFIGURE_NOTIFY: {
                    xcb_configure_notify_event_t *event = \
                        (xcb_configure_notify_event_t *)generic_event;
                    window = event->window;

                    if (window != focused_window) {
                        focus(window);
                    }
                }
                break;
        }

        xcb_flush(connection);
        free(generic_event);

    }
}

int
main(void)
{
    atexit(cleanexit);

    if (!start()) {
        errx(EXIT_FAILURE, "Couldn't connect to X :c");
    }

    loop();

    return EXIT_SUCCESS;
}
