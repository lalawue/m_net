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
   lua_State *L;                /* lua_State */
   unsigned char *buf;
   int buf_len;
} lua_chann_t;

#define LUA_CHANN_BUF_LEN (64*1024)
static unsigned char *g_buf;

static lua_chann_t*
_create_lua_chann(lua_State *L, chann_t *n) {
   lua_chann_t *lc = (lua_chann_t*)malloc(sizeof(lua_chann_t));
   lc->n = n;
   lc->L = L;
   lc->buf = g_buf;
   lc->buf_len = LUA_CHANN_BUF_LEN;
   return lc;
}

static void
_destroy_lua_chann(lua_chann_t *lc) {
   free(lc);
}

/* check types in lua stack */
static int
_check_type(lua_State *L, int *types, int count) {
   for (int i=0; i<count; i++) {
      if (lua_type(L, i+1) != types[i]) {
         fprintf(stderr, "invalid type idx %d: %d, %d\n", i, lua_type(L, i+1), types[i]);
         return 0;
      }
   }
   return 1;
}

static const char*
_event_msg(chann_event_t event) {
   switch (event) {
      case CHANN_EVENT_RECV: return "recv";
      case CHANN_EVENT_SEND: return "send";
      case CHANN_EVENT_ACCEPT:  return "accept";
      case CHANN_EVENT_CONNECTED: return "connected";
      case CHANN_EVENT_DISCONNECT: return "disconnect";
      default: return "invalid";
   }
}

static int
_msg_event(const char *emsg) {
   static const char *events[CHANN_EVENT_DISCONNECT + 1] = {
      "invalid", "recv", "send", "accept", "connected", "disconnect",
   };
   for (int i=0; i<(CHANN_EVENT_DISCONNECT+1); i++) {
      if (strcmp(emsg, events[i]) == 0) {
         return i;
      }
   }
   return 0;
}

static int
_mnet_init(lua_State *L) {
   int ret = mnet_init();
   lua_pushboolean(L, ret);
   if (ret && !g_buf) {
      g_buf = malloc(LUA_CHANN_BUF_LEN);
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


/* input microseconds */
static int
_mnet_poll(lua_State *L) {
   _RETURN_NONE_NIL(L, 1);

   int microseconds = luaL_checkinteger(L, 1);
   int ret = mnet_poll(microseconds);

   lua_pushinteger(L, ret);
   return 1;
}

/* get chann count */
static int
_chann_count(lua_State *L) {
   int ret = mnet_report(0);
   lua_pushinteger(L, ret);
   return 1;
}


/* c level event callbacl function */
static void
_chann_msg_cb(chann_msg_t *msg) {
   lua_chann_t *lc = msg->opaque;

   lua_settop(lc->L, 0);
   lua_getglobal(lc->L, "MNetChannDispatchEvent");

   if (lua_gettop(lc->L)==1 && lua_isfunction(lc->L, -1)) {

      const char *emsg = _event_msg(msg->event);

      lua_pushstring(lc->L, emsg);
      lua_pushlightuserdata(lc->L, lc);
      if (msg->r) {
         lua_chann_t *rc = _create_lua_chann(lc->L, msg->r);
         lua_pushlightuserdata(lc->L, rc);
         mnet_chann_set_cb(msg->r, _chann_msg_cb, rc);
      } else {
         lua_pushnil(lc->L);
      }

      if (lua_pcall(lc->L, 3, 0, 0) != 0) {
         fprintf(stderr, "%s\n", "fail to pcall");
         luaL_error (lc->L, "%s\n", "fail to pcall !");
      }
   } else {
      fprintf(stderr, "%s\n", "fail to pcall 2");      
   }
}


/* open chann with type */
static int
_chann_open(lua_State *L) {
   _RETURN_NONE_NIL(L, 1);

   const char *s = luaL_checkstring(L, 1);
   chann_type_t t = CHANN_TYPE_STREAM;

   if (strcmp(s, "tcp") == 0) {
      t = CHANN_TYPE_STREAM;
   } else if (strcmp(s, "udp") == 0) {
      t = CHANN_TYPE_DGRAM;
   } else if (strcmp(s, "broadcast") == 0) {
      t = CHANN_TYPE_BROADCAST;
   }

   chann_t *n = mnet_chann_open(t);
   if ( n ) {
      lua_chann_t *lc = _create_lua_chann(L, n);
      lua_pushlightuserdata(L, lc);
      mnet_chann_set_cb(n, _chann_msg_cb, lc);
   } else {
      lua_pushnil(L);
   }

   return 1;
}

/* close chann */
static int
_chann_close(lua_State *L) {
   if ( ! lua_isuserdata(L, 1) ) {
      return 0;
   }
   
   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   if ( lc ) {
      mnet_chann_close(lc->n);
      _destroy_lua_chann(lc);
   }
   return 0;
}


/* .listen(n, "127.0.0.1:8080", 5) */
static int
_chann_listen(lua_State *L) {
   int types[3] = { LUA_TLIGHTUSERDATA, LUA_TSTRING, LUA_TNUMBER };

   if ( !_check_type(L, types, 3) ) {
      fprintf(stderr, "invalid listen params !\n");
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   const char *ipport = lua_tostring(L, 2);
   int backlog = (int)lua_tointeger(L, 3);

   chann_addr_t addr;
   mnet_parse_ipport(ipport, &addr);

   int ret = mnet_chann_listen(lc->n, addr.ip, addr.port, backlog);
   lua_pushboolean(L, ret);
   return 1;
}


/* connect(n, "127.0.0.1:8080") */
static int
_chann_connect(lua_State *L) {
   int types[2] = { LUA_TLIGHTUSERDATA, LUA_TSTRING };
   if ( !_check_type(L, types, 2) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   const char *ipport = lua_tostring(L, 2);
   
   chann_addr_t addr;
   mnet_parse_ipport(ipport, &addr);

   int ret = mnet_chann_connect(lc->n, addr.ip, addr.port);
   lua_pushboolean(L, ret);
   return 1;
}

static int
_chann_disconnect(lua_State *L) {
   if ( !lua_isuserdata(L, 1) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   mnet_chann_disconnect(lc->n);
   return 0;
}

/* local buf = .chann_recv(n) */
static int
_chann_recv(lua_State *L) {
   if ( !lua_isuserdata(L, 1) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   if ( lc ) {
      int ret = mnet_chann_recv(lc->n, lc->buf, lc->buf_len);
      if (ret >= 0) {
         lua_pushlstring(L, (const char*)lc->buf, ret);
      } else {
         lua_pushnil(L);
      }
      return 1;
   }
   return 0;
}

/* .chann_send( buf ) */
static int
_chann_send(lua_State *L) {
   int types[2] = { LUA_TLIGHTUSERDATA, LUA_TSTRING };
   if ( !_check_type(L, types, 2) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   size_t buf_len = 0;
   const char *buf = lua_tolstring(L, 2, &buf_len);
   if (lc && buf_len>0) {
      int ret = mnet_chann_send(lc->n, (void*)buf, buf_len);
      lua_pushinteger(L, ret);
      return 1;
   }
   return 0;
}


/* .chann_set_bufsize(n, 64*1024) */
static int
_chann_set_bufsize(lua_State *L) {
   int types[2] = { LUA_TLIGHTUSERDATA, LUA_TNUMBER };
   if ( !_check_type(L, types, 2) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   int bufsize = (int)lua_tointeger(L, 2);
   int ret = mnet_chann_set_bufsize(lc->n, bufsize);
   lua_pushboolean(L, ret);
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

/* "127.0.0.1:8080" */
static int
_chann_addr(lua_State *L) {
   if ( !lua_islightuserdata(L, 1) ) {
      return 0;
   }
   
   chann_addr_t addr;
   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   if ( mnet_chann_addr(lc->n, &addr) ) {
      char buf[32] = {0};
      snprintf(buf, 32, "%s:%d", addr.ip, addr.port);
      lua_pushstring(L, buf);
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
   int ret = mnet_chann_state(lc->n);
   
   const char *state = "closed";
   switch (ret) {
      case CHANN_STATE_DISCONNECT: state = "disconnect"; break;
      case CHANN_STATE_CONNECTING: state = "connecting"; break;
      case CHANN_STATE_CONNECTED: state = "connected"; break;
      case CHANN_STATE_LISTENING: state = "listening"; break;
      default: state = "closed"; break;
   }
   lua_pushstring(L, state);
   return 1;
}

static int
_chann_bytes(lua_State *L) {
   int types[2] = { LUA_TLIGHTUSERDATA, LUA_TBOOLEAN };
   if ( !_check_type(L, types, 2) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   int be_send = lua_toboolean(L, 2);
   long long ret = mnet_chann_bytes(lc->n, be_send);
   lua_pushinteger(L, ret);
   return 1;
}

static int
_chann_active_event(lua_State *L) {
   int types[3] = { LUA_TLIGHTUSERDATA, LUA_TSTRING, LUA_TBOOLEAN };
   if ( !_check_type(L, types, 3) ) {
      return 0;
   }

   lua_chann_t *lc = (lua_chann_t*)lua_touserdata(L, 1);
   const char *emsg = lua_tostring(L, 2);
   int active = lua_toboolean(L, 3);
   mnet_chann_active_event(lc->n, _msg_event(emsg), active);
   return 0;
}

static const luaL_Reg mnet_core_lib[] = {
    { "init", _mnet_init },
    { "fini", _mnet_fini },

    { "poll", _mnet_poll },
    { "chann_count", _chann_count },

    { "chann_open", _chann_open },
    { "chann_close", _chann_close },

    { "chann_listen", _chann_listen },
    { "chann_connect", _chann_connect },
    { "chann_disconnect", _chann_disconnect },

    { "chann_active_event", _chann_active_event },

    { "chann_recv", _chann_recv},
    { "chann_send", _chann_send },

    { "chann_set_bufsize", _chann_set_bufsize },
    { "chann_cached", _chann_cached },

    { "chann_addr", _chann_addr },

    { "chann_state", _chann_state },
    { "chann_bytes", _chann_bytes },

    { NULL, NULL }
};

LUALIB_API int
luaopen_mnet_core(lua_State *L) {
   luaL_checkversion(L);
   luaL_newlib(L, mnet_core_lib);
   return 1;
}
