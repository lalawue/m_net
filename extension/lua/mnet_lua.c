/*
 * Copyright (c) 2017 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "mnet_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _RETURN_NONE_NIL(LUA, ARGN) do {             \
      if ( lua_isnoneornil(LUA, ARGN) ) {            \
         return 0;                                   \
      }                                              \
   } while (0)

typedef struct {
   chann_t *n;                  /* chann pointer */
   int index;                   /* chann outer index */
   lua_State *L;                /* lua_State */
} lua_chann_t;

static unsigned char *g_buf;

static lua_chann_t*
_create_lua_chann(lua_State *L, chann_t *n) {
   lua_chann_t *lc = (lua_chann_t*)malloc(sizeof(lua_chann_t));
   lc->n = n;
   lc->index = 0;
   lc->L = L;
   return lc;
}

static void
_destroy_lua_chann(lua_chann_t *lc) {
   free(lc);
}

/* check types in lua stack */
static int
_check_type(lua_State *L, int *types, int count, int nline) {
   int i = 0;
   for (i=0; i<count; i++) {
      if (lua_type(L, i+1) != types[i]) {
         luaL_error(L,
                 "invalid type idx %d: wanted %d, actual %d, in line %d\n",
                 i,
                 types[i],
                 lua_type(L, i+1),
                 nline);
         return 0;
      }
   }
   return 1;
}

/* interface */

static int
_mnet_init(lua_State *L) {
   int ret = mnet_init(1);      /* pull style API */
   lua_pushboolean(L, ret);
   if (ret && !g_buf) {
      g_buf = calloc(1, MNET_BUF_SIZE);
   }
   return 1;
}

static int
_mnet_fini(lua_State *L) {
   mnet_fini();
   if (g_buf) {
      free(g_buf);
      g_buf = NULL;
   }
   return 0;
}

static int
_mnet_version(lua_State *L) {
   lua_pushinteger(L, mnet_version());
   return 1;
}


/* input milliseconds */
static int
_mnet_poll(lua_State *L) {
   _RETURN_NONE_NIL(L, 1);

   int milliseconds = luaL_checkinteger(L, 1);
   poll_result_t *result = mnet_poll(milliseconds);
   lua_pushlightuserdata(L, result);
   return 1;
}

static int
_mnet_result_count(lua_State *L) {
   if ( !lua_islightuserdata(L, 1) ) {
      return 0;
   }

   poll_result_t *result = (poll_result_t *)lua_touserdata(L, 1);
   lua_pushinteger(L, result->chann_count);
   return 1;
}

static int
_mnet_result_next(lua_State *L) {
   if ( !lua_islightuserdata(L, 1) ) {
      return 0;
   }

   poll_result_t *result = (poll_result_t *)lua_touserdata(L, 1);
   chann_msg_t *msg = mnet_result_next(result);
   if (msg) {
      lua_pushlightuserdata(L, mnet_chann_get_opaque(msg->n));
      lua_pushinteger(L, msg->event);
      if (msg->r) {
         lua_chann_t *lc = _create_lua_chann(L, msg->r);
         mnet_chann_set_opaque(msg->r, lc);
         lua_pushlightuserdata(L, lc);
      } else {
         lua_pushnil(L);
      }
      return 3;
   } else {
      return 0;
   }
}

static int
_mnet_current(lua_State *L) {
   lua_pushinteger(L, mnet_current());
   return 1;
}

static int
_mnet_parse_ipport(lua_State *L) {
   _RETURN_NONE_NIL(L, 1);

   const char *ipport = lua_tostring(L, 1);
   chann_addr_t addr;
   if (mnet_parse_ipport(ipport, &addr)) {
      lua_pushstring(L, addr.ip);
      lua_pushinteger(L, addr.port);
      return 2;
   }
   return 0;
}

static int
_mnet_resolve(lua_State *L) {
   int types[3] = { LUA_TSTRING, LUA_TNUMBER, LUA_TNUMBER};
   if ( !_check_type(L, types, 1, __LINE__) ) {
      return 0;
   }

   const char *host = lua_tostring(L, 1);
   int port = (int)lua_tointeger(L, 2);
   int ctype = (int)lua_tointeger(L, 3);
   chann_addr_t addr;
   if (mnet_resolve(host, port, ctype, &addr)) {
      lua_pushstring(L, addr.ip);
      lua_pushinteger(L, addr.port);
      return 2;
   }
   return 0;
}

/* open chann with type */
static int
_chann_open(lua_State *L) {
   _RETURN_NONE_NIL(L, 1);
   chann_t *n = mnet_chann_open(lua_tointeger(L, 1));
   if ( n ) {
      lua_chann_t *lc = _create_lua_chann(L, n);
      mnet_chann_set_opaque(n, lc);
      lua_pushlightuserdata(L, lc);
      return 1;
   }
   return 0;
}

/* close chann */
static int
_chann_close(lua_State *L) {
   if ( ! lua_islightuserdata(L, 1) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   if ( lc ) {
      mnet_chann_close(lc->n);
      _destroy_lua_chann(lc);
   }
   return 0;
}

static int
_chann_fd(lua_State *L) {
   if ( ! lua_islightuserdata(L, 1) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   if ( lc ) {
      lua_pushinteger(L, mnet_chann_fd(lc->n));
   } else {
      lua_pushinteger(L, 0);
   }
   return 1;
}

static int
_chann_type(lua_State *L) {
   if ( ! lua_islightuserdata(L, 1) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   if (lc) {
      lua_pushinteger(L, mnet_chann_type(lc->n));
   } else {
      lua_pushinteger(L, 0);
   }
   return 1;
}


/* .listen(n, "127.0.0.1", 8080, 5) */
static int
_chann_listen(lua_State *L) {
   int types[4] = { LUA_TLIGHTUSERDATA, LUA_TSTRING, LUA_TNUMBER, LUA_TNUMBER };
   if ( !_check_type(L, types, 4, __LINE__) ) {
      fprintf(stderr, "invalid listen params !\n");
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   const char *ip = lua_tostring(L, 2);
   int port = (int)lua_tointeger(L, 3);
   int backlog = (int)lua_tointeger(L, 4);
   if (backlog <= 0) {
      backlog = 1;
   }
   int ret = mnet_chann_listen(lc->n, ip, port, backlog);
   lua_pushboolean(L, ret);
   return 1;
}


/* connect(n, "127.0.0.1", 8080) */
static int
_chann_connect(lua_State *L) {
   int types[3] = { LUA_TLIGHTUSERDATA, LUA_TSTRING, LUA_TNUMBER };
   if ( !_check_type(L, types, 3, __LINE__) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   const char *ip = lua_tostring(L, 2);
   int port = lua_tointeger(L, 3);
   int ret = mnet_chann_connect(lc->n, ip, port);
   lua_pushboolean(L, ret);
   return 1;
}

static int
_chann_disconnect(lua_State *L) {
   if ( !lua_islightuserdata(L, 1) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   mnet_chann_disconnect(lc->n);
   return 0;
}

static int
_chann_set_index(lua_State *L) {
   int types[2] = { LUA_TLIGHTUSERDATA, LUA_TNUMBER };
   if ( !_check_type(L, types, 2, __LINE__) ) {
      return 0;
   }
   lua_chann_t *lc = (lua_chann_t *)lua_touserdata(L, 1);
   lc->index = (int)lua_tointeger(L, 2);
   return 0;
}

static int
_chann_get_index(lua_State *L) {
   if ( !lua_islightuserdata(L, 1) ) {
      lua_pushinteger(L, 0);
      return 1;
   }
   lua_chann_t *lc = (lua_chann_t *)lua_touserdata(L, 1);
   lua_pushinteger(L, lc->index);
   return 1;
}

/* local buf = .chann_recv(n) */
static int
_chann_recv(lua_State *L) {
   if ( !lua_islightuserdata(L, 1) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   if ( lc ) {
      int ret = mnet_chann_recv(lc->n, g_buf, MNET_BUF_SIZE)->ret;
      if (ret >= 0) {
         lua_pushlstring(L, (const char*)g_buf, ret);
         return 1;
      }
   }
   return 0;
}

/* .chann_send( buf ) */
static int
_chann_send(lua_State *L) {
   int types[2] = { LUA_TLIGHTUSERDATA, LUA_TSTRING };
   if ( !_check_type(L, types, 2, __LINE__) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   size_t buf_len = 0;
   const char *buf = lua_tolstring(L, 2, &buf_len);
   if (lc && buf_len > 0) {
      int ret = mnet_chann_send(lc->n, (void*)buf, buf_len)->ret;
      lua_pushinteger(L, ret);
      return 1;
   }
   return 0;
}

static int
_chann_state(lua_State *L) {
   if ( !lua_islightuserdata(L, 1) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   lua_pushinteger(L, mnet_chann_state(lc->n));
   return 1;
}

static int
_chann_bytes(lua_State *L) {
   int types[2] = { LUA_TLIGHTUSERDATA, LUA_TNUMBER };
   if ( !_check_type(L, types, 2, __LINE__) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   int be_send = lua_tointeger(L, 2);
   long long ret = mnet_chann_bytes(lc->n, be_send);
   lua_pushinteger(L, ret);
   return 1;
}


static int
_chann_cached(lua_State *L) {
   if ( !lua_islightuserdata(L, 1) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   int ret = mnet_chann_cached(lc->n);
   lua_pushinteger(L, ret);
   return 1;
}

static int
_chann_active_event(lua_State *L) {
   int types[3] = { LUA_TLIGHTUSERDATA, LUA_TNUMBER, LUA_TNUMBER };
   if ( !_check_type(L, types, 3, __LINE__) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   chann_event_t event = lua_tointeger(L, 2);
   int value = lua_tointeger(L, 3);
   mnet_chann_active_event(lc->n, event, value);
   return 0;
}

/* .chann_set_bufsize(n, 64*1024) */
static int
_chann_set_bufsize(lua_State *L) {
   int types[2] = { LUA_TLIGHTUSERDATA, LUA_TNUMBER };
   if ( !_check_type(L, types, 2, __LINE__) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   int bufsize = (int)lua_tointeger(L, 2);
   int ret = mnet_chann_socket_set_bufsize(lc->n, bufsize);
   lua_pushboolean(L, ret);
   return 1;
}

static int
_chann_addr(lua_State *L) {
   if ( !lua_islightuserdata(L, 1) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   chann_addr_t addr;
   mnet_chann_socket_addr(lc->n, &addr);
   lua_pushstring(L, addr.ip);
   lua_pushinteger(L, addr.port);
   return 2;
}

/* get chann count */
static int
_chann_count(lua_State *L) {
   int ret = mnet_report(0);
   lua_pushinteger(L, ret);
   return 1;
}

static const luaL_Reg mnet_lib[] = {
    { "init", _mnet_init },
    { "fini", _mnet_fini },
    { "version", _mnet_version},

    { "poll", _mnet_poll },
    { "result_count", _mnet_result_count },
    { "result_next", _mnet_result_next },

    { "chann_open", _chann_open },
    { "chann_close", _chann_close },

    { "chann_fd", _chann_fd },
    { "chann_type", _chann_type },

    { "chann_listen", _chann_listen },
    { "chann_connect", _chann_connect },
    { "chann_disconnect", _chann_disconnect },

    { "chann_set_index", _chann_set_index},
    { "chann_get_index", _chann_get_index},

    { "chann_active_event", _chann_active_event },

    { "chann_recv", _chann_recv},
    { "chann_send", _chann_send },

    { "chann_cached", _chann_cached },

    { "chann_state", _chann_state },
    { "chann_bytes", _chann_bytes },

    { "chann_count", _chann_count },

    { "chann_set_bufsize", _chann_set_bufsize },
    { "chann_addr", _chann_addr },

    { "current", _mnet_current },
    { "parse_ipport", _mnet_parse_ipport },
    { "resolve", _mnet_resolve},

    { NULL, NULL }
};

LUALIB_API int
luaopen_mnet(lua_State *L) {
   luaL_newlib(L, mnet_lib);
   return 1;
}
