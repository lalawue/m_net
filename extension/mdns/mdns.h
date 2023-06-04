/* 
 * Copyright (c) 2020 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _MDNS_UTILS_H
#define _MDNS_UTILS_H

/* -- build query content
 * buf: buffer for store DNS query packet content, must have size == 1024 bytes
 * qid: DNS query id, make sure > 0
 * domain: '0' terminated string
 * --
 * return: valid query_size > 0 for ok, <= 0 for error and indicate error type
 */
int mdns_query_build(uint8_t *buf, unsigned short qid, const char *domain);

/* -- fetch query id for later input query_size
 * buf: response buffer
 * content_len: valid content length
 * --
 * return: 0 for error, or qid from response
 */
int mdns_response_fetch_qid(const uint8_t *buf, int content_len);

/* -- 
 * buf: response buffer
 * content_len: valid content length
 * query_size: query_size for qid fetched before
 * domain: '0' terminated string for compare
 * out_ipv4: 4 byte buffer for output ipv4
 * --
 * return: 1 for ok, <= 0 for error and indicate error type
 */
int mdns_response_parse(uint8_t *buf,
                        int content_len,
                        int query_size,
                        const char *domain,
                        uint8_t *out_ipv4);

#endif
