#pragma once

#include <core/mutex.h>
#include <core/common_defines.h>
#include <core/pubsub.h>
#include <rpc/rpc_keyboard.h>

typedef struct RpcChatpad RpcChatpad;

/**
 * @brief Allocates a chatpad.
 * @return RpcChatpad* pointer to the chatpad.
 */
RpcChatpad* rpc_chatpad_alloc(void);

/**
 * @brief Frees the chatpad.
 * @param[in] chatpad pointer to the chatpad.
 */
void rpc_chatpad_free(RpcChatpad* chatpad);

/**
 * @brief determines if chatpad is ready for use.
 * @param[in] chatpad pointer to the chatpad.
 * @return true if the chatpad is responding to data.
 */
bool rpc_chatpad_ready(RpcChatpad* chatpad);
