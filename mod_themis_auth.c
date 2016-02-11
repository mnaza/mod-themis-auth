/*
 * Copyright (c) 2015 Cossack Labs Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "plugin.h"
#include "log.h"
#include "response.h"

#define LOG(level, ...)                                           \
    if (pc->loglevel >= level) {                                  \
        log_error_write(srv, __FILE__, __LINE__, __VA_ARGS__);    \
    }

#define FATAL(...) LOG(0, __VA_ARGS__)
#define ERROR(...) LOG(1, __VA_ARGS__)
#define WARN(...)  LOG(2, __VA_ARGS__)
#define INFO(...)  LOG(3, __VA_ARGS__)
#define DEBUG(...) LOG(4, __VA_ARGS__)

#define HEADER(con, key)                                                \
    (data_string *)array_get_element((con)->request.headers, (key))

typedef struct {
    int loglevel;
} plugin_config;

typedef struct {
    PLUGIN_DATA;
    plugin_config **config;
    plugin_config   conf;
} plugin_data;

static plugin_config * merge_config(server *srv, connection *con, plugin_data *pd) {
#define PATCH(x) pd->conf.x = pc->x
#define MATCH(k) if (buffer_is_equal_string(du->key, CONST_STR_LEN(k)))
#define MERGE(k, x) MATCH(k) PATCH(x)

    size_t i, j;
    plugin_config *pc = pd->config[0];

    PATCH(loglevel);

    for (i = 1; i < srv->config_context->used; i++) {
        data_config *dc = (data_config *)srv->config_context->data[i];
        if (! config_check_cond(srv, con, dc)) continue;
        pc = pd->config[i];
        for (j = 0; j < dc->value->used; j++) {
            data_unset *du = dc->value->data[j];
            MERGE("tgmis_auth.loglevel", loglevel);
        }
    }
    return &(pd->conf);
#undef PATCH
#undef MATCH
#undef MERGE
}

INIT_FUNC(module_init) {
    plugin_data *pd;

    pd = calloc(1, sizeof(*pd));
    return pd;
}

FREE_FUNC(module_free) {
    plugin_data *pd = p_d;
    if (! pd) return HANDLER_GO_ON;
    free(pd);
    return HANDLER_GO_ON;
}

URIHANDLER_FUNC(module_uri_handler) {
    plugin_data   *pd = p_d;
    plugin_config *pc = merge_config(srv, con, pd);
    return HANDLER_GO_ON;
}

SETDEFAULTS_FUNC(module_set_defaults) {
    plugin_data *pd = p_d;
    size_t i;

    config_values_t cv[] = {
        { "auth-ticket.loglevel",
          NULL, T_CONFIG_INT,    T_CONFIG_SCOPE_CONNECTION },
        { NULL, NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
    };

    pd->config = calloc(1, srv->config_context->used * sizeof(specific_config *));

    for (i = 0; i < srv->config_context->used; i++) {
        plugin_config *pc;

        pc = pd->config[i] = calloc(1, sizeof(plugin_config));
        pc->loglevel = 1;

        cv[0].destination = &(pc->loglevel);

        array *ca = ((data_config *)srv->config_context->data[i])->value;
        if (config_insert_values_global(srv, ca, cv, T_CONFIG_SCOPE_CONNECTION) != 0) {
            return HANDLER_ERROR;
        }
    }
    return HANDLER_GO_ON;
}

int
mod_themis_auth_plugin_init(plugin *p) {
    p->version          = LIGHTTPD_VERSION_ID;
    p->name             = buffer_init_string("themis_auth");
    p->init             = module_init;
    p->set_defaults     = module_set_defaults;
    p->cleanup          = module_free;
    p->handle_uri_clean = module_uri_handler;
    p->data             = NULL;

    return 0;
}
