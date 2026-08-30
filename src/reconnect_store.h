/*
 reconnect_store.h
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#ifndef DC_RECONNECT_STORE_H
#define DC_RECONNECT_STORE_H

#include <stdint.h>

#include "types.h"

/*
 On-disk half of reconnect-with-stack (#112).

 The in-memory token in the client covers a dropped socket, but not the client
 exiting -- a crash, or the player closing it after a drop and starting it
 again. Keeping the token in a small file covers those too, and it is still the
 SESSION token, not an identity: scoped to one server and one nick, worthless
 to any other server, and garbage once the server's grace window closes.

 The file is a bearer credential, so it is written 0600 via O_EXCL through a
 pid-unique temp name -- several clients on one machine must not race on it the
 way a fixed temp path would. Failures are silent by design: losing the token
 costs a reclaim, never a join.
*/

/* Fills out with the stored token for this server+nick, or all zeros when there
   is none, the file is unreadable, or it is the wrong size. */
void reconnect_store_load(const char *host, uint16_t port, const char *nick,
                          unsigned char out[RECONNECT_TOKEN_LEN]);

/* Replaces the stored token. An all-zero token removes the file instead. */
void reconnect_store_save(const char *host, uint16_t port, const char *nick,
                          const unsigned char token[RECONNECT_TOKEN_LEN]);

#endif
