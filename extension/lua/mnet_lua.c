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

#define _RETURN_NONE_NIL(LUA, ARGN) do {             \
      if ( lua_isnoneornil(LUA, ARGN) ) {            \
         return 0;                                   \
      }                                              \
   } while (0)

static int
_check_type(lua_State *L, int *types, int count) {
   for (int i=0; i<count; i++) {
      if (lua_type(L, i+1) != types[i]) {
         return 0;
      }
   }
   return 1;
}

static int
_mnet_init(lua_State *L) {
   int ret = mnet_init();
   lua_pushboolean(L, ret);
   return 1;
}

static int
_mnet_fini(lua_State *L) {
   mnet_fini();
   return 0;
}

static int
_mnet_poll(lua_State *L) {
   _RETURN_NONE_NIL(L, 1);

   int microseconds = luaL_checkint(L, 1);
   int ret = mnet_poll(microseconds);

   lua_pushinteger(L, ret);
   return 1;
}

static int
_chann_count(lua_State *L) {
   int ret = mnet_report(0);
   lua_pushinteger(L, ret);
   return 1;
}

static int
_chann_open(lua_State *L) {
   _RETURN_NONE_NIL(L, 1);

   int stype = luaL_checkint(L, 1);
   chann_t *n = mnet_chann_open((chann_type_t)stype);
   if ( n ) {
      lua_pushlightuserdata(L, n);
   } else {
      lua_pushnil(L);
   }

   return 1;
}

static int
_chann_close(lua_State *L) {
   if ( ! lua_islightuserdata(L, 1) ) {
      return 0;
   }
   
   chann_t *n = (chann_t*)lua_touserdata(L, 1);
   if ( n ) {
      mnet_chann_close(n);
   }
   return 0;
}

static int
_chann_listen(lua_State *L) {
   int types[4] = {
      LUA_TLIGHTUSERDATA,
      LUA_TSTRING,
      LUA_TNUMBER,
      LUA_TNUMBER,
   };

   if ( !_check_type(L, types, 4) ) {
      return 0;
   }

   chann_t *n = (chann_t*)lua_touserdata(L, 1);
   const char *host = lua_tostring(L, 2);
   int port = (int)lua_tointeger(L, 3);
   int backlog = (int)lua_tointeger(L, 4);

   int ret = mnet_chann_listen(n, host, port, backlog);
   lua_pushboolean(L, ret);
   return 1;
}

static int
_chann_connect(lua_State *L) {
   int types[3] = { LUA_TLIGHTUSERDATA, LUA_TSTRING,  LUA_TNUMBER };
   if ( !_check_type(L, types, 3) ) {
      return 0;
   }

   chann_t *n = (chann_t*)lua_touserdata(L, 1);
   const char *host = lua_tostring(L, 2);
   int port  = (int)lua_tointeger(L, 3);

   int ret = mnet_chann_connect(n, host, port);
   lua_pushboolean(L, ret);
   return 1;
}

static int
_chann_disconnect(lua_State *L) {
   if ( !lua_isuserdata(L, 1) ) {
      return 0;
   }

   chann_t *n = (chann_t*)lua_touserdata(L, 1);
   mnet_chann_disconnect(n);
   return 0;
}

static int
_chann_recv(lua_State *L) {
   if ( !lua_isuserdata(L, 1) ) {
      return 0;
   }

   chann_t *n = (chann_t*)lua_touserdata(L, 1);
   if ( n ) {
      unsigned char buf[64 * 1024] = { 0 };
      int ret = mnet_chann_recv(n, buf, 64 * 1024);
      if (ret >= 0) {
         lua_pushlstring(L, (const char*)buf, ret);
      } else {
         lua_pushnil(L);
      }
      return 1;
   }
   return 0;
}

static int
_chann_send(lua_State *L) {
   int types[2] = { LUA_TLIGHTUSERDATA, LUA_TSTRING };
   if ( !_check_type(L, types, 2) ) {
      return 0;
   }

   chann_t *n = (chann_t*)lua_touserdata(L, 1);
   size_t buf_len = 0;
   const char *buf = lua_tolstring(L, 2, &buf_len);
   if (n && buf_len>0) {
      int ret = mnet_chann_send(n, (void*)buf, buf_len);
      lua_pushinteger(L, ret);
      return 1;
   }
   return 0;
}

static int
_chann_set_bufsize(lua_State *L) {
   int types[2] = { LUA_TLIGHTUSERDATA, LUA_TNUMBER };
   if ( !_check_type(L, types, 2) ) {
      return 0;
   }

   chann_t *n = (chann_t*)lua_touserdata(L, 1);
   int bufsize = (int)lua_tointeger(L, 2);
   int ret = mnet_chann_set_bufsize(n, bufsize);
   lua_pushboolean(L, ret);
   return 1;
}

static int
_chann_cached(lua_State *L) {
   if ( !lua_islightuserdata(L, 1) ) {
      return 0;
   }

   chann_t *n = (chann_t*)lua_touserdata(L, 1);
   int ret = mnet_chann_cached(n);
   lua_pushinteger(L, ret);
   return 1;
}

static int
_chann_addr(lua_State *L) {
   if ( !lua_islightuserdata(L, 1) ) {
      return 0;
   }
   
   chann_addr_t addr;
   chann_t *n = (chann_t*)lua_touserdata(L, 1);
   if ( mnet_chann_addr(n, &addr) ) {
      char buf[32] = {0};
      snprintf(buf, 64, "%s:%d", addr.ip, addr.port);
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

   chann_t *n = (chann_t*)lua_touserdata(L, 1);
   int ret = mnet_chann_state(n);
   lua_pushinteger(L, ret);
   return 1;
}

static int
_chann_bytes(lua_State *L) {
   int types[2] = { LUA_TLIGHTUSERDATA, LUA_TBOOLEAN }
   if ( !_check_type(L, types, 2) ) {
      return 0;
   }

   chann_t *n = (chann_t*)lua_touserdata(L, 1);
   int be_send = lua_toboolean(L, 2);
   long long ret = mnet_chann_bytes(n, be_send);
   lua_pushinteger(L, ret);
   return 1;
}

/* static int */
/* _chann_set_cb(lua_State *L) { */
/*    int types[2] = { LUA_TLIGHTUSERDATA, LUA_TFUNCTION } */
/*    if ( !_check_type(L, types, 2) ) { */
/*       return 0; */
/*    } */
/* } */

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

    //{ "chann_set_cb", _chann_set_cb },

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
