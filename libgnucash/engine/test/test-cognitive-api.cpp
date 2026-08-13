/********************************************************************\
 * test-cognitive-api.cpp -- Phase 4 API Integration Tests        *
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

#include <gtest/gtest.h>
#include <glib.h>
#include <json-glib/json-glib.h>
#include "gnc-cognitive-api.h"
#include "gnc-cognitive-accounting.h"
#include "gnc-tensor-network.h"

class CognitiveApiTest : public ::testing::Test 
{
protected:
    void SetUp() override 
    {
        // Initialize cognitive systems
        gnc_cognitive_accounting_init();
        gnc_tensor_network_init();
        
        // Initialize API server
        ASSERT_TRUE(gnc_cognitive_api_init(8080, 8081));
        ASSERT_TRUE(gnc_cognitive_api_start_server());
    }

    void TearDown() override 
    {
        gnc_cognitive_api_shutdown();
        gnc_tensor_network_shutdown();
        gnc_cognitive_accounting_shutdown();
    }

    GncApiRequest* create_test_request(const gchar *method, const gchar *path, 
                                      const gchar *body = nullptr)
    {
        GncApiRequest *request = g_new0(GncApiRequest, 1);
        request->method = g_strdup(method);
        request->path = g_strdup(path);
        request->headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        
        if (body) {
            request->body = g_strdup(body);
            request->body_length = strlen(body);
            g_hash_table_insert(request->headers, g_strdup("Content-Type"), 
                              g_strdup("application/json"));
        }
        
        return request;
    }

    void free_test_request(GncApiRequest *request)
    {
        if (!request) return;
        g_free(request->method);
        g_free(request->path);
        g_free(request->query_string);
        g_free(request->body);
        g_hash_table_destroy(request->headers);
        g_free(request);
    }

    void free_api_response(GncApiResponse *response)
    {
        if (!response) return;
        g_free(response->body);
        g_free(response->content_type);
        g_hash_table_destroy(response->headers);
        g_free(response);
    }

    gboolean is_valid_json(const gchar *json_string)
    {
        JsonParser *parser = json_parser_new();
        GError *error = nullptr;
        gboolean valid = json_parser_load_from_data(parser, json_string, -1, &error);
        g_object_unref(parser);
        return valid;
    }

    JsonObject* parse_json_response(const gchar *json_string)
    {
        JsonParser *parser = json_parser_new();
        json_parser_load_from_data(parser, json_string, -1, nullptr);
        JsonNode *root = json_parser_get_root(parser);
        JsonObject *object = json_node_dup_object(root);
        g_object_unref(parser);
        return object;
    }
};

/** Test REST API Endpoints */

TEST_F(CognitiveApiTest, TestGetCognitiveState)
{
    // Test GET /api/v1/cognitive/state
    GncApiRequest *request = create_test_request("GET", "/api/v1/cognitive/state");
    GncApiResponse *response = gnc_api_get_cognitive_state(request);
    
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->status_code, GNC_API_STATUS_OK);
    EXPECT_STREQ(response->content_type, "application/json");
    EXPECT_TRUE(is_valid_json(response->body));
    
    // Verify response structure
    JsonObject *json_obj = parse_json_response(response->body);
    EXPECT_TRUE(json_object_has_member(json_obj, "status"));
    EXPECT_TRUE(json_object_has_member(json_obj, "attention"));
    EXPECT_TRUE(json_object_has_member(json_obj, "tensor_network"));
    
    const gchar *status = json_object_get_string_member(json_obj, "status");
    EXPECT_STREQ(status, "active");
    
    json_object_unref(json_obj);
    free_api_response(response);
    free_test_request(request);
}

TEST_F(CognitiveApiTest, TestProcessCognitiveTask)
{
    // Test POST /api/v1/cognitive/process
    const gchar *task_json = "{\"task_type\":\"pattern_recognition\",\"data\":{\"transactions\":[]}}";
    GncApiRequest *request = create_test_request("POST", "/api/v1/cognitive/process", task_json);
    GncApiResponse *response = gnc_api_process_cognitive_task(request);
    
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->status_code, GNC_API_STATUS_ACCEPTED);
    EXPECT_TRUE(is_valid_json(response->body));
    
    // Verify response structure
    JsonObject *json_obj = parse_json_response(response->body);
    EXPECT_TRUE(json_object_has_member(json_obj, "task_id"));
    EXPECT_TRUE(json_object_has_member(json_obj, "task_type"));
    EXPECT_TRUE(json_object_has_member(json_obj, "status"));
    
    const gchar *task_type = json_object_get_string_member(json_obj, "task_type");
    EXPECT_STREQ(task_type, "pattern_recognition");
    
    const gchar *status = json_object_get_string_member(json_obj, "status");
    EXPECT_STREQ(status, "processing");
    
    json_object_unref(json_obj);
    free_api_response(response);
    free_test_request(request);
}

TEST_F(CognitiveApiTest, TestGetAttentionAllocation)
{
    // Test GET /api/v1/attention/allocation
    GncApiRequest *request = create_test_request("GET", "/api/v1/attention/allocation");
    GncApiResponse *response = gnc_api_get_attention_allocation(request);
    
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->status_code, GNC_API_STATUS_OK);
    EXPECT_TRUE(is_valid_json(response->body));
    
    // Verify attention allocation structure
    JsonObject *json_obj = parse_json_response(response->body);
    EXPECT_TRUE(json_object_has_member(json_obj, "attention_allocation"));
    
    JsonObject *attention = json_object_get_object_member(json_obj, "attention_allocation");
    EXPECT_TRUE(json_object_has_member(attention, "total_sti_circulation"));
    EXPECT_TRUE(json_object_has_member(attention, "total_lti_circulation"));
    EXPECT_TRUE(json_object_has_member(attention, "node_allocations"));
    
    JsonArray *allocations = json_object_get_array_member(attention, "node_allocations");
    EXPECT_EQ(static_cast<int>(json_array_get_length(allocations)), 4); // Memory, Task, AI, Autonomy nodes
    
    json_object_unref(json_obj);
    free_api_response(response);
    free_test_request(request);
}

TEST_F(CognitiveApiTest, TestAgentRegistration)
{
    // Test POST /api/v1/agents/register
    const gchar *agent_json = "{\"agent_type\":\"unity3d\",\"capabilities\":[\"visualization\",\"interaction\"]}";
    GncApiRequest *request = create_test_request("POST", "/api/v1/agents/register", agent_json);
    GncApiResponse *response = gnc_api_register_agent(request);
    
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->status_code, GNC_API_STATUS_CREATED);
    EXPECT_TRUE(is_valid_json(response->body));
    
    // Verify registration response
    JsonObject *json_obj = parse_json_response(response->body);
    EXPECT_TRUE(json_object_has_member(json_obj, "agent_id"));
    EXPECT_TRUE(json_object_has_member(json_obj, "agent_type"));
    EXPECT_TRUE(json_object_has_member(json_obj, "status"));
    
    const gchar *agent_type = json_object_get_string_member(json_obj, "agent_type");
    EXPECT_STREQ(agent_type, "unity3d");
    
    const gchar *status = json_object_get_string_member(json_obj, "status");
    EXPECT_STREQ(status, "registered");
    
    json_object_unref(json_obj);
    free_api_response(response);
    free_test_request(request);
}

TEST_F(CognitiveApiTest, TestMissingRequestBody)
{
    // Test POST request without body
    GncApiRequest *request = create_test_request("POST", "/api/v1/cognitive/process");
    GncApiResponse *response = gnc_api_process_cognitive_task(request);
    
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->status_code, GNC_API_STATUS_BAD_REQUEST);
    EXPECT_TRUE(is_valid_json(response->body));
    
    JsonObject *json_obj = parse_json_response(response->body);
    EXPECT_TRUE(json_object_has_member(json_obj, "error"));
    
    json_object_unref(json_obj);
    free_api_response(response);
    free_test_request(request);
}

/** Test WebSocket Communication */

TEST_F(CognitiveApiTest, TestWebSocketConnection)
{
    // Test WebSocket connection registration
    GncWebSocketConnection *conn = gnc_websocket_register_connection("conn_001", "web_agent");
    
    ASSERT_NE(conn, nullptr);
    EXPECT_STREQ(conn->connection_id, "conn_001");
    EXPECT_STREQ(conn->agent_type, "web_agent");
    EXPECT_TRUE(conn->is_active);
    EXPECT_GT(conn->connect_time, 0);
    
    // Test subscription
    EXPECT_TRUE(gnc_websocket_subscribe("conn_001", "cognitive.state.update"));
    EXPECT_TRUE(gnc_websocket_subscribe("conn_001", "attention.allocation.change"));
    
    // Test unsubscription
    EXPECT_TRUE(gnc_websocket_unsubscribe("conn_001", "cognitive.state.update"));
    
    // Test connection removal
    EXPECT_TRUE(gnc_websocket_unregister_connection("conn_001"));
    EXPECT_FALSE(gnc_websocket_unregister_connection("conn_001")); // Should fail second time
}

TEST_F(CognitiveApiTest, TestWebSocketEventBroadcast)
{
    // Register multiple connections
    gnc_websocket_register_connection("conn_001", "unity3d");
    gnc_websocket_register_connection("conn_002", "ros_node");
    gnc_websocket_register_connection("conn_003", "web_agent");
    
    // Subscribe to events
    gnc_websocket_subscribe("conn_001", "cognitive.state.update");
    gnc_websocket_subscribe("conn_002", "cognitive.state.update");
    
    // Test broadcast
    const gchar *event_data = "{\"state\":\"updated\",\"timestamp\":1234567890}";
    gint recipients = gnc_websocket_broadcast_event("cognitive.state.update", event_data);
    
    // Should broadcast to all 3 connections (implementation returns total connections)
    EXPECT_EQ(recipients, 3);
    
    // Test targeted send
    EXPECT_TRUE(gnc_websocket_send_event("conn_001", "attention.allocation.change", event_data));
    EXPECT_FALSE(gnc_websocket_send_event("nonexistent_conn", "test.event", event_data));
}

/** Test Unity3D Integration */

TEST_F(CognitiveApiTest, TestUnity3DIntegration)
{
    ASSERT_TRUE(gnc_unity_init());
    
    // Test component creation
    gdouble position[3] = {1.0, 2.0, 3.0};
    gdouble rotation[4] = {0.0, 0.0, 0.0, 1.0};
    GncUnityComponent *component = gnc_unity_create_component("CognitiveVisualizer", position, rotation);
    
    ASSERT_NE(component, nullptr);
    EXPECT_STREQ(component->component_type, "CognitiveVisualizer");
    EXPECT_EQ(component->position[0], 1.0);
    EXPECT_EQ(component->position[1], 2.0);
    EXPECT_EQ(component->position[2], 3.0);
    EXPECT_EQ(component->rotation[3], 1.0);
    
    // Test attention visualization update
    GncAttentionParams attention = {0};
    attention.sti = 150.0;
    attention.lti = 75.0;
    attention.confidence = 0.85;
    
    EXPECT_TRUE(gnc_unity_update_attention_visualization(component, &attention));
    EXPECT_EQ(component->attention.sti, 150.0);
    EXPECT_EQ(component->attention.lti, 75.0);
    
    // Test cognitive state JSON
    gchar *state_json = gnc_unity_get_cognitive_state_json();
    ASSERT_NE(state_json, nullptr);
    EXPECT_TRUE(is_valid_json(state_json));
    g_free(state_json);
    
    // Test command processing
    const gchar *command = "{\"action\":\"update_visualization\",\"component_id\":\"comp_001\"}";
    gchar *response = gnc_unity_process_command(command);
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(is_valid_json(response));
    g_free(response);
    
    g_free(component->component_id);
    g_free(component->component_type);
    g_hash_table_destroy(component->properties);
    g_free(component);
}

/** Test ROS Integration */

TEST_F(CognitiveApiTest, TestRosIntegration)
{
    ASSERT_TRUE(gnc_ros_init("test_cognitive_node", "/gnucash/test"));
    
    // Test service creation
    EXPECT_TRUE(gnc_ros_create_service("cognitive_planning", "gnucash_cognitive/Planning"));
    EXPECT_TRUE(gnc_ros_create_service("attention_query", "gnucash_cognitive/AttentionQuery"));
    
    // Test topic operations
    EXPECT_TRUE(gnc_ros_subscribe_topic("/cognitive/state", "gnucash_cognitive/CognitiveState"));
    EXPECT_TRUE(gnc_ros_publish_message("/cognitive/attention", 
                                       "gnucash_cognitive/AttentionUpdate",
                                       "{\"sti_change\":50.0,\"node_id\":\"memory\"}"));
    
    // Test cognitive planning
    const gchar *goal = "{\"target_balance\":1000.0,\"account\":\"checking\",\"constraints\":[]}";
    gchar *plan_result = gnc_ros_get_planning_result(goal);
    ASSERT_NE(plan_result, nullptr);
    EXPECT_TRUE(is_valid_json(plan_result));
    
    JsonObject *plan_obj = parse_json_response(plan_result);
    EXPECT_TRUE(json_object_has_member(plan_obj, "plan"));
    EXPECT_TRUE(json_object_has_member(plan_obj, "status"));
    
    g_object_unref(plan_obj);
    g_free(plan_result);
    
    // Test sensor data processing
    const gchar *sensor_data = "{\"type\":\"financial_flow\",\"value\":150.0,\"timestamp\":1234567890}";
    gchar *analysis = gnc_ros_process_sensor_data(sensor_data);
    ASSERT_NE(analysis, nullptr);
    EXPECT_TRUE(is_valid_json(analysis));
    
    JsonObject *analysis_obj = parse_json_response(analysis);
    EXPECT_TRUE(json_object_has_member(analysis_obj, "analysis"));
    EXPECT_TRUE(json_object_has_member(analysis_obj, "confidence"));
    
    g_object_unref(analysis_obj);
    g_free(analysis);
}

/** Test Web Agent Interface */

TEST_F(CognitiveApiTest, TestWebAgentInterface)
{
    ASSERT_TRUE(gnc_web_agent_init());
    
    // Test agent registration
    GHashTable *capabilities = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(capabilities, g_strdup("visualization"), g_strdup("true"));
    g_hash_table_insert(capabilities, g_strdup("interaction"), g_strdup("true"));
    
    GncWebAgent *agent = gnc_web_agent_register("web_dashboard", capabilities, "session_123");
    
    ASSERT_NE(agent, nullptr);
    EXPECT_STREQ(agent->agent_type, "web_dashboard");
    EXPECT_STREQ(agent->session_id, "session_123");
    EXPECT_FALSE(agent->is_authenticated);
    EXPECT_GT(agent->last_activity, 0);
    
    // Test command processing
    const gchar *command = "{\"action\":\"get_cognitive_state\",\"filters\":[\"attention\"]}";
    gchar *result = gnc_web_agent_process_command(agent->agent_id, command);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(is_valid_json(result));
    g_free(result);
    
    // Test interface description
    gchar *interface_desc = gnc_web_agent_get_interface(agent->agent_id);
    ASSERT_NE(interface_desc, nullptr);
    EXPECT_TRUE(is_valid_json(interface_desc));
    
    JsonObject *interface_obj = parse_json_response(interface_desc);
    EXPECT_TRUE(json_object_has_member(interface_obj, "interface"));
    
    g_object_unref(interface_obj);
    g_free(interface_desc);
    
    // Test agent unregistration
    EXPECT_TRUE(gnc_web_agent_unregister(agent->agent_id));
    EXPECT_FALSE(gnc_web_agent_unregister(agent->agent_id)); // Should fail second time
    
    g_free(agent->agent_id);
    g_free(agent->agent_type);
    g_free(agent->session_id);
    g_hash_table_unref(agent->capabilities);
    g_free(agent);
    g_hash_table_destroy(capabilities);
}

/** Test Performance Monitoring */

TEST_F(CognitiveApiTest, TestPerformanceMonitoring)
{
    // Reset metrics
    gnc_cognitive_api_reset_metrics();
    
    // Get initial metrics
    GncApiMetrics metrics = gnc_cognitive_api_get_metrics();
    EXPECT_EQ(metrics.total_requests, 0);
    EXPECT_EQ(metrics.successful_requests, 0);
    EXPECT_EQ(metrics.failed_requests, 0);
    EXPECT_EQ(metrics.active_connections, 0);
    
    // Add some connections to test active count
    gnc_websocket_register_connection("perf_conn_1", "test_agent");
    gnc_websocket_register_connection("perf_conn_2", "test_agent");
    
    metrics = gnc_cognitive_api_get_metrics();
    EXPECT_EQ(metrics.active_connections, 2);
    
    // Test monitoring toggle
    gnc_cognitive_api_set_monitoring(TRUE);
    gnc_cognitive_api_set_monitoring(FALSE);
}

/** Integration Test: Full Workflow */

TEST_F(CognitiveApiTest, TestFullCognitiveWorkflow)
{
    // 1. Register agents
    const gchar *unity_agent_json = "{\"agent_type\":\"unity3d\",\"capabilities\":[\"3d_visualization\"]}";
    GncApiRequest *reg_request = create_test_request("POST", "/api/v1/agents/register", unity_agent_json);
    GncApiResponse *reg_response = gnc_api_register_agent(reg_request);
    EXPECT_EQ(reg_response->status_code, GNC_API_STATUS_CREATED);
    
    // 2. Get initial cognitive state
    GncApiRequest *state_request = create_test_request("GET", "/api/v1/cognitive/state");
    GncApiResponse *state_response = gnc_api_get_cognitive_state(state_request);
    EXPECT_EQ(state_response->status_code, GNC_API_STATUS_OK);
    
    // 3. Submit cognitive task
    const gchar *task_json = "{\"task_type\":\"attention_allocation\",\"parameters\":{\"focus\":\"financial_analysis\"}}";
    GncApiRequest *task_request = create_test_request("POST", "/api/v1/cognitive/process", task_json);
    GncApiResponse *task_response = gnc_api_process_cognitive_task(task_request);
    EXPECT_EQ(task_response->status_code, GNC_API_STATUS_ACCEPTED);
    
    // 4. Check attention allocation
    GncApiRequest *attention_request = create_test_request("GET", "/api/v1/attention/allocation");
    GncApiResponse *attention_response = gnc_api_get_attention_allocation(attention_request);
    EXPECT_EQ(attention_response->status_code, GNC_API_STATUS_OK);
    
    // 5. Submit transaction
    const gchar *txn_json = "{\"amount\":100.0,\"from_account\":\"checking\",\"to_account\":\"savings\"}";
    GncApiRequest *txn_request = create_test_request("POST", "/api/v1/transactions/submit", txn_json);
    GncApiResponse *txn_response = gnc_api_submit_transaction(txn_request);
    EXPECT_EQ(txn_response->status_code, GNC_API_STATUS_ACCEPTED);
    
    // 6. Get network status
    GncApiRequest *network_request = create_test_request("GET", "/api/v1/network/status");
    GncApiResponse *network_response = gnc_api_get_network_status(network_request);
    EXPECT_EQ(network_response->status_code, GNC_API_STATUS_OK);
    
    // Cleanup
    free_api_response(reg_response);
    free_test_request(reg_request);
    free_api_response(state_response);
    free_test_request(state_request);
    free_api_response(task_response);
    free_test_request(task_request);
    free_api_response(attention_response);
    free_test_request(attention_request);
    free_api_response(txn_response);
    free_test_request(txn_request);
    free_api_response(network_response);
    free_test_request(network_request);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}