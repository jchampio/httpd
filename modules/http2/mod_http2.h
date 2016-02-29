/* Copyright 2015 greenbytes GmbH (https://www.greenbytes.de)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __MOD_HTTP2_H__
#define __MOD_HTTP2_H__

#include "httpd.h"
#include "apr_optional.h"

/* Create a set of HTTP2_DECLARE(type), HTTP2_DECLARE_NONSTD(type) and
 * HTTP2_DECLARE_DATA with appropriate export and import tags for the platform
 */
#if !defined(WIN32)
#define HTTP2_DECLARE(type)            type
#define HTTP2_DECLARE_NONSTD(type)     type
#define HTTP2_DECLARE_DATA
#elif defined(HTTP2_DECLARE_STATIC)
#define HTTP2_DECLARE(type)            type __stdcall
#define HTTP2_DECLARE_NONSTD(type)     type
#define HTTP2_DECLARE_DATA
#elif defined(HTTP2_DECLARE_EXPORT)
#define HTTP2_DECLARE(type)            __declspec(dllexport) type __stdcall
#define HTTP2_DECLARE_NONSTD(type)     __declspec(dllexport) type
#define HTTP2_DECLARE_DATA             __declspec(dllexport)
#else
#define HTTP2_DECLARE(type)            __declspec(dllimport) type __stdcall
#define HTTP2_DECLARE_NONSTD(type)     __declspec(dllimport) type
#define HTTP2_DECLARE_DATA             __declspec(dllimport)
#endif

/** The http2_var_lookup() optional function retrieves HTTP2 environment
 * variables. */
APR_DECLARE_OPTIONAL_FN(char *, 
                        http2_var_lookup, (apr_pool_t *, server_rec *,
                                           conn_rec *, request_rec *,  char *));

/** An optional function which returns non-zero if the given connection
 * or its master connection is using HTTP/2. */
APR_DECLARE_OPTIONAL_FN(int, 
                        http2_is_h2, (conn_rec *));


/*******************************************************************************
 * HTTP/2 request engines
 ******************************************************************************/
 
struct apr_thread_cond_t;

typedef struct h2_req_engine h2_req_engine;

/**
 * Initialize a h2_req_engine. The structure will be passed in but
 * only the name and master are set. The function should initialize
 * all fields.
 * @param engine the allocated, partially filled structure
 * @param r      the first request to process, or NULL
 */
typedef apr_status_t h2_req_engine_init(h2_req_engine *engine, request_rec *r);

/**
 * The public structure of a h2_req_engine. It gets allocated by the http2
 * infrastructure, assigned id, type, pool, io and connection and passed to the
 * h2_req_engine_init() callback to complete initialization.
 * This happens whenever a new request gets "push"ed for an engine type and
 * no instance, or no free instance, for the type is available.
 */
struct h2_req_engine {
    const char *id;        /* identifier */
    apr_pool_t *pool;      /* pool for engine specific allocations */
    const char *type;      /* name of the engine type */
    unsigned char window_bits;/* preferred size of overall response data
                            * mod_http2 is willing to buffer as log2 */
    unsigned char req_window_bits;/* preferred size of response body data
                            * mod_http2 is willing to buffer per request,
                            * as log2 */
    apr_size_t capacity;   /* maximum concurrent requests */
    void *user_data;       /* user specific data */
};

/**
 * Push a request to an engine with the specified name for further processing.
 * If no such engine is available, einit is not NULL, einit is called 
 * with a new engine record and the caller is responsible for running the
 * new engine instance.
 * @param engine_type the type of the engine to add the request to
 * @param r           the request to push to an engine for processing
 * @param einit       an optional initialization callback for a new engine 
 *                    of the requested type, should no instance be available.
 *                    By passing a non-NULL callback, the caller is willing
 *                    to init and run a new engine itself.
 * @return APR_SUCCESS iff slave was successfully added to an engine
 */
APR_DECLARE_OPTIONAL_FN(apr_status_t, 
                        http2_req_engine_push, (const char *engine_type, 
                                                request_rec *r,
                                                h2_req_engine_init *einit));

/**
 * Get a new request for processing in this engine.
 * @param engine      the engine which is done processing the slave
 * @param timeout     wait a maximum amount of time for a new slave, 0 will not wait
 * @param pslave      the slave connection that needs processing or NULL
 * @return APR_SUCCESS if new request was assigned
 *         APR_EAGAIN/APR_TIMEUP if no new request is available
 *         APR_ECONNABORTED if the engine needs to shut down
 */
APR_DECLARE_OPTIONAL_FN(apr_status_t, 
                        http2_req_engine_pull, (h2_req_engine *engine, 
                                                apr_read_type_e block,
                                                request_rec **pr));
APR_DECLARE_OPTIONAL_FN(void, 
                        http2_req_engine_done, (h2_req_engine *engine, 
                                                conn_rec *rconn));
/**
 * The given request engine is done processing and needs to be excluded
 * from further handling. 
 * @param engine      the engine to exit
 */
APR_DECLARE_OPTIONAL_FN(void,
                        http2_req_engine_exit, (h2_req_engine *engine));


#define H2_TASK_ID_NOTE     "http2-task-id"

#endif
