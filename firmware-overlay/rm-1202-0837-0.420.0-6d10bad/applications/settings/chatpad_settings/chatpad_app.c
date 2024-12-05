#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <rpc/rpc_keyboard.h>
#include <storage/storage.h>

#define TAG                 "Chatpad"
#define CHATPAD_DATA_FOLDER "/ext/apps_data/chatpad"
#define CHATPAD_DATA_FILE   CHATPAD_DATA_FOLDER "/chatpad.key"

// Change this to BACKLIGHT_AUTO if you don't want the backlight to be continuously on.
#define BACKLIGHT_ON 1

typedef enum {
    ChatpadSubmenuIndexConfigure,
    ChatpadSubmenuIndexChatpad,
    ChatpadSubmenuIndexAbout,
} ChatpadSubmenuIndex;

// Each view is a screen we show the user.
typedef enum {
    ChatpadViewSubmenu, // The menu when the app starts
    ChatpadViewTextInput, // Input for configuring text settings
    ChatpadViewConfigure, // The configuration screen
    ChatpadViewChatpad, // The main screen
    ChatpadViewAbout, // The about screen with directions, link to social channel, etc.
} ChatpadView;

typedef enum {
    ChatpadEventIdRedrawScreen = 0, // Custom event to redraw the screen
    ChatpadEventIdOkPressed, // Custom event to process OK button getting pressed down
} ChatpadEventId;

typedef struct {
    ViewDispatcher* view_dispatcher; // Switches between our views
    NotificationApp* notifications; // Used for controlling the backlight
    Submenu* submenu; // The application menu
    TextInput* text_input; // The text input screen
    VariableItemList* variable_item_list_config; // The configuration screen
    View* view_chatpad; // The main screen
    Widget* widget_about; // The about screen

    VariableItem* setting_macro; // Allow choose the macro to edit
    uint8_t setting_macro_index; // The current setting index

    char* temp_buffer; // Temporary buffer for text input
    uint32_t temp_buffer_size; // Size of temporary buffer

    FuriTimer* timer; // Timer for redrawing the screen
} ChatpadApp;

typedef struct {
    bool chatpad_enabled; // The chatpad enabled setting
    FuriPubSubSubscription* keyboard_subscription;
    RpcKeyboardChatpadStatus chatpad_status;
    char message[256];
    uint16_t length;
    RpcKeyboardEventType type;
} ChatpadModel;

/**
 * @brief      Callback for exiting the application.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to exit the application.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t chatpad_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to navigate to the submenu.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t chatpad_navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return ChatpadViewSubmenu;
}

/**
 * @brief      Callback for returning to configure screen.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to navigate to the configure screen.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t chatpad_navigation_configure_callback(void* _context) {
    UNUSED(_context);
    return ChatpadViewConfigure;
}

/**
 * @brief      Handle submenu item selection.
 * @details    This function is called when user selects an item from the submenu.
 * @param      context  The context - ChatpadApp object.
 * @param      index     The ChatpadSubmenuIndex item that was clicked.
*/
static void chatpad_submenu_callback(void* context, uint32_t index) {
    ChatpadApp* app = (ChatpadApp*)context;
    switch(index) {
    case ChatpadSubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, ChatpadViewConfigure);
        break;
    case ChatpadSubmenuIndexChatpad:
        view_dispatcher_switch_to_view(app->view_dispatcher, ChatpadViewChatpad);
        break;
    case ChatpadSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, ChatpadViewAbout);
        break;
    default:
        break;
    }
}

static RpcKeyboardChatpadStatus chatpad_device_status() {
    RpcKeyboardChatpadStatus status = RpcKeyboardChatpadStatusError;
    RpcKeyboard* rpc_keyboard = furi_record_open(RECORD_RPC_KEYBOARD);
    if(rpc_keyboard) {
        status = rpc_keyboard_chatpad_status(rpc_keyboard);
    }
    furi_record_close(RECORD_RPC_KEYBOARD);
    return status;
}

static bool chatpad_device_enabled(void) {
    RpcKeyboardChatpadStatus status = chatpad_device_status();
    return (status != RpcKeyboardChatpadStatusStopped) &&
           (status != RpcKeyboardChatpadStatusError);
}

static void chatpad_device_enable(bool enable) {
    RpcKeyboard* rpc_keyboard = furi_record_open(RECORD_RPC_KEYBOARD);
    if(rpc_keyboard) {
        if(enable) {
            rpc_keyboard_chatpad_start(rpc_keyboard);
        } else {
            rpc_keyboard_chatpad_stop(rpc_keyboard);
        }
    }
    furi_record_close(RECORD_RPC_KEYBOARD);
}

static const char* setting_chatpad_enabled_label = "Chatpad";
static char* setting_chatpad_enabled_names[] = {"Off", "On"};
static void chatpad_setting_chatpad_enabled_change(VariableItem* item) {
    ChatpadApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, setting_chatpad_enabled_names[index]);
    ChatpadModel* model = view_get_model(app->view_chatpad);
    model->chatpad_enabled = index == 1;
    chatpad_device_enable(model->chatpad_enabled);
}

static void chatpad_save_macro(char macro, char* value) {
    if(macro < 'a' || macro > 'z') {
        return;
    }
    uint32_t index = macro - 'a';
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage) {
        File* file = storage_file_alloc(storage);
        do {
            char* null_char = "\0\0\0\0\0\0\0\0";
            if(!storage_dir_exists(storage, CHATPAD_DATA_FOLDER)) {
                storage_common_mkdir(storage, CHATPAD_DATA_FOLDER);
            }
            if(!storage_file_open(file, CHATPAD_DATA_FILE, FSAM_WRITE, FSOM_OPEN_ALWAYS)) break;
            if(storage_file_size(file) == 0) {
                // Wipe the file with null characters
                for(int i = 0; i < 26; i++) {
                    for(int j = 0; j < 32; j++) {
                        storage_file_write(file, null_char, 8);
                    }
                }
            }
            storage_file_seek(file, index * 256, true);
            if(strlen(value)) {
                size_t len = strlen(value);
                if(len > 255) {
                    len = 255;
                }
                storage_file_write(file, value, len);
            }
            storage_file_write(file, null_char, 1);
        } while(0);
        storage_file_free(file);
    }
    furi_record_close(RECORD_STORAGE);
}

static void chatpad_load_macros() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage) {
        File* file = storage_file_alloc(storage);
        do {
            RpcKeyboard* rpc_keyboard = furi_record_open(RECORD_RPC_KEYBOARD);
            if(rpc_keyboard) {
                if(!storage_file_open(file, CHATPAD_DATA_FILE, FSAM_READ, FSOM_OPEN_EXISTING))
                    break;
                char buffer[256];
                for(int i = 0; i < 26; i++) {
                    storage_file_seek(file, i * 256, true);
                    if(storage_file_read(file, buffer, 256) == 0) {
                        continue;
                    }
                    buffer[255] = '\0';
                    FURI_LOG_D(TAG, "Read macro %c: %s", 'a' + i, buffer);
                    if(buffer[0] == '\0') {
                        continue;
                    }
                    rpc_keyboard_set_macro(rpc_keyboard, 'a' + i, buffer);
                }
            }
        } while(0);
        furi_record_close(RECORD_RPC_KEYBOARD);
        storage_file_free(file);
    }
    furi_record_close(RECORD_STORAGE);
}

static void chatpad_setting_macro_updated(void* context) {
    ChatpadApp* app = (ChatpadApp*)context;
    FURI_LOG_D(TAG, "Text updated: %s", app->temp_buffer ? app->temp_buffer : "NULL");
    RpcKeyboard* rpc_keyboard = furi_record_open(RECORD_RPC_KEYBOARD);
    if(rpc_keyboard) {
        rpc_keyboard_set_macro(rpc_keyboard, 'a' + app->setting_macro_index, app->temp_buffer);
        chatpad_save_macro('a' + app->setting_macro_index, app->temp_buffer);
    }
    furi_record_close(RECORD_RPC_KEYBOARD);
    view_dispatcher_switch_to_view(app->view_dispatcher, ChatpadViewConfigure);
}

static char setting_chatpad_macro_name[2];
static void chatpad_setting_macro_change(VariableItem* item) {
    ChatpadApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->setting_macro_index = index;
    setting_chatpad_macro_name[0] = 'A' + index;
    setting_chatpad_macro_name[1] = '\0';
    variable_item_set_current_value_text(item, setting_chatpad_macro_name);
}

/**
 * @brief      Callback when item in configuration screen is clicked.
 * @details    This function is called when user clicks OK on an item in the configuration screen.
 *            If the item clicked is our text field then we switch to the text input screen.
 * @param      context  The context - ChatpadApp object.
 * @param      index - The index of the item that was clicked.
*/
static void chatpad_setting_item_clicked(void* context, uint32_t index) {
    ChatpadApp* app = (ChatpadApp*)context;

    // Our configuration UI has the 2nd item as a text field.
    if(index == 1) {
        // Header to display on the text input screen.
        text_input_set_header_text(app->text_input, "Enter macro");

        // Copy the current macro text into the temporary buffer.
        RpcKeyboard* rpc_keyboard = furi_record_open(RECORD_RPC_KEYBOARD);
        if(rpc_keyboard) {
            char* macro_text =
                rpc_keyboard_get_macro(rpc_keyboard, 'a' + app->setting_macro_index);
            if(macro_text) {
                strncpy(app->temp_buffer, macro_text, app->temp_buffer_size);
                free(macro_text);
            } else {
                app->temp_buffer[0] = 'a' + app->setting_macro_index;
                app->temp_buffer[1] = '\0';
            }
        }
        furi_record_close(RECORD_RPC_KEYBOARD);

        // Configure the text input.  When user enters text and clicks OK, chatpad_setting_text_updated be called.
        bool clear_previous_text = false;
        text_input_set_result_callback(
            app->text_input,
            chatpad_setting_macro_updated,
            app,
            app->temp_buffer,
            app->temp_buffer_size,
            clear_previous_text);

        // Pressing the BACK button will reload the configure screen.
        view_set_previous_callback(
            text_input_get_view(app->text_input), chatpad_navigation_configure_callback);

        // Show text input dialog.
        view_dispatcher_switch_to_view(app->view_dispatcher, ChatpadViewTextInput);
    }
}

/**
 * @brief      Callback for drawing the chatpad screen.
 * @details    This function is called when the screen needs to be redrawn, like when the model gets updated.
 * @param      canvas  The canvas to draw on.
 * @param      model   The model - MyModel object.
*/
static void chatpad_view_chatpad_draw_callback(Canvas* canvas, void* model) {
    ChatpadModel* my_model = (ChatpadModel*)model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 1, 10, "CHATPAD");

    canvas_set_font(canvas, FontSecondary);

    if(my_model->chatpad_enabled) {
        canvas_draw_str(canvas, 1, 20, "Chatpad is ON");
    } else {
        canvas_draw_str(canvas, 1, 20, "Chatpad is OFF");
    }

    switch(my_model->chatpad_status) {
    case RpcKeyboardChatpadStatusStopped:
        canvas_draw_str(canvas, 1, 30, "Chatpad is stopped");
        break;
    case RpcKeyboardChatpadStatusStarted:
        canvas_draw_str(canvas, 1, 30, "Chatpad is not ready");
        break;
    case RpcKeyboardChatpadStatusReady:
        canvas_draw_str(canvas, 1, 30, "Chatpad is READY");
        break;
    case RpcKeyboardChatpadStatusError:
        canvas_draw_str(canvas, 1, 30, "Chatpad has ERROR");
        break;
    default:
        break;
    }

    FuriString* xstr = furi_string_alloc();
    if(my_model->length > 0) {
        if(my_model->type == RpcKeyboardEventTypeTextEntered) {
            furi_string_printf(xstr, "Text: %s", my_model->message);
        } else if(my_model->type == RpcKeyboardEventTypeCharEntered) {
            furi_string_printf(xstr, "Char: %c", my_model->message[0]);
        } else if(my_model->type == RpcKeyboardEventTypeMacroEntered) {
            furi_string_printf(xstr, "Macro: %s", my_model->message);
        }
    }

    canvas_draw_str(canvas, 10, 40, furi_string_get_cstr(xstr));
    furi_string_free(xstr);
}

/**
 * @brief      Callback for timer elapsed.
 * @details    This function is called when the timer is elapsed.  We use this to queue a redraw event.
 * @param      context  The context - ChatpadApp object.
*/
static void chatpad_view_chatpad_timer_callback(void* context) {
    ChatpadApp* app = (ChatpadApp*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, ChatpadEventIdRedrawScreen);
}

static void chatpad_view_chatpad_keyboard_callback(const void* message, void* context) {
    ChatpadApp* app = context;
    const RpcKeyboardEvent* event = message;

    with_view_model(
        app->view_chatpad,
        ChatpadModel * model,
        {
            furi_mutex_acquire(event->data.mutex, FuriWaitForever);
            model->type = event->type;
            model->length = event->data.length;
            strncpy(model->message, event->data.message, sizeof(model->message));
            furi_mutex_release(event->data.mutex);
        },
        true);
}

/**
 * @brief      Callback when the user starts the chatpad screen.
 * @details    This function is called when the user enters the chatpad screen.  We start a timer to
 *           redraw the screen periodically (so the random number is refreshed).
 * @param      context  The context - ChatpadApp object.
*/
static void chatpad_view_chatpad_enter_callback(void* context) {
    uint32_t period = furi_ms_to_ticks(500);
    ChatpadApp* app = (ChatpadApp*)context;
    furi_assert(app->timer == NULL);
    app->timer =
        furi_timer_alloc(chatpad_view_chatpad_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(app->timer, period);

    RpcKeyboard* rpc_keyboard = furi_record_open(RECORD_RPC_KEYBOARD);
    FuriPubSub* rpc_keyboard_pubsub = rpc_keyboard_get_pubsub(rpc_keyboard);
    if(rpc_keyboard_pubsub != NULL) {
        with_view_model(
            app->view_chatpad,
            ChatpadModel * model,
            {
                model->keyboard_subscription = furi_pubsub_subscribe(
                    rpc_keyboard_pubsub, chatpad_view_chatpad_keyboard_callback, app);
            },
            false);
    }
    furi_record_close(RECORD_RPC_KEYBOARD);
}

/**
 * @brief      Callback when the user exits the chatpad screen.
 * @details    This function is called when the user exits the chatpad screen.  We stop the timer.
 * @param      context  The context - ChatpadApp object.
*/
static void chatpad_view_chatpad_exit_callback(void* context) {
    ChatpadApp* app = (ChatpadApp*)context;
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    app->timer = NULL;

    RpcKeyboard* rpc_keyboard = furi_record_open(RECORD_RPC_KEYBOARD);
    FuriPubSub* rpc_keyboard_pubsub = rpc_keyboard_get_pubsub(rpc_keyboard);
    if(rpc_keyboard_pubsub != NULL) {
        with_view_model(
            app->view_chatpad,
            ChatpadModel * model,
            {
                furi_pubsub_unsubscribe(rpc_keyboard_pubsub, model->keyboard_subscription);
                model->keyboard_subscription = NULL;
            },
            false);
    }
    furi_record_close(RECORD_RPC_KEYBOARD);
}

/**
 * @brief      Callback for custom events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - ChatpadEventId value.
 * @param      context  The context - ChatpadApp object.
*/
static bool chatpad_view_chatpad_custom_event_callback(uint32_t event, void* context) {
    ChatpadApp* app = (ChatpadApp*)context;
    switch(event) {
    case ChatpadEventIdRedrawScreen:
        // Redraw screen by passing true to last parameter of with_view_model.
        {
            bool redraw = true;
            with_view_model(
                app->view_chatpad,
                ChatpadModel * model,
                { model->chatpad_status = chatpad_device_status(); },
                redraw);
            return true;
        }
    case ChatpadEventIdOkPressed:
        // OK button toggles chatpad.
        with_view_model(
            app->view_chatpad,
            ChatpadModel * model,
            {
                model->chatpad_enabled = !model->chatpad_enabled;
                chatpad_device_enable(model->chatpad_enabled);
            },
            true);
        return true;
    default:
        return false;
    }
}

/**
 * @brief      Callback for chatpad screen input.
 * @details    This function is called when the user presses a button while on the chatpad screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - ChatpadApp object.
 * @return     true if the event was handled, false otherwise.
*/
static bool chatpad_view_chatpad_input_callback(InputEvent* event, void* context) {
    ChatpadApp* app = (ChatpadApp*)context;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk) {
            view_dispatcher_send_custom_event(app->view_dispatcher, ChatpadEventIdOkPressed);
            return true;
        }
    }

    return false;
}

/**
 * @brief      Allocate the chatpad application.
 * @details    This function allocates the chatpad application resources.
 * @return     ChatpadApp object.
*/
static ChatpadApp* chatpad_app_alloc() {
    ChatpadApp* app = (ChatpadApp*)malloc(sizeof(ChatpadApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, "Config", ChatpadSubmenuIndexConfigure, chatpad_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Chatpad", ChatpadSubmenuIndexChatpad, chatpad_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", ChatpadSubmenuIndexAbout, chatpad_submenu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), chatpad_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, ChatpadViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, ChatpadViewSubmenu);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ChatpadViewTextInput, text_input_get_view(app->text_input));
    app->temp_buffer_size = 32;
    app->temp_buffer = (char*)malloc(app->temp_buffer_size);

    app->variable_item_list_config = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list_config);
    VariableItem* item = variable_item_list_add(
        app->variable_item_list_config,
        setting_chatpad_enabled_label,
        COUNT_OF(setting_chatpad_enabled_names),
        chatpad_setting_chatpad_enabled_change,
        app);
    bool chatpad_enabled = chatpad_device_enabled();
    uint8_t setting_chatpad_enabled_index = chatpad_enabled ? 1 : 0;
    variable_item_set_current_value_index(item, setting_chatpad_enabled_index);
    variable_item_set_current_value_text(
        item, setting_chatpad_enabled_names[setting_chatpad_enabled_index]);

    app->setting_macro = variable_item_list_add(
        app->variable_item_list_config, "Macro", 26, chatpad_setting_macro_change, app);
    variable_item_set_current_value_text(app->setting_macro, "A");
    variable_item_list_set_enter_callback(
        app->variable_item_list_config, chatpad_setting_item_clicked, app);
    app->setting_macro_index = 0;

    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_config),
        chatpad_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        ChatpadViewConfigure,
        variable_item_list_get_view(app->variable_item_list_config));

    app->view_chatpad = view_alloc();
    view_set_draw_callback(app->view_chatpad, chatpad_view_chatpad_draw_callback);
    view_set_input_callback(app->view_chatpad, chatpad_view_chatpad_input_callback);
    view_set_previous_callback(app->view_chatpad, chatpad_navigation_submenu_callback);
    view_set_enter_callback(app->view_chatpad, chatpad_view_chatpad_enter_callback);
    view_set_exit_callback(app->view_chatpad, chatpad_view_chatpad_exit_callback);
    view_set_context(app->view_chatpad, app);
    view_set_custom_callback(app->view_chatpad, chatpad_view_chatpad_custom_event_callback);
    view_allocate_model(app->view_chatpad, ViewModelTypeLockFree, sizeof(ChatpadModel));
    ChatpadModel* model = view_get_model(app->view_chatpad);
    model->chatpad_enabled = chatpad_enabled;
    view_dispatcher_add_view(app->view_dispatcher, ChatpadViewChatpad, app->view_chatpad);

    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "Xbox 360 Chatpad!\nauthor: @codeallnight\nhttps://discord.com/invite/NsjCvqwPAd\nhttps://youtube.com/@MrDerekJamison");
    view_set_previous_callback(
        widget_get_view(app->widget_about), chatpad_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, ChatpadViewAbout, widget_get_view(app->widget_about));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
#endif

    return app;
}

/**
 * @brief      Free the chatpad application.
 * @details    This function frees the chatpad application resources.
 * @param      app  The chatpad application object.
*/
static void chatpad_app_free(ChatpadApp* app) {
#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
#endif
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(app->view_dispatcher, ChatpadViewTextInput);
    text_input_free(app->text_input);
    free(app->temp_buffer);
    view_dispatcher_remove_view(app->view_dispatcher, ChatpadViewAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, ChatpadViewChatpad);
    view_free(app->view_chatpad);
    view_dispatcher_remove_view(app->view_dispatcher, ChatpadViewConfigure);
    variable_item_list_free(app->variable_item_list_config);
    view_dispatcher_remove_view(app->view_dispatcher, ChatpadViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
}

/**
 * @brief      Main function for chatpad application.
 * @details    This function is the entry point for the chatpad application.  It should be defined in
 *           application.fam as the entry_point setting.
 * @param      _p  Input parameter - unused
 * @return     0 - Success
*/
int32_t chatpad_app(void* _p) {
    UNUSED(_p);

    ChatpadApp* app = chatpad_app_alloc();
    chatpad_load_macros();
    view_dispatcher_run(app->view_dispatcher);

    chatpad_app_free(app);
    return 0;
}
