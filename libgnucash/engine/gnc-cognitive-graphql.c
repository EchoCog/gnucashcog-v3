/********************************************************************\
 * gnc-cognitive-graphql.c -- GraphQL Interface Implementation    *
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

#include "gnc-cognitive-graphql.h"
#include "gnc-cognitive-accounting.h"
#include "gnc-cognitive-json-util.h"
#include "gnc-tensor-network.h"
#ifdef HAVE_JSON_GLIB
#include <json-glib/json-glib.h>
#endif
#include <string.h>
#include <time.h>

/** GraphQL Schema Definition (SDL format) */
const gchar* GNC_GRAPHQL_SCHEMA = 
"type Query {\n"
"  cognitiveState: CognitiveState\n"
"  attentionAllocation(nodeId: String): AttentionAllocation\n"
"  tensorNetwork: TensorNetwork\n"
"  account(id: String!): Account\n"
"  transaction(id: String!): Transaction\n"
"  patterns(threshold: Float): [Pattern]\n"
"  agents: [Agent]\n"
"  metrics: SystemMetrics\n"
"}\n"
"\n"
"type Mutation {\n"
"  processTask(input: TaskInput!): TaskResult\n"
"  registerAgent(input: AgentInput!): Agent\n"
"  updateAttention(input: AttentionInput!): AttentionAllocation\n"
"  submitTransaction(input: TransactionInput!): Transaction\n"
"  allocateAttention(nodeId: String!, amount: Float!): Boolean\n"
"}\n"
"\n"
"type Subscription {\n"
"  cognitiveStateUpdates: CognitiveState\n"
"  attentionChanges(nodeId: String): AttentionUpdate\n"
"  transactionEvents: TransactionEvent\n"
"  patternDiscovery: PatternEvent\n"
"  agentActivity: AgentEvent\n"
"}\n"
"\n"
"type CognitiveState {\n"
"  status: String!\n"
"  timestamp: String!\n"
"  attention: AttentionSummary!\n"
"  tensorNetwork: TensorNetworkStatus!\n"
"  cognitiveLoad: Float!\n"
"  activeAgents: Int!\n"
"}\n"
"\n"
"type AttentionAllocation {\n"
"  totalSti: Float!\n"
"  totalLti: Float!\n"
"  nodeAllocations: [NodeAttention!]!\n"
"  stiFundBalance: Float!\n"
"  ltiFundBalance: Float!\n"
"  efficiency: Float!\n"
"}\n"
"\n"
"type NodeAttention {\n"
"  nodeId: String!\n"
"  nodeType: String!\n"
"  sti: Float!\n"
"  lti: Float!\n"
"  vlti: Float!\n"
"  confidence: Float!\n"
"  activityLevel: Float!\n"
"}\n"
"\n"
"type TensorNetwork {\n"
"  status: String!\n"
"  nodes: [TensorNode!]!\n"
"  connections: Int!\n"
"  messageQueue: Int!\n"
"  processingLoad: Float!\n"
"}\n"
"\n"
"type TensorNode {\n"
"  id: String!\n"
"  type: NodeType!\n"
"  active: Boolean!\n"
"  attentionWeight: Float!\n"
"  lastUpdate: String!\n"
"  processingCapacity: Float!\n"
"}\n"
"\n"
"enum NodeType {\n"
"  MEMORY\n"
"  TASK\n"
"  AI\n"
"  AUTONOMY\n"
"}\n"
"\n"
"type Account {\n"
"  id: String!\n"
"  name: String!\n"
"  type: AccountType!\n"
"  balance: Float!\n"
"  cognitiveType: CognitiveAccountType!\n"
"  attention: NodeAttention!\n"
"  transactions: [Transaction!]!\n"
"}\n"
"\n"
"enum AccountType {\n"
"  BANK\n"
"  INCOME\n"
"  EXPENSE\n"
"  ASSET\n"
"  LIABILITY\n"
"  EQUITY\n"
"}\n"
"\n"
"enum CognitiveAccountType {\n"
"  TRADITIONAL\n"
"  ADAPTIVE\n"
"  PREDICTIVE\n"
"  MULTIMODAL\n"
"  ATTENTION_DRIVEN\n"
"}\n"
"\n"
"type Transaction {\n"
"  id: String!\n"
"  description: String!\n"
"  amount: Float!\n"
"  timestamp: String!\n"
"  splits: [Split!]!\n"
"  cognitiveValidation: ValidationResult!\n"
"}\n"
"\n"
"type Split {\n"
"  account: Account!\n"
"  amount: Float!\n"
"  reconciled: Boolean!\n"
"}\n"
"\n"
"type ValidationResult {\n"
"  isValid: Boolean!\n"
"  confidence: Float!\n"
"  plnScore: Float!\n"
"  issues: [String!]!\n"
"}\n"
"\n"
"type Pattern {\n"
"  id: String!\n"
"  type: String!\n"
"  confidence: Float!\n"
"  frequency: Int!\n"
"  accounts: [Account!]!\n"
"  emergenceScore: Float!\n"
"  discovered: String!\n"
"}\n"
"\n"
"type Agent {\n"
"  id: String!\n"
"  type: AgentType!\n"
"  status: String!\n"
"  capabilities: [String!]!\n"
"  lastActivity: String!\n"
"  cognitiveLoad: Float!\n"
"}\n"
"\n"
"enum AgentType {\n"
"  UNITY3D\n"
"  ROS_NODE\n"
"  WEB_AGENT\n"
"  API_CLIENT\n"
"}\n"
"\n"
"type SystemMetrics {\n"
"  uptime: Int!\n"
"  totalRequests: Int!\n"
"  requestsPerSecond: Float!\n"
"  avgResponseTime: Float!\n"
"  memoryUsage: Float!\n"
"  cpuUsage: Float!\n"
"  activeConnections: Int!\n"
"}\n"
"\n"
"input TaskInput {\n"
"  type: String!\n"
"  priority: Int\n"
"  parameters: JSON\n"
"  deadline: String\n"
"}\n"
"\n"
"input AgentInput {\n"
"  type: AgentType!\n"
"  capabilities: [String!]!\n"
"  sessionId: String\n"
"}\n"
"\n"
"input AttentionInput {\n"
"  nodeId: String!\n"
"  stiDelta: Float\n"
"  ltiDelta: Float\n"
"  priority: Float\n"
"}\n"
"\n"
"input TransactionInput {\n"
"  description: String!\n"
"  splits: [SplitInput!]!\n"
"}\n"
"\n"
"input SplitInput {\n"
"  accountId: String!\n"
"  amount: Float!\n"
"}\n"
"\n"
"type TaskResult {\n"
"  id: String!\n"
"  type: String!\n"
"  status: String!\n"
"  result: JSON\n"
"  processingTime: Float!\n"
"}\n"
"\n"
"type AttentionUpdate {\n"
"  nodeId: String!\n"
"  oldSti: Float!\n"
"  newSti: Float!\n"
"  oldLti: Float!\n"
"  newLti: Float!\n"
"  reason: String!\n"
"  timestamp: String!\n"
"}\n"
"\n"
"type TransactionEvent {\n"
"  type: String!\n"
"  transaction: Transaction!\n"
"  timestamp: String!\n"
"}\n"
"\n"
"type PatternEvent {\n"
"  pattern: Pattern!\n"
"  action: String!\n"
"  timestamp: String!\n"
"}\n"
"\n"
"type AgentEvent {\n"
"  agent: Agent!\n"
"  action: String!\n"
"  timestamp: String!\n"
"}\n"
"\n"
"scalar JSON\n";

/** Global GraphQL state */
typedef struct {
    gboolean initialized;
    GHashTable *subscriptions;     /**< Map of subscription_id -> GncGraphQLSubscription */
    GMutex graphql_mutex;
} GncGraphQLState;

static GncGraphQLState g_graphql_state = {0};

/** Simple GraphQL query parser - extracts operation type and name */
typedef struct {
    gchar *operation_type;  /**< "query", "mutation", or "subscription" */
    gchar *operation_name;  /**< Optional operation name */
    gchar *selection_set;   /**< Field selections */
} ParsedQuery;

/** Parse GraphQL query to extract basic information */
static ParsedQuery* parse_graphql_query(const gchar *query)
{
    ParsedQuery *parsed = g_new0(ParsedQuery, 1);
    
    // Simple parsing - look for operation type at start
    gchar *trimmed = g_strstrip(g_strdup(query));
    
    if (g_str_has_prefix(trimmed, "query")) {
        parsed->operation_type = g_strdup("query");
        parsed->selection_set = g_strdup("cognitiveState");
    } else if (g_str_has_prefix(trimmed, "mutation")) {
        parsed->operation_type = g_strdup("mutation");
        parsed->selection_set = g_strdup("processTask");
    } else if (g_str_has_prefix(trimmed, "subscription")) {
        parsed->operation_type = g_strdup("subscription");
        parsed->selection_set = g_strdup("cognitiveStateUpdates");
    } else {
        // Default to query
        parsed->operation_type = g_strdup("query");
        parsed->selection_set = g_strdup("cognitiveState");
    }
    
    g_free(trimmed);
    return parsed;
}

static void free_parsed_query(ParsedQuery *parsed)
{
    if (!parsed) return;
    g_free(parsed->operation_type);
    g_free(parsed->operation_name);
    g_free(parsed->selection_set);
    g_free(parsed);
}

/** Initialize GraphQL interface */
gboolean gnc_graphql_init(void)
{
    g_mutex_lock(&g_graphql_state.graphql_mutex);
    
    if (g_graphql_state.initialized) {
        g_mutex_unlock(&g_graphql_state.graphql_mutex);
        return TRUE;
    }
    
    g_graphql_state.subscriptions = g_hash_table_new_full(g_str_hash, g_str_equal, 
                                                          g_free, (GDestroyNotify)gnc_graphql_free_subscription);
    
    g_graphql_state.initialized = TRUE;
    
    g_mutex_unlock(&g_graphql_state.graphql_mutex);
    return TRUE;
}

/** Shutdown GraphQL interface */
void gnc_graphql_shutdown(void)
{
    g_mutex_lock(&g_graphql_state.graphql_mutex);
    
    if (!g_graphql_state.initialized) {
        g_mutex_unlock(&g_graphql_state.graphql_mutex);
        return;
    }
    
    g_hash_table_destroy(g_graphql_state.subscriptions);
    
    g_graphql_state.initialized = FALSE;
    
    g_mutex_unlock(&g_graphql_state.graphql_mutex);
}

/** Execute GraphQL query */
GncGraphQLResult* gnc_graphql_execute_query(const GncGraphQLQuery *query)
{
    if (!query || !query->query) {
        return NULL;
    }
    
    ParsedQuery *parsed = parse_graphql_query(query->query);
    GncGraphQLResult *result = g_new0(GncGraphQLResult, 1);
    result->errors = NULL;
    result->extensions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    result->has_errors = FALSE;
    
    // Route to appropriate resolver based on operation type and selection
    if (g_strcmp0(parsed->operation_type, "query") == 0) {
        if (g_strstr_len(parsed->selection_set, -1, "cognitiveState")) {
            result->data = gnc_graphql_resolve_cognitive_state(query->variables, query->context);
        } else if (g_strstr_len(parsed->selection_set, -1, "attentionAllocation")) {
            result->data = gnc_graphql_resolve_attention_allocation(query->variables, query->context);
        } else if (g_strstr_len(parsed->selection_set, -1, "tensorNetwork")) {
            result->data = gnc_graphql_resolve_tensor_network(query->variables, query->context);
        } else if (g_strstr_len(parsed->selection_set, -1, "account")) {
            result->data = gnc_graphql_resolve_account(query->variables, query->context);
        } else if (g_strstr_len(parsed->selection_set, -1, "patterns")) {
            result->data = gnc_graphql_resolve_patterns(query->variables, query->context);
        } else if (g_strstr_len(parsed->selection_set, -1, "agents")) {
            result->data = gnc_graphql_resolve_agents(query->variables, query->context);
        } else {
            result->data = g_strdup("{\"cognitiveState\":{\"status\":\"active\",\"timestamp\":\"2024-01-01T00:00:00Z\"}}");
        }
    } else {
        result->has_errors = TRUE;
        result->errors = g_list_append(result->errors, g_strdup("Invalid operation type for query"));
        result->data = g_strdup("null");
    }
    
    free_parsed_query(parsed);
    return result;
}

/** Execute GraphQL mutation */
GncGraphQLResult* gnc_graphql_execute_mutation(const GncGraphQLQuery *mutation)
{
    if (!mutation || !mutation->query) {
        return NULL;
    }
    
    ParsedQuery *parsed = parse_graphql_query(mutation->query);
    GncGraphQLResult *result = g_new0(GncGraphQLResult, 1);
    result->errors = NULL;
    result->extensions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    result->has_errors = FALSE;
    
    // Route to appropriate mutation resolver
    if (g_strcmp0(parsed->operation_type, "mutation") == 0) {
        if (g_strstr_len(parsed->selection_set, -1, "processTask")) {
            result->data = gnc_graphql_mutate_process_task(mutation->variables, mutation->context);
        } else if (g_strstr_len(parsed->selection_set, -1, "registerAgent")) {
            result->data = gnc_graphql_mutate_register_agent(mutation->variables, mutation->context);
        } else if (g_strstr_len(parsed->selection_set, -1, "updateAttention")) {
            result->data = gnc_graphql_mutate_update_attention(mutation->variables, mutation->context);
        } else if (g_strstr_len(parsed->selection_set, -1, "submitTransaction")) {
            result->data = gnc_graphql_mutate_submit_transaction(mutation->variables, mutation->context);
        } else {
            result->data = g_strdup("{\"processTask\":{\"id\":\"task_001\",\"status\":\"processing\"}}");
        }
    } else {
        result->has_errors = TRUE;
        result->errors = g_list_append(result->errors, g_strdup("Invalid operation type for mutation"));
        result->data = g_strdup("null");
    }
    
    free_parsed_query(parsed);
    return result;
}

/** Create GraphQL subscription */
gboolean gnc_graphql_create_subscription(const GncGraphQLSubscription *subscription)
{
    if (!subscription || !subscription->subscription_id || !subscription->query) {
        return FALSE;
    }
    
    g_mutex_lock(&g_graphql_state.graphql_mutex);
    
    GncGraphQLSubscription *copy = g_new0(GncGraphQLSubscription, 1);
    copy->subscription_id = g_strdup(subscription->subscription_id);
    copy->query = g_strdup(subscription->query);
    copy->connection_id = g_strdup(subscription->connection_id);
    copy->is_active = TRUE;
    copy->created_time = g_get_real_time();
    
    if (subscription->variables) {
        copy->variables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, subscription->variables);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            g_hash_table_insert(copy->variables, g_strdup((gchar*)key), g_strdup((gchar*)value));
        }
    }
    
    g_hash_table_insert(g_graphql_state.subscriptions, g_strdup(subscription->subscription_id), copy);
    
    g_mutex_unlock(&g_graphql_state.graphql_mutex);
    return TRUE;
}

/** Cancel GraphQL subscription */
gboolean gnc_graphql_cancel_subscription(const gchar *subscription_id)
{
    if (!subscription_id) {
        return FALSE;
    }
    
    g_mutex_lock(&g_graphql_state.graphql_mutex);
    gboolean removed = g_hash_table_remove(g_graphql_state.subscriptions, subscription_id);
    g_mutex_unlock(&g_graphql_state.graphql_mutex);
    
    return removed;
}

/** Query Resolvers */

gchar* gnc_graphql_resolve_cognitive_state(GHashTable *args, gpointer context)
{
#ifdef HAVE_JSON_GLIB
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    
    json_builder_set_member_name(builder, "cognitiveState");
    json_builder_begin_object(builder);
    
    json_builder_set_member_name(builder, "status");
    json_builder_add_string_value(builder, "active");
    
    json_builder_set_member_name(builder, "timestamp");
    json_builder_add_string_value(builder, "2024-01-01T00:00:00Z");
    
    json_builder_set_member_name(builder, "attention");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "totalSti");
    json_builder_add_double_value(builder, 750.0);
    json_builder_set_member_name(builder, "totalLti");
    json_builder_add_double_value(builder, 400.0);
    json_builder_end_object(builder);
    
    json_builder_set_member_name(builder, "tensorNetwork");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "status");
    json_builder_add_string_value(builder, "active");
    json_builder_set_member_name(builder, "nodes");
    json_builder_add_int_value(builder, 4);
    json_builder_end_object(builder);
    
    json_builder_set_member_name(builder, "cognitiveLoad");
    json_builder_add_double_value(builder, 0.65);
    
    json_builder_set_member_name(builder, "activeAgents");
    json_builder_add_int_value(builder, 3);
    
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    
    JsonGenerator *generator = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    gchar *json_string = json_generator_to_data(generator, NULL);
    
    json_node_free(root);
    g_object_unref(generator);
    g_object_unref(builder);

    return json_string;
#else
    return g_strdup_printf(
        "{\"cognitiveState\":{\"status\":\"active\",\"timestamp\":\"2024-01-01T00:00:00Z\","
        "\"attention\":{\"totalSti\":750.0,\"totalLti\":400.0},"
        "\"tensorNetwork\":{\"status\":\"active\",\"nodes\":4},"
        "\"cognitiveLoad\":0.65,\"activeAgents\":3}}");
#endif
}

gchar* gnc_graphql_resolve_attention_allocation(GHashTable *args, gpointer context)
{
#ifdef HAVE_JSON_GLIB
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    
    json_builder_set_member_name(builder, "attentionAllocation");
    json_builder_begin_object(builder);
    
    json_builder_set_member_name(builder, "totalSti");
    json_builder_add_double_value(builder, 750.0);
    
    json_builder_set_member_name(builder, "totalLti");
    json_builder_add_double_value(builder, 400.0);
    
    json_builder_set_member_name(builder, "nodeAllocations");
    json_builder_begin_array(builder);
    
    const gchar *node_types[] = {"MEMORY", "TASK", "AI", "AUTONOMY"};
    const gchar *node_ids[] = {"memory", "task", "ai", "autonomy"};
    gdouble sti_values[] = {200.0, 180.0, 220.0, 150.0};
    gdouble lti_values[] = {100.0, 90.0, 120.0, 90.0};
    
    for (int i = 0; i < 4; i++) {
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "nodeId");
        json_builder_add_string_value(builder, node_ids[i]);
        json_builder_set_member_name(builder, "nodeType");
        json_builder_add_string_value(builder, node_types[i]);
        json_builder_set_member_name(builder, "sti");
        json_builder_add_double_value(builder, sti_values[i]);
        json_builder_set_member_name(builder, "lti");
        json_builder_add_double_value(builder, lti_values[i]);
        json_builder_set_member_name(builder, "vlti");
        json_builder_add_double_value(builder, lti_values[i] * 0.5);
        json_builder_set_member_name(builder, "confidence");
        json_builder_add_double_value(builder, 0.85);
        json_builder_set_member_name(builder, "activityLevel");
        json_builder_add_double_value(builder, 0.7 + i * 0.05);
        json_builder_end_object(builder);
    }
    
    json_builder_end_array(builder);
    
    json_builder_set_member_name(builder, "stiFundBalance");
    json_builder_add_double_value(builder, 250.0);
    
    json_builder_set_member_name(builder, "ltiFundBalance");
    json_builder_add_double_value(builder, 100.0);
    
    json_builder_set_member_name(builder, "efficiency");
    json_builder_add_double_value(builder, 0.82);
    
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    
    JsonGenerator *generator = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    gchar *json_string = json_generator_to_data(generator, NULL);
    
    json_node_free(root);
    g_object_unref(generator);
    g_object_unref(builder);

    return json_string;
#else
    const gchar *node_types[] = {"MEMORY", "TASK", "AI", "AUTONOMY"};
    const gchar *node_ids[] = {"memory", "task", "ai", "autonomy"};
    gdouble sti_values[] = {200.0, 180.0, 220.0, 150.0};
    gdouble lti_values[] = {100.0, 90.0, 120.0, 90.0};

    GString *allocations = g_string_new(NULL);
    for (int i = 0; i < 4; i++) {
        if (i > 0)
            g_string_append_c(allocations, ',');
        g_string_append_printf(allocations,
            "{\"nodeId\":\"%s\",\"nodeType\":\"%s\",\"sti\":%g,\"lti\":%g,"
            "\"vlti\":%g,\"confidence\":0.85,\"activityLevel\":%g}",
            node_ids[i], node_types[i], sti_values[i], lti_values[i],
            lti_values[i] * 0.5, 0.7 + i * 0.05);
    }

    gchar *json_string = g_strdup_printf(
        "{\"attentionAllocation\":{\"totalSti\":750.0,\"totalLti\":400.0,"
        "\"nodeAllocations\":[%s],\"stiFundBalance\":250.0,\"ltiFundBalance\":100.0,"
        "\"efficiency\":0.82}}",
        allocations->str);
    g_string_free(allocations, TRUE);

    return json_string;
#endif
}

gchar* gnc_graphql_resolve_tensor_network(GHashTable *args, gpointer context)
{
    return g_strdup("{\"tensorNetwork\":{\"status\":\"active\",\"nodes\":[{\"id\":\"memory\",\"type\":\"MEMORY\",\"active\":true,\"attentionWeight\":0.25}],\"connections\":12,\"processingLoad\":0.65}}");
}

gchar* gnc_graphql_resolve_account(GHashTable *args, gpointer context)
{
    return g_strdup("{\"account\":{\"id\":\"acc_001\",\"name\":\"Cognitive Checking\",\"type\":\"BANK\",\"balance\":1000.0,\"cognitiveType\":\"ADAPTIVE\"}}");
}

gchar* gnc_graphql_resolve_transaction(GHashTable *args, gpointer context)
{
    return g_strdup("{\"transaction\":{\"id\":\"txn_001\",\"description\":\"Cognitive Transfer\",\"amount\":100.0,\"timestamp\":\"2024-01-01T00:00:00Z\"}}");
}

gchar* gnc_graphql_resolve_patterns(GHashTable *args, gpointer context)
{
    return g_strdup("{\"patterns\":[{\"id\":\"pat_001\",\"type\":\"financial_flow\",\"confidence\":0.95,\"frequency\":12,\"emergenceScore\":0.8}]}");
}

gchar* gnc_graphql_resolve_agents(GHashTable *args, gpointer context)
{
    return g_strdup("{\"agents\":[{\"id\":\"agent_001\",\"type\":\"UNITY3D\",\"status\":\"active\",\"capabilities\":[\"visualization\"],\"cognitiveLoad\":0.3}]}");
}

/** Mutation Resolvers */

gchar* gnc_graphql_mutate_process_task(GHashTable *args, gpointer context)
{
    return g_strdup("{\"processTask\":{\"id\":\"task_001\",\"type\":\"pattern_recognition\",\"status\":\"processing\",\"processingTime\":0.15}}");
}

gchar* gnc_graphql_mutate_register_agent(GHashTable *args, gpointer context)
{
    return g_strdup("{\"registerAgent\":{\"id\":\"agent_002\",\"type\":\"WEB_AGENT\",\"status\":\"registered\",\"capabilities\":[\"interaction\"]}}");
}

gchar* gnc_graphql_mutate_update_attention(GHashTable *args, gpointer context)
{
    return g_strdup("{\"updateAttention\":{\"totalSti\":800.0,\"totalLti\":450.0,\"efficiency\":0.88}}");
}

gchar* gnc_graphql_mutate_submit_transaction(GHashTable *args, gpointer context)
{
    return g_strdup("{\"submitTransaction\":{\"id\":\"txn_002\",\"description\":\"GraphQL Transaction\",\"amount\":250.0,\"timestamp\":\"2024-01-01T00:00:00Z\"}}");
}

/** Utility Functions */

GncGraphQLQuery* gnc_graphql_create_query(const gchar *query_string, GHashTable *variables, const gchar *operation_name)
{
    if (!query_string) {
        return NULL;
    }
    
    GncGraphQLQuery *query = g_new0(GncGraphQLQuery, 1);
    query->query = g_strdup(query_string);
    
    if (variables) {
        query->variables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, variables);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            g_hash_table_insert(query->variables, g_strdup((gchar*)key), g_strdup((gchar*)value));
        }
    }
    
    if (operation_name) {
        query->operation_name = g_strdup(operation_name);
    }
    
    return query;
}

void gnc_graphql_free_query(GncGraphQLQuery *query)
{
    if (!query) return;
    
    g_free(query->query);
    g_free(query->operation_name);
    if (query->variables) {
        g_hash_table_destroy(query->variables);
    }
    g_free(query);
}

GncGraphQLSubscription* gnc_graphql_create_subscription_context(const gchar *query_string, GHashTable *variables, const gchar *connection_id)
{
    if (!query_string || !connection_id) {
        return NULL;
    }
    
    GncGraphQLSubscription *subscription = g_new0(GncGraphQLSubscription, 1);
    subscription->subscription_id = g_uuid_string_random();
    subscription->query = g_strdup(query_string);
    subscription->connection_id = g_strdup(connection_id);
    subscription->is_active = TRUE;
    subscription->created_time = g_get_real_time();
    
    if (variables) {
        subscription->variables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, variables);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            g_hash_table_insert(subscription->variables, g_strdup((gchar*)key), g_strdup((gchar*)value));
        }
    }
    
    return subscription;
}

void gnc_graphql_free_subscription(GncGraphQLSubscription *subscription)
{
    if (!subscription) return;
    
    g_free(subscription->subscription_id);
    g_free(subscription->query);
    g_free(subscription->connection_id);
    if (subscription->variables) {
        g_hash_table_destroy(subscription->variables);
    }
    g_free(subscription);
}

void gnc_graphql_free_result(GncGraphQLResult *result)
{
    if (!result) return;
    
    g_free(result->data);
    if (result->errors) {
        g_list_free_full(result->errors, g_free);
    }
    if (result->extensions) {
        g_hash_table_destroy(result->extensions);
    }
    g_free(result);
}

gchar* gnc_graphql_result_to_json(const GncGraphQLResult *result)
{
    if (!result) {
        return g_strdup("{\"data\":null,\"errors\":[{\"message\":\"No result\"}]}");
    }

#ifndef HAVE_JSON_GLIB
    /* result->data is already a JSON text (produced by the resolver
     * functions above), so it can be spliced in directly rather than
     * re-parsed/re-serialized through JsonBuilder. */
    GString *errors = g_string_new(NULL);
    if (result->has_errors && result->errors) {
        gboolean first = TRUE;
        for (GList *l = result->errors; l != NULL; l = l->next) {
            if (!first)
                g_string_append_c(errors, ',');
            gchar *escaped_message = gnc_cognitive_json_escape_string((gchar*)l->data);
            g_string_append_printf(errors, "{\"message\":\"%s\"}", escaped_message);
            g_free(escaped_message);
            first = FALSE;
        }
    }

    gchar *json_string = g_strdup_printf("{\"data\":%s,\"errors\":[%s]}",
        result->data ? result->data : "null", errors->str);
    g_string_free(errors, TRUE);
    return json_string;
#else
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    
    // Add data
    if (result->data) {
        JsonParser *parser = json_parser_new();
        if (json_parser_load_from_data(parser, result->data, -1, NULL)) {
            JsonNode *data_node = json_parser_get_root(parser);
            json_builder_set_member_name(builder, "data");
            json_builder_add_value(builder, json_node_copy(data_node));
        } else {
            json_builder_set_member_name(builder, "data");
            json_builder_add_null_value(builder);
        }
        g_object_unref(parser);
    } else {
        json_builder_set_member_name(builder, "data");
        json_builder_add_null_value(builder);
    }
    
    // Add errors
    if (result->has_errors && result->errors) {
        json_builder_set_member_name(builder, "errors");
        json_builder_begin_array(builder);
        
        for (GList *l = result->errors; l != NULL; l = l->next) {
            json_builder_begin_object(builder);
            json_builder_set_member_name(builder, "message");
            json_builder_add_string_value(builder, (gchar*)l->data);
            json_builder_end_object(builder);
        }
        
        json_builder_end_array(builder);
    }
    
    json_builder_end_object(builder);
    
    JsonGenerator *generator = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    gchar *json_string = json_generator_to_data(generator, NULL);
    
    json_node_free(root);
    g_object_unref(generator);
    g_object_unref(builder);

    return json_string;
#endif
}

/** Subscription resolver stubs */
gboolean gnc_graphql_subscribe_cognitive_state(const GncGraphQLSubscription *subscription) { return TRUE; }
gboolean gnc_graphql_subscribe_attention_changes(const GncGraphQLSubscription *subscription) { return TRUE; }
gboolean gnc_graphql_subscribe_transaction_events(const GncGraphQLSubscription *subscription) { return TRUE; }
gboolean gnc_graphql_subscribe_pattern_events(const GncGraphQLSubscription *subscription) { return TRUE; }