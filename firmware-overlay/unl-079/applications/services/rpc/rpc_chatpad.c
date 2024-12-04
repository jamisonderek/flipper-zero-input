// baud rate and tx data from https://github.com/frequem/Chatpad

#include <api_lock.h>
#include <expansion/expansion.h>
#include <rpc/rpc_chatpad.h>
#include <rpc/rpc_keyboard.h>
#include <furi_hal_bus.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_vibro.h>

#define TAG "RpcChatpad"

struct RpcChatpad {
    FuriHalBus lpuart_bus;
    FuriHalSerialHandle* lpuart_handle;
    bool lpuart_already_opened;
    FuriStreamBuffer* rx_stream;
    FuriThread* worker_thread;
    FuriApiLock worker_thread_exit_lock;
    bool ready;
};

static uint8_t rpc_chatpad_keymap[7][7] = {
    {'7', '6', '5', '4', '3', '2', '1'},
    {'U', 'Y', 'T', 'R', 'E', 'W', 'Q'},
    {'J', 'H', 'G', 'F', 'D', 'S', 'A'},
    {'N', 'B', 'V', 'C', 'X', 'Z', '?'},
    {RPC_KEYBOARD_KEY_RIGHT, 'M', '.', ' ', RPC_KEYBOARD_KEY_LEFT, '?', '?'},
    {'?', ',', RPC_KEYBOARD_KEY_ENTER, 'P', '0', '9', '8'},
    {RPC_KEYBOARD_KEY_BACKSPACE, 'L', '?', '?', 'O', 'I', 'K'}};

static char orangeMap(uint8_t button) {
    switch(button) {
    case 'R':
        return '$';
    case 'P':
        return '=';
    case ',':
        return ';';
    case 'J':
        return '\"';
    case 'H':
        return '\\';
    case 'V':
        return '_';
    case 'B':
        return '+';
    default:
        return button;
    }
}

static char greenMap(uint8_t button) {
    switch(button) {
    case 'Q':
        return '!';
    case 'W':
        return '@';
    case 'R':
        return '#';
    case 'T':
        return '%';
    case 'Y':
        return '^';
    case 'U':
        return '&';
    case 'I':
        return '*';
    case 'O':
        return '(';
    case 'P':
        return ')';
    case 'A':
        return '~';
    case 'D':
        return '{';
    case 'F':
        return '}';
    case 'H':
        return '/';
    case 'J':
        return '\'';
    case 'K':
        return '[';
    case 'L':
        return ']';
    case ',':
        return ':';
    case 'Z':
        return '`';
    case 'V':
        return '-';
    case 'B':
        return '|';
    case 'N':
        return '<';
    case 'M':
        return '>';
    case '.':
        return '?';
    default:
        return button;
    }
}

static char rpc_chatpad_code_to_char(uint8_t code, uint8_t modifier, uint8_t caplock) {
    uint8_t row = code >> 4;
    uint8_t col = code & 0xF;
    uint8_t button = '\0';

    if(row > 0 && col > 0 && row <= 7 && col <= 7) {
        button = rpc_chatpad_keymap[row - 1][col - 1];
        if(modifier == 4) {
            button = orangeMap(button);
        } else if(modifier == 2) {
            button = greenMap(button);
        } else if((modifier & 1) == caplock && isupper(button)) {
            button = button + 32; // make lowercase
        }
    }

    return button;
}

static void rpc_chatpad_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    furi_check(context);
    RpcChatpad* chatpad = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(chatpad->rx_stream, (void*)&data, 1, FuriWaitForever);
    }
}

static bool rpc_chatpad_init(RpcChatpad* chatpad) {
    furi_check(chatpad);
    const uint8_t init_sequence[] = {0x87, 0x02, 0x8C, 0x1F, 0xCC};
    for(size_t retry_loop = 0; retry_loop < 5; retry_loop++) {
        furi_hal_serial_tx(chatpad->lpuart_handle, init_sequence, 5);
        uint8_t init_response[8];
        size_t bytes_read =
            furi_stream_buffer_receive(chatpad->rx_stream, &init_response, 8, 1000);
        if(bytes_read != 8) {
            FURI_LOG_E(TAG, "Failed to get response from initialize chatpad.");
            furi_delay_ms(500);
            continue;
        }
        furi_delay_ms(500);
        return true;
    }
    return false;
}

static void rpc_chatpad_heartbeat(RpcChatpad* chatpad) {
    const uint8_t heartbeat_sequence[] = {0x87, 0x02, 0x8C, 0x1B, 0xD0};
    furi_hal_serial_tx(chatpad->lpuart_handle, heartbeat_sequence, 5);
}

static int32_t rpc_chatpad_worker(void* context) {
    furi_check(context);
    RpcChatpad* chatpad = context;
    FURI_LOG_D(TAG, "Chatpad worker started.");
    rpc_chatpad_init(chatpad);

    uint32_t line = 0;
    bool show = true;
    uint8_t caplock = 0;
    while(api_lock_is_locked(chatpad->worker_thread_exit_lock)) {
        if(line++ % 100 == 0) {
            FURI_LOG_T(TAG, "Chatpad heartbeat.");
            rpc_chatpad_heartbeat(chatpad);
        }
        uint8_t data[8];
        size_t bytes_read = furi_stream_buffer_receive(chatpad->rx_stream, &data, 8, 20);
        if(bytes_read == 0) {
            continue;
        }
        if(bytes_read != 8) {
            FURI_LOG_T(TAG, "Invalid data size: %zu", bytes_read);
            continue;
        }

        if(data[0] != 0xB4 && data[0] != 0xA5) {
            FURI_LOG_D(
                TAG,
                "Invalid data: %02x %02x %02x %02x %02x %02x %02x %02x",
                data[0],
                data[1],
                data[2],
                data[3],
                data[4],
                data[5],
                data[6],
                data[7]);

            uint8_t sync_bytes = 0;
            for(size_t i = 1; i < 8; i++) {
                if(data[i] == 0xA5 || data[i] == 0xB4) {
                    sync_bytes = i;
                }
            }
            if(sync_bytes != 0) {
                bytes_read =
                    furi_stream_buffer_receive(chatpad->rx_stream, &data, sync_bytes, 100);
                if(bytes_read != sync_bytes) {
                    FURI_LOG_D(
                        TAG,
                        "Failed to sync data. attempted: %d actual: %d",
                        sync_bytes,
                        bytes_read);
                } else {
                    FURI_LOG_T(TAG, "Synced data.");
                }
            }

            continue;
        }

        if((data[0] + data[1] + data[2] + data[3] + data[4] + data[5] + data[6] + data[7]) % 256 !=
           0) {
            FURI_LOG_E(
                TAG,
                "Checksum failed. data: %02x %02x %02x %02x %02x %02x %02x %02x",
                data[0],
                data[1],
                data[2],
                data[3],
                data[4],
                data[5],
                data[6],
                data[7]);
            continue;
        }

        if(data[0] == 0xB4 && data[1] == 0xC5 && show) {
            uint8_t mod = data[3];
            uint8_t btn = data[4];
            if(mod != 0 || btn != 0) {
                char ch = rpc_chatpad_code_to_char(btn, mod, caplock);
                if(mod == 8 && btn != 0) {
                    // People button + other key.
                    FURI_LOG_T(TAG, "Char: %c Mod: %02x, Btn: %02x", ch, mod, btn);
                    RpcKeyboard* rpc_keyboard = furi_record_open(RECORD_RPC_KEYBOARD);
                    rpc_keyboard_publish_macro(rpc_keyboard, ch);
                    furi_record_close(RECORD_RPC_KEYBOARD);
                } else if(mod == 5) {
                    // Caps lock button pressed.
                    furi_hal_vibro_on(true);
                    caplock = (caplock == 0) ? 1 : 0;
                    furi_delay_ms(100);
                    furi_hal_vibro_on(false);
                } else if(ch != '\0') {
                    FURI_LOG_T(TAG, "Char: %c Mod: %02x, Btn: %02x", ch, mod, btn);
                    RpcKeyboard* rpc_keyboard = furi_record_open(RECORD_RPC_KEYBOARD);
                    rpc_keyboard_publish_char(rpc_keyboard, ch);
                    furi_record_close(RECORD_RPC_KEYBOARD);
                } else if(btn != 0) {
                    FURI_LOG_D(TAG, "Mod: %02x, Btn: %02x", mod, btn);
                }
            }
            show = false;
            line = 1;
        } else if(data[0] == 0xA5) {
            if(!chatpad->ready) {
                FURI_LOG_I(TAG, "Chatpad ready.");
                chatpad->ready = true;
            }
            show = true;
        }
    }

    FURI_LOG_D(TAG, "Chatpad worker exiting.");
    return 0;
}

RpcChatpad* rpc_chatpad_alloc(void) {
    RpcChatpad* chatpad = malloc(sizeof(RpcChatpad));
    const uint32_t lpuart_baud = 19200;
    chatpad->ready = false;
    chatpad->worker_thread_exit_lock = api_lock_alloc_locked();
    chatpad->lpuart_bus = FuriHalBusLPUART1;
    chatpad->lpuart_handle = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    chatpad->lpuart_already_opened = furi_hal_bus_is_enabled(chatpad->lpuart_bus);
    if(!chatpad->lpuart_already_opened) {
        furi_hal_serial_init(chatpad->lpuart_handle, lpuart_baud);
    } else {
        FURI_LOG_D(TAG, "LPUART already initialized.");
    }
    furi_hal_serial_set_br(chatpad->lpuart_handle, lpuart_baud);
    chatpad->rx_stream = furi_stream_buffer_alloc(32, 8);
    furi_hal_serial_async_rx_start(
        chatpad->lpuart_handle, rpc_chatpad_rx_callback, chatpad, false);
    chatpad->worker_thread =
        furi_thread_alloc_ex("rpc_chatpad_work", 1024, rpc_chatpad_worker, chatpad);
    furi_thread_start(chatpad->worker_thread);
    return chatpad;
}

void rpc_chatpad_free(RpcChatpad* chatpad) {
    furi_check(chatpad);
    chatpad->ready = false;
    furi_hal_serial_async_rx_stop(chatpad->lpuart_handle);
    api_lock_unlock(chatpad->worker_thread_exit_lock);
    furi_thread_join(chatpad->worker_thread);
    chatpad->worker_thread = NULL;
    FURI_LOG_D(TAG, "Chatpad freeing resources.");
    if(chatpad->lpuart_already_opened) {
        furi_hal_serial_deinit(chatpad->lpuart_handle);
    }
    furi_hal_serial_control_release(chatpad->lpuart_handle);
    chatpad->lpuart_handle = NULL;
    furi_stream_buffer_free(chatpad->rx_stream);
    chatpad->rx_stream = NULL;
    free(chatpad);
}

bool rpc_chatpad_ready(RpcChatpad* chatpad) {
    furi_check(chatpad);
    return chatpad->ready;
}
