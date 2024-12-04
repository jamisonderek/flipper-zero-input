#include <core/record.h>
#include <furi.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/rpc_keyboard.h>
#include <rpc/rpc_chatpad.h>

#define TAG "RpcKeyboard"

struct RpcKeyboard {
    RpcKeyboardFunctions funcs;
    FuriPubSub* event_pubsub;
    RpcKeyboardEvent event;
    RpcChatpad* chatpad;
    FuriString* macros;
};

void rpc_keyboard_register(void) {
    RpcKeyboard* keyboard = malloc(sizeof(RpcKeyboard));
    keyboard->event = (RpcKeyboardEvent){
        .data.newline_enabled = true,
        .data.length = 0,
        .data.mutex = furi_mutex_alloc(FuriMutexTypeRecursive)};
    keyboard->event_pubsub = furi_pubsub_alloc();
    keyboard->chatpad = NULL;
    keyboard->macros = furi_string_alloc();
    keyboard->funcs.major = 1;
    keyboard->funcs.minor = 6;
    keyboard->funcs.fn_get_pubsub = rpc_keyboard_get_pubsub;
    keyboard->funcs.fn_newline_enable = rpc_keyboard_newline_enable;
    keyboard->funcs.fn_publish_char = rpc_keyboard_publish_char;
    keyboard->funcs.fn_publish_macro = rpc_keyboard_publish_macro;
    keyboard->funcs.fn_get_macro = rpc_keyboard_get_macro;
    keyboard->funcs.fn_set_macro = rpc_keyboard_set_macro;
    keyboard->funcs.fn_chatpad_start = rpc_keyboard_chatpad_start;
    keyboard->funcs.fn_chatpad_stop = rpc_keyboard_chatpad_stop;
    keyboard->funcs.fn_chatpad_status = rpc_keyboard_chatpad_status;
    furi_record_create(RECORD_RPC_KEYBOARD, keyboard);
}

void rpc_keyboard_release(void) {
    RpcKeyboard* rpc_keyboard = furi_record_open(RECORD_RPC_KEYBOARD);
    if(!rpc_keyboard) {
        FURI_LOG_E(TAG, "rpc keyboard is NULL");
        return;
    }
    furi_pubsub_free(rpc_keyboard->event_pubsub);
    furi_mutex_free(rpc_keyboard->event.data.mutex);
    if(rpc_keyboard->chatpad) {
        rpc_chatpad_free(rpc_keyboard->chatpad);
    }
    furi_string_free(rpc_keyboard->macros);
    furi_record_destroy(RECORD_RPC_KEYBOARD);
}

FuriPubSub* rpc_keyboard_get_pubsub(RpcKeyboard* rpc_keyboard) {
    if(rpc_keyboard) {
        return rpc_keyboard->event_pubsub;
    } else {
        FURI_LOG_E(TAG, "rpc keyboard is NULL");
        return NULL;
    }
}

void rpc_keyboard_newline_enable(RpcKeyboard* rpc_keyboard, bool enable) {
    if(rpc_keyboard) {
        rpc_keyboard->event.data.newline_enabled = enable;
    } else {
        FURI_LOG_E(TAG, "rpc keyboard is NULL");
    }
}

/**
 * @brief Internal API - publishes a remote keyboard event.
 * @param[in] rpc_keyboard pointer to the RECORD_RPC_KEYBOARD.
 * @param[in] bytes the data to publish.
 * @param[in] buffer_size the size of the bytes.
 * @param[in] type the type of the event.
 */
static void rpc_keyboard_publish(
    RpcKeyboard* rpc_keyboard,
    uint8_t* bytes,
    uint32_t buffer_size,
    RpcKeyboardEventType type) {
    if(!rpc_keyboard) {
        FURI_LOG_E(TAG, "rpc keyboard is NULL");
        return;
    }
    furi_mutex_acquire(rpc_keyboard->event.data.mutex, FuriWaitForever);
    if(buffer_size > sizeof(rpc_keyboard->event.data.message) - 1) {
        buffer_size = sizeof(rpc_keyboard->event.data.message) - 1;
    }
    strncpy(rpc_keyboard->event.data.message, (const char*)bytes, buffer_size);
    rpc_keyboard->event.data.length = buffer_size;
    rpc_keyboard->event.data.message[rpc_keyboard->event.data.length] = '\0';
    rpc_keyboard->event.type = type;
    furi_mutex_release(rpc_keyboard->event.data.mutex);

    if(rpc_keyboard->event_pubsub) {
        furi_pubsub_publish(rpc_keyboard->event_pubsub, &rpc_keyboard->event);
    }
}

void rpc_keyboard_publish_text(RpcKeyboard* rpc_keyboard, uint8_t* bytes, uint32_t buffer_size) {
    rpc_keyboard_publish(rpc_keyboard, bytes, buffer_size, RpcKeyboardEventTypeTextEntered);
}

void rpc_keyboard_publish_char(RpcKeyboard* rpc_keyboard, char character) {
    rpc_keyboard_publish(rpc_keyboard, (uint8_t*)&character, 1, RpcKeyboardEventTypeCharEntered);
}

void rpc_keyboard_publish_macro(RpcKeyboard* rpc_keyboard, char macro) {
    if(isupper(macro)) { // our macro table is lowercase
        macro = tolower(macro);
    }
    char* macro_text = rpc_keyboard_get_macro(rpc_keyboard, macro);
    if(macro_text == NULL) {
        macro_text = malloc(sizeof(char) * 2);
        macro_text[0] = macro; // send the macro key if no macro is set
        macro_text[1] = '\0';
    }

    rpc_keyboard_publish(
        rpc_keyboard, (uint8_t*)macro_text, strlen(macro_text), RpcKeyboardEventTypeMacroEntered);
    free(macro_text);
}

/**
 * @brief Internal API - searches for the start of a macro in the macro string.
 * @param[in] rpc_keyboard pointer to the RECORD_RPC_KEYBOARD.
 * @param[in] macro the macro key to search for.
 * @return size_t the index of the start of the macro in the string.
 */
static size_t rpc_keyboard_get_macro_start_index(RpcKeyboard* rpc_keyboard, char macro) {
    furi_check(rpc_keyboard);
    char macro_start[3] = {'\x80', macro, '\0'};
    return furi_string_search_str(rpc_keyboard->macros, macro_start, 0);
}
static const char macro_end[2] = {'\x81', '\0'};

char* rpc_keyboard_get_macro(RpcKeyboard* rpc_keyboard, char macro) {
    if(!rpc_keyboard) {
        FURI_LOG_E(TAG, "rpc keyboard is NULL");
        return NULL;
    }
    char* macro_text = NULL;
    size_t index_start = rpc_keyboard_get_macro_start_index(rpc_keyboard, macro);
    if(index_start != FURI_STRING_FAILURE) {
        size_t index_end = furi_string_search_str(rpc_keyboard->macros, macro_end, index_start);
        if(index_end != FURI_STRING_FAILURE) {
            size_t len = index_end - index_start - 2;
            macro_text = malloc(sizeof(char) * (len + 1));
            strncpy(macro_text, furi_string_get_cstr(rpc_keyboard->macros) + index_start + 2, len);
            macro_text[len] = '\0';
        }
    }
    FURI_LOG_D(
        TAG,
        "Get Macro %c: %d %s >%s",
        macro,
        index_start,
        macro_text ? macro_text : "NULL",
        furi_string_get_cstr(rpc_keyboard->macros));
    return macro_text;
}

void rpc_keyboard_set_macro(RpcKeyboard* rpc_keyboard, char macro, char* value) {
    if(!rpc_keyboard) {
        FURI_LOG_E(TAG, "rpc keyboard is NULL");
        return;
    }
    size_t index_start = rpc_keyboard_get_macro_start_index(rpc_keyboard, macro);
    if(index_start != FURI_STRING_FAILURE) {
        size_t index_end = furi_string_search_str(rpc_keyboard->macros, macro_end, index_start);
        if(index_end != FURI_STRING_FAILURE) {
            size_t len = index_end - index_start + 1;
            furi_string_replace_at(rpc_keyboard->macros, index_start, len, "");
        }
    }
    if(strlen(value) != 0) {
        furi_string_cat_printf(rpc_keyboard->macros, "\x80%c%s\x81", macro, value);
    }
    FURI_LOG_D(
        TAG,
        "Set Macro %c: %d %s >%s",
        macro,
        index_start,
        value ? value : "NULL",
        furi_string_get_cstr(rpc_keyboard->macros));
}

void rpc_keyboard_chatpad_start(RpcKeyboard* rpc_keyboard) {
    if(!rpc_keyboard) {
        FURI_LOG_E(TAG, "rpc keyboard is NULL");
        return;
    }
    if(rpc_keyboard->chatpad) {
        rpc_keyboard_chatpad_stop(rpc_keyboard);
    }
    rpc_keyboard->chatpad = rpc_chatpad_alloc();
}

void rpc_keyboard_chatpad_stop(RpcKeyboard* rpc_keyboard) {
    if(!rpc_keyboard) {
        FURI_LOG_E(TAG, "rpc keyboard is NULL");
        return;
    }
    rpc_chatpad_free(rpc_keyboard->chatpad);
    rpc_keyboard->chatpad = NULL;
}

RpcKeyboardChatpadStatus rpc_keyboard_chatpad_status(RpcKeyboard* rpc_keyboard) {
    if(!rpc_keyboard) {
        FURI_LOG_E(TAG, "rpc keyboard is NULL");
        return RpcKeyboardChatpadStatusError;
    }
    if(rpc_keyboard->chatpad == NULL) {
        return RpcKeyboardChatpadStatusStopped;
    }
    if(rpc_chatpad_ready(rpc_keyboard->chatpad)) {
        return RpcKeyboardChatpadStatusReady;
    } else {
        return RpcKeyboardChatpadStatusStarted;
    }
}
