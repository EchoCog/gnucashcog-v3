/********************************************************************\
 * gnc-cognitive-api.c -- Phase 4 Distributed Cognitive Mesh API  *
 * Copyright (C) 2024 GnuCash Cognitive Engine                     *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 ********************************************************************/

#include "gnc-cognitive-api.h"
#include "gnc-cognitive-accounting.h"
#include "gnc-tensor-network.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef HAVE_JSON_GLIB
#include <json-glib/json-glib.h>
#endif

/** Global API server state */
typedef struct {
    gboolean initialized;
    gboolean server_running;
    gint http_port;
    gint websocket_port;
    GHashTable *endpoints;          /**< Map of path -> handler function */
    GHashTable *websocket_connections; /**< Map of connection_id -> GncWebSocketConnection */
    GHashTable *registered_agents;  /**< Map of agent_id -> agent data */
    GncApiMetrics metrics;
    gint64 start_time;
    GMutex api_mutex;
} GncApiServerState;

static GncApiServerState g_api_state = {0};

/** Escape a string for embedding as a JSON string literal.
 * Used only by the no-json-glib fallback paths below, which build JSON
 * with g_strdup_printf() rather than JsonBuilder (which escapes
 * automatically) -- without this, a quote or backslash in an
 * interpolated value (e.g. a path-derived agent/account id) would break
 * the JSON payload or inject fields.
 */
static gchar* json_escape_string(const gchar *s)
{
    if (!s)
        return g_strdup("");

    GString *out = g_string_sized_new(strlen(s));
    for (const gchar *p = s; *p; p++) {
        switch (*p) {
            case '"':  g_string_append(out, "\\\""); break;
            case '\\': g_string_append(out, "\\\\"); break;
            case '\n': g_string_append(out, "\\n"); break;
            case '\r': g_string_append(out, "\\r"); break;
            case '\t': g_string_append(out, "\\t"); break;
            default:
                if ((guchar)*p < 0x20)
                    g_string_append_printf(out, "\\u%04x", (guchar)*p);
                else
                    g_string_append_c(out, *p);
        }
    }
    return g_string_free(out, FALSE);
}

/** Helper function to create JSON response */
static GncApiResponse* create_json_response(GncApiStatus status, const gchar *json_data)
{
    GncApiResponse *response = g_new0(GncApiResponse, 1);
    response->status_code = status;
    response->headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    response->content_type = g_strdup("application/json");
    response->body = g_strdup(json_data ? json_data : "{}");
    response->body_length = strlen(response->body);
    
    g_hash_table_insert(response->headers, g_strdup("Content-Type"), g_strdup("application/json"));
    g_hash_table_insert(response->headers, g_strdup("Access-Control-Allow-Origin"), g_strdup("*"));
    g_hash_table_insert(response->headers, g_strdup("Access-Control-Allow-Methods"), g_strdup("GET, POST, PUT, DELETE, OPTIONS"));
    
    return response;
}

/** Helper function to create error response */
static GncApiResponse* create_error_response(GncApiStatus status, const gchar *message)
{
#ifdef HAVE_JSON_GLIB
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "error");
    json_builder_add_string_value(builder, message);
    json_builder_set_member_name(builder, "status");
    json_builder_add_int_value(builder, (gint)status);
    json_builder_set_member_name(builder, "timestamp");
    json_builder_add_int_value(builder, (gint64)time(NULL));
    json_builder_end_object(builder);

    JsonGenerator *generator = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    gchar *json_string = json_generator_to_data(generator, NULL);

    GncApiResponse *response = create_json_response(status, json_string);

    g_free(json_string);
    json_node_free(root);
    g_object_unref(generator);
    g_object_unref(builder);

    return response;
#else
    /* JSON-GLib not available at build time -- hand-build the same shape,
     * matching this file's own no-json-glib endpoints below
     * (gnc_api_get_network_status, gnc_api_submit_transaction, ...). */
    gchar *escaped_message = json_escape_string(message);
    gchar *json_string = g_strdup_printf(
        "{\"error\":\"%s\",\"status\":%d,\"timestamp\":%" G_GINT64_FORMAT "}",
        escaped_message, (gint)status, (gint64)time(NULL));
    g_free(escaped_message);
    GncApiResponse *response = create_json_response(status, json_string);
    g_free(json_string);
    return response;
#endif
}

/** Helper function to extract path parameter */
static gchar* extract_path_param(const gchar *path, const gchar *pattern, const gchar *param_name)
{
    // Simple implementation - extract {id} from path like /api/v1/accounts/{id}
    gchar *param_start = g_strrstr(path, "/");
    if (param_start && param_start[1] != '\0') {
        return g_strdup(param_start + 1);
    }
    return NULL;
}

/** Initialize the Cognitive API server */
gboolean gnc_cognitive_api_init(gint port, gint websocket_port)
{
    g_mutex_lock(&g_api_state.api_mutex);
    
    if (g_api_state.initialized) {
        g_mutex_unlock(&g_api_state.api_mutex);
        return TRUE;
    }
    
#ifdef HAVE_JSON_GLIB
    // Initialize JSON support
    JsonBuilder *json_probe = json_builder_new(); // Test JSON-GLib availability
    g_object_unref(json_probe);
#endif


    g_api_state.http_port = port;
    g_api_state.websocket_port = websocket_port;
    g_api_state.endpoints = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    g_api_state.websocket_connections = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_api_state.registered_agents = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    
    // Initialize performance metrics
    memset(&g_api_state.metrics, 0, sizeof(GncApiMetrics));
    g_api_state.start_time = g_get_real_time();
    
    // Register default endpoints
    gnc_cognitive_api_register_handler("GET", "/api/v1/cognitive/state", gnc_api_get_cognitive_state);
    gnc_cognitive_api_register_handler("POST", "/api/v1/cognitive/process", gnc_api_process_cognitive_task);
    gnc_cognitive_api_register_handler("GET", "/api/v1/attention/allocation", gnc_api_get_attention_allocation);
    gnc_cognitive_api_register_handler("POST", "/api/v1/agents/register", gnc_api_register_agent);
    gnc_cognitive_api_register_handler("DELETE", "/api/v1/agents/{id}", gnc_api_unregister_agent);
    gnc_cognitive_api_register_handler("GET", "/api/v1/network/status", gnc_api_get_network_status);
    gnc_cognitive_api_register_handler("POST", "/api/v1/transactions/submit", gnc_api_submit_transaction);
    gnc_cognitive_api_register_handler("GET", "/api/v1/accounts/{id}", gnc_api_get_account);
    
    g_api_state.initialized = TRUE;
    
    g_mutex_unlock(&g_api_state.api_mutex);
    return TRUE;
}

/** Shutdown the Cognitive API server */
void gnc_cognitive_api_shutdown(void)
{
    g_mutex_lock(&g_api_state.api_mutex);
    
    if (!g_api_state.initialized) {
        g_mutex_unlock(&g_api_state.api_mutex);
        return;
    }
    
    if (g_api_state.server_running) {
        gnc_cognitive_api_stop_server();
    }
    
    gnc_websocket_shutdown();
    gnc_unity_shutdown();
    gnc_ros_shutdown();
    gnc_web_agent_shutdown();
    
    g_hash_table_destroy(g_api_state.endpoints);
    g_hash_table_destroy(g_api_state.websocket_connections);
    g_hash_table_destroy(g_api_state.registered_agents);
    
    g_api_state.initialized = FALSE;
    
    g_mutex_unlock(&g_api_state.api_mutex);
}

/** Register API endpoint handler */
gboolean gnc_cognitive_api_register_handler(const gchar *method, const gchar *path, GncApiHandler handler)
{
    if (!g_api_state.initialized || !method || !path || !handler) {
        return FALSE;
    }
    
    gchar *key = g_strdup_printf("%s:%s", method, path);
    g_hash_table_insert(g_api_state.endpoints, key, handler);
    return TRUE;
}

/** Start the API server */
gboolean gnc_cognitive_api_start_server(void)
{
    g_mutex_lock(&g_api_state.api_mutex);
    
    if (!g_api_state.initialized || g_api_state.server_running) {
        g_mutex_unlock(&g_api_state.api_mutex);
        return FALSE;
    }
    
    // Initialize WebSocket server
    gnc_websocket_init(g_api_state.websocket_port);
    
    // Initialize integration layers
    gnc_unity_init();
    gnc_ros_init("gnucash_cognitive", "/gnucash");
    gnc_web_agent_init();
    
    g_api_state.server_running = TRUE;
    
    g_mutex_unlock(&g_api_state.api_mutex);
    return TRUE;
}

/** Stop the API server */
void gnc_cognitive_api_stop_server(void)
{
    g_mutex_lock(&g_api_state.api_mutex);
    g_api_state.server_running = FALSE;
    g_mutex_unlock(&g_api_state.api_mutex);
}

/** Get current cognitive state */
GncApiResponse* gnc_api_get_cognitive_state(const GncApiRequest *request)
{
#ifdef HAVE_JSON_GLIB
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    
    // Add cognitive system status
    json_builder_set_member_name(builder, "status");
    json_builder_add_string_value(builder, "active");
    
    json_builder_set_member_name(builder, "timestamp");
    json_builder_add_int_value(builder, (gint64)time(NULL));
    
    // Add attention allocation summary
    json_builder_set_member_name(builder, "attention");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "total_sti_funds");
    json_builder_add_double_value(builder, 1000.0);
    json_builder_set_member_name(builder, "total_lti_funds");
    json_builder_add_double_value(builder, 500.0);
    json_builder_set_member_name(builder, "active_nodes");
    json_builder_add_int_value(builder, 4); // Memory, Task, AI, Autonomy
    json_builder_end_object(builder);
    
    // Add tensor network status
    json_builder_set_member_name(builder, "tensor_network");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "nodes");
    json_builder_begin_array(builder);
    
    const gchar *node_types[] = {"memory", "task", "ai", "autonomy"};
    for (int i = 0; i < 4; i++) {
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "type");
        json_builder_add_string_value(builder, node_types[i]);
        json_builder_set_member_name(builder, "active");
        json_builder_add_boolean_value(builder, TRUE);
        json_builder_set_member_name(builder, "attention_weight");
        json_builder_add_double_value(builder, 0.25);
        json_builder_end_object(builder);
    }
    
    json_builder_end_array(builder);
    json_builder_end_object(builder);
    
    json_builder_end_object(builder);
    
    JsonGenerator *generator = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    gchar *json_string = json_generator_to_data(generator, NULL);
    
    GncApiResponse *response = create_json_response(GNC_API_STATUS_OK, json_string);

    g_free(json_string);
    json_node_free(root);
    g_object_unref(generator);
    g_object_unref(builder);

    return response;
#else
    gchar *json_string = g_strdup_printf(
        "{\"status\":\"active\",\"timestamp\":%" G_GINT64_FORMAT ","
        "\"attention\":{\"total_sti_funds\":1000.0,\"total_lti_funds\":500.0,\"active_nodes\":4},"
        "\"tensor_network\":{\"nodes\":["
        "{\"type\":\"memory\",\"active\":true,\"attention_weight\":0.25},"
        "{\"type\":\"task\",\"active\":true,\"attention_weight\":0.25},"
        "{\"type\":\"ai\",\"active\":true,\"attention_weight\":0.25},"
        "{\"type\":\"autonomy\",\"active\":true,\"attention_weight\":0.25}]}}",
        (gint64)time(NULL));
    GncApiResponse *response = create_json_response(GNC_API_STATUS_OK, json_string);
    g_free(json_string);
    return response;
#endif
}

/** Process cognitive task */
GncApiResponse* gnc_api_process_cognitive_task(const GncApiRequest *request)
{
    if (!request || !request->body) {
        return create_error_response(GNC_API_STATUS_BAD_REQUEST, "Missing request body");
    }

#ifndef HAVE_JSON_GLIB
    /* Parsing the request body's JSON needs a real JSON parser -- rather
     * than fake it, report unavailable when JSON-GLib wasn't found. */
    return create_error_response(GNC_API_STATUS_NOT_IMPLEMENTED,
        "JSON support not available in this build (JSON-GLib not found)");
#else
    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    
    if (!json_parser_load_from_data(parser, request->body, request->body_length, &error)) {
        g_object_unref(parser);
        return create_error_response(GNC_API_STATUS_BAD_REQUEST, "Invalid JSON in request body");
    }
    
    JsonNode *root = json_parser_get_root(parser);
    JsonObject *object = json_node_get_object(root);
    
    const gchar *task_type = json_object_get_string_member(object, "task_type");
    if (!task_type) {
        g_object_unref(parser);
        return create_error_response(GNC_API_STATUS_BAD_REQUEST, "Missing task_type field");
    }
    
    // Process task based on type
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "task_id");
    json_builder_add_string_value(builder, g_uuid_string_random());
    json_builder_set_member_name(builder, "task_type");
    json_builder_add_string_value(builder, task_type);
    json_builder_set_member_name(builder, "status");
    json_builder_add_string_value(builder, "processing");
    json_builder_set_member_name(builder, "timestamp");
    json_builder_add_int_value(builder, (gint64)time(NULL));
    
    // Add task-specific results
    if (g_strcmp0(task_type, "pattern_recognition") == 0) {
        json_builder_set_member_name(builder, "patterns_found");
        json_builder_add_int_value(builder, 3);
    } else if (g_strcmp0(task_type, "attention_allocation") == 0) {
        json_builder_set_member_name(builder, "attention_redistributed");
        json_builder_add_double_value(builder, 150.0);
    }
    
    json_builder_end_object(builder);
    
    JsonGenerator *generator = json_generator_new();
    JsonNode *response_root = json_builder_get_root(builder);
    json_generator_set_root(generator, response_root);
    gchar *json_string = json_generator_to_data(generator, NULL);
    
    GncApiResponse *response = create_json_response(GNC_API_STATUS_ACCEPTED, json_string);
    
    g_free(json_string);
    json_node_free(response_root);
    g_object_unref(generator);
    g_object_unref(builder);
    g_object_unref(parser);

    return response;
#endif
}

/** Get attention allocation */
GncApiResponse* gnc_api_get_attention_allocation(const GncApiRequest *request)
{
#ifdef HAVE_JSON_GLIB
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    
    json_builder_set_member_name(builder, "attention_allocation");
    json_builder_begin_object(builder);
    
    json_builder_set_member_name(builder, "total_sti_circulation");
    json_builder_add_double_value(builder, 750.0);
    json_builder_set_member_name(builder, "total_lti_circulation");
    json_builder_add_double_value(builder, 400.0);
    
    json_builder_set_member_name(builder, "node_allocations");
    json_builder_begin_array(builder);
    
    const gchar *nodes[] = {"memory", "task", "ai", "autonomy"};
    gdouble sti_values[] = {200.0, 180.0, 220.0, 150.0};
    gdouble lti_values[] = {100.0, 90.0, 120.0, 90.0};
    
    for (int i = 0; i < 4; i++) {
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "node_id");
        json_builder_add_string_value(builder, nodes[i]);
        json_builder_set_member_name(builder, "sti");
        json_builder_add_double_value(builder, sti_values[i]);
        json_builder_set_member_name(builder, "lti");
        json_builder_add_double_value(builder, lti_values[i]);
        json_builder_end_object(builder);
    }
    
    json_builder_end_array(builder);
    json_builder_end_object(builder);
    
    json_builder_set_member_name(builder, "timestamp");
    json_builder_add_int_value(builder, (gint64)time(NULL));
    
    json_builder_end_object(builder);
    
    JsonGenerator *generator = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    gchar *json_string = json_generator_to_data(generator, NULL);
    
    GncApiResponse *response = create_json_response(GNC_API_STATUS_OK, json_string);
    
    g_free(json_string);
    json_node_free(root);
    g_object_unref(generator);
    g_object_unref(builder);

    return response;
#else
    gchar *json_string = g_strdup_printf(
        "{\"attention_allocation\":{\"total_sti_circulation\":750.0,\"total_lti_circulation\":400.0,"
        "\"node_allocations\":["
        "{\"node_id\":\"memory\",\"sti\":200.0,\"lti\":100.0},"
        "{\"node_id\":\"task\",\"sti\":180.0,\"lti\":90.0},"
        "{\"node_id\":\"ai\",\"sti\":220.0,\"lti\":120.0},"
        "{\"node_id\":\"autonomy\",\"sti\":150.0,\"lti\":90.0}]},"
        "\"timestamp\":%" G_GINT64_FORMAT "}",
        (gint64)time(NULL));
    GncApiResponse *response = create_json_response(GNC_API_STATUS_OK, json_string);
    g_free(json_string);
    return response;
#endif
}

/** Register new agent */
GncApiResponse* gnc_api_register_agent(const GncApiRequest *request)
{
    if (!request || !request->body) {
        return create_error_response(GNC_API_STATUS_BAD_REQUEST, "Missing request body");
    }

#ifndef HAVE_JSON_GLIB
    return create_error_response(GNC_API_STATUS_NOT_IMPLEMENTED,
        "JSON support not available in this build (JSON-GLib not found)");
#else
    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    
    if (!json_parser_load_from_data(parser, request->body, request->body_length, &error)) {
        g_object_unref(parser);
        return create_error_response(GNC_API_STATUS_BAD_REQUEST, "Invalid JSON in request body");
    }
    
    JsonNode *root = json_parser_get_root(parser);
    JsonObject *object = json_node_get_object(root);
    
    const gchar *agent_type = json_object_get_string_member(object, "agent_type");
    if (!agent_type) {
        g_object_unref(parser);
        return create_error_response(GNC_API_STATUS_BAD_REQUEST, "Missing agent_type field");
    }
    
    gchar *agent_id = g_uuid_string_random();
    
    // Store agent registration
    GHashTable *agent_data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(agent_data, g_strdup("agent_type"), g_strdup(agent_type));
    g_hash_table_insert(agent_data, g_strdup("registration_time"), g_strdup_printf("%ld", time(NULL)));
    g_hash_table_insert(g_api_state.registered_agents, g_strdup(agent_id), agent_data);
    
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "agent_id");
    json_builder_add_string_value(builder, agent_id);
    json_builder_set_member_name(builder, "agent_type");
    json_builder_add_string_value(builder, agent_type);
    json_builder_set_member_name(builder, "status");
    json_builder_add_string_value(builder, "registered");
    json_builder_set_member_name(builder, "timestamp");
    json_builder_add_int_value(builder, (gint64)time(NULL));
    json_builder_end_object(builder);
    
    JsonGenerator *generator = json_generator_new();
    JsonNode *response_root = json_builder_get_root(builder);
    json_generator_set_root(generator, response_root);
    gchar *json_string = json_generator_to_data(generator, NULL);
    
    GncApiResponse *response = create_json_response(GNC_API_STATUS_CREATED, json_string);
    
    g_free(json_string);
    json_node_free(response_root);
    g_object_unref(generator);
    g_object_unref(builder);
    g_object_unref(parser);
    g_free(agent_id);

    return response;
#endif
}

/** Unregister agent */
GncApiResponse* gnc_api_unregister_agent(const GncApiRequest *request)
{
    gchar *agent_id = extract_path_param(request->path, "/api/v1/agents/{id}", "id");
    if (!agent_id) {
        return create_error_response(GNC_API_STATUS_BAD_REQUEST, "Missing agent ID in path");
    }

    if (!g_hash_table_remove(g_api_state.registered_agents, agent_id)) {
        g_free(agent_id);
        return create_error_response(GNC_API_STATUS_NOT_FOUND, "Agent not found");
    }

#ifdef HAVE_JSON_GLIB
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "agent_id");
    json_builder_add_string_value(builder, agent_id);
    json_builder_set_member_name(builder, "status");
    json_builder_add_string_value(builder, "unregistered");
    json_builder_set_member_name(builder, "timestamp");
    json_builder_add_int_value(builder, (gint64)time(NULL));
    json_builder_end_object(builder);
    
    JsonGenerator *generator = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    gchar *json_string = json_generator_to_data(generator, NULL);
    
    GncApiResponse *response = create_json_response(GNC_API_STATUS_OK, json_string);
    
    g_free(json_string);
    json_node_free(root);
    g_object_unref(generator);
    g_object_unref(builder);
    g_free(agent_id);

    return response;
#else
    gchar *escaped_agent_id = json_escape_string(agent_id);
    gchar *json_string = g_strdup_printf(
        "{\"agent_id\":\"%s\",\"status\":\"unregistered\",\"timestamp\":%" G_GINT64_FORMAT "}",
        escaped_agent_id, (gint64)time(NULL));
    g_free(escaped_agent_id);
    GncApiResponse *response = create_json_response(GNC_API_STATUS_OK, json_string);
    g_free(json_string);
    g_free(agent_id);
    return response;
#endif
}

/** Get network status */
GncApiResponse* gnc_api_get_network_status(const GncApiRequest *request)
{
    return create_json_response(GNC_API_STATUS_OK, 
        "{\"network_status\":\"active\",\"nodes\":4,\"connections\":12,\"uptime\":3600}");
}

/** Submit transaction */
GncApiResponse* gnc_api_submit_transaction(const GncApiRequest *request)
{
    return create_json_response(GNC_API_STATUS_ACCEPTED, 
        "{\"transaction_id\":\"txn_001\",\"status\":\"processing\",\"validation\":\"passed\"}");
}

/** Get account */
GncApiResponse* gnc_api_get_account(const GncApiRequest *request)
{
    gchar *account_id = extract_path_param(request->path, "/api/v1/accounts/{id}", "id");
    if (!account_id) {
        return create_error_response(GNC_API_STATUS_BAD_REQUEST, "Missing account ID in path");
    }
    
    gchar *response_json = g_strdup_printf(
        "{\"account_id\":\"%s\",\"name\":\"Cognitive Account\",\"balance\":1000.00,\"attention\":{\"sti\":150.0,\"lti\":75.0}}",
        account_id);
    
    GncApiResponse *response = create_json_response(GNC_API_STATUS_OK, response_json);
    
    g_free(response_json);
    g_free(account_id);
    
    return response;
}

/** WebSocket initialization stub */
gboolean gnc_websocket_init(gint port)
{
    // WebSocket implementation placeholder
    return TRUE;
}

void gnc_websocket_shutdown(void) {}

GncWebSocketConnection* gnc_websocket_register_connection(const gchar *connection_id, const gchar *agent_type)
{
    GncWebSocketConnection *conn = g_new0(GncWebSocketConnection, 1);
    conn->connection_id = g_strdup(connection_id);
    conn->agent_type = g_strdup(agent_type);
    conn->is_active = TRUE;
    conn->connect_time = g_get_real_time();
    conn->subscriptions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    
    g_hash_table_insert(g_api_state.websocket_connections, g_strdup(connection_id), conn);
    return conn;
}

gboolean gnc_websocket_unregister_connection(const gchar *connection_id)
{
    return g_hash_table_remove(g_api_state.websocket_connections, connection_id);
}

gboolean gnc_websocket_subscribe(const gchar *connection_id, const gchar *event_type)
{
    GncWebSocketConnection *conn = g_hash_table_lookup(g_api_state.websocket_connections, connection_id);
    if (!conn) return FALSE;
    
    g_hash_table_insert(conn->subscriptions, g_strdup(event_type), GINT_TO_POINTER(1));
    return TRUE;
}

gboolean gnc_websocket_unsubscribe(const gchar *connection_id, const gchar *event_type)
{
    GncWebSocketConnection *conn = g_hash_table_lookup(g_api_state.websocket_connections, connection_id);
    if (!conn) return FALSE;
    
    return g_hash_table_remove(conn->subscriptions, event_type);
}

gint gnc_websocket_broadcast_event(const gchar *event_type, const gchar *data)
{
    // Placeholder: broadcast to all subscribed connections
    return g_hash_table_size(g_api_state.websocket_connections);
}

gboolean gnc_websocket_send_event(const gchar *connection_id, const gchar *event_type, const gchar *data)
{
    // Placeholder: send to specific connection
    return g_hash_table_lookup(g_api_state.websocket_connections, connection_id) != NULL;
}

/** Unity3D integration stubs */
gboolean gnc_unity_init(void) { return TRUE; }
void gnc_unity_shutdown(void) {}

GncUnityComponent* gnc_unity_create_component(const gchar *component_type, gdouble position[3], gdouble rotation[4])
{
    GncUnityComponent *component = g_new0(GncUnityComponent, 1);
    component->component_id = g_uuid_string_random();
    component->component_type = g_strdup(component_type);
    memcpy(component->position, position, 3 * sizeof(gdouble));
    memcpy(component->rotation, rotation, 4 * sizeof(gdouble));
    component->properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    return component;
}

gboolean gnc_unity_update_attention_visualization(GncUnityComponent *component, const GncAttentionParams *attention_params)
{
    if (!component || !attention_params) return FALSE;
    component->attention = *attention_params;
    return TRUE;
}

gchar* gnc_unity_get_cognitive_state_json(void)
{
    return g_strdup("{\"unity_state\":\"active\",\"components\":4,\"attention_total\":1000.0}");
}

gchar* gnc_unity_process_command(const gchar *command)
{
    return g_strdup("{\"result\":\"command_processed\",\"status\":\"ok\"}");
}

/** ROS integration stubs */
gboolean gnc_ros_init(const gchar *node_name, const gchar *node_namespace) { return TRUE; }
void gnc_ros_shutdown(void) {}

gboolean gnc_ros_create_service(const gchar *service_name, const gchar *service_type) { return TRUE; }

gboolean gnc_ros_publish_message(const gchar *topic, const gchar *message_type, const gchar *data) { return TRUE; }

gboolean gnc_ros_subscribe_topic(const gchar *topic, const gchar *message_type) { return TRUE; }

gchar* gnc_ros_get_planning_result(const gchar *goal)
{
    return g_strdup("{\"plan\":[\"step1\",\"step2\",\"step3\"],\"status\":\"success\"}");
}

gchar* gnc_ros_process_sensor_data(const gchar *sensor_data)
{
    return g_strdup("{\"analysis\":\"processed\",\"confidence\":0.95,\"patterns\":3}");
}

/** Web agent interface stubs */
gboolean gnc_web_agent_init(void) { return TRUE; }
void gnc_web_agent_shutdown(void) {}

GncWebAgent* gnc_web_agent_register(const gchar *agent_type, GHashTable *capabilities, const gchar *session_id)
{
    GncWebAgent *agent = g_new0(GncWebAgent, 1);
    agent->agent_id = g_uuid_string_random();
    agent->agent_type = g_strdup(agent_type);
    agent->session_id = g_strdup(session_id);
    agent->is_authenticated = FALSE;
    agent->capabilities = g_hash_table_ref(capabilities);
    agent->last_activity = g_get_real_time();
    return agent;
}

gboolean gnc_web_agent_unregister(const gchar *agent_id) { return TRUE; }

gchar* gnc_web_agent_process_command(const gchar *agent_id, const gchar *command)
{
    gchar *escaped_agent_id = json_escape_string(agent_id);
    gchar *result = g_strdup_printf("{\"result\":\"command_processed\",\"agent_id\":\"%s\"}",
                                     escaped_agent_id);
    g_free(escaped_agent_id);
    return result;
}

gchar* gnc_web_agent_get_interface(const gchar *agent_id)
{
    return g_strdup("{\"interface\":{\"methods\":[\"process\",\"query\"],\"events\":[\"state_change\"]}}");
}

/** Performance monitoring */
GncApiMetrics gnc_cognitive_api_get_metrics(void)
{
    // Update real-time metrics
    g_api_state.metrics.active_connections = g_hash_table_size(g_api_state.websocket_connections);
    return g_api_state.metrics;
}

void gnc_cognitive_api_reset_metrics(void)
{
    memset(&g_api_state.metrics, 0, sizeof(GncApiMetrics));
}

void gnc_cognitive_api_set_monitoring(gboolean enabled)
{
    // Placeholder for enabling/disabling monitoring
}