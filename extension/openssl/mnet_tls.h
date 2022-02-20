/*
 * Copyright (c) 2022 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef MNET_TLS_H
#define MNET_TLS_H

#include "mnet_core.h"
#include <openssl/ssl.h>

extern chann_type_t const CHANN_TYPE_TLS;

/* config tls */
int mnet_tls_config(SSL_CTX *ctx);

#endif