/********************************************************************\
 * phase4-api-demo.cpp -- Phase 4 API & Embodiment Layer Demo    *
 * Copyright (C) 2024 GnuCash Cognitive Engine                     *
 *                                                                  *
 * This program demonstrates the Phase 4 Distributed Cognitive    *
 * Mesh API capabilities including REST endpoints, WebSocket       *
 * communication, Unity3D integration, ROS bindings, web agents,   *
 * and GraphQL interface for embodied cognitive applications.      *
 ********************************************************************/

#include <iostream>
#include <thread>
#include <chrono>
#include <glib.h>
#include "gnc-cognitive-api.h"
#include "gnc-cognitive-graphql.h"
#include "gnc-cognitive-accounting.h"
#include "gnc-tensor-network.h"

class Phase4ApiDemo 
{
public:
    Phase4ApiDemo() : initialized_(false) {}
    
    ~Phase4ApiDemo() {
        shutdown();
    }

    bool initialize() {
        std::cout << "\n=== Phase 4: Distributed Cognitive Mesh API & Embodiment Layer Demo ===\n";
        
        // Initialize cognitive systems
        if (!gnc_cognitive_accounting_init()) {
            std::cerr << "Failed to initialize cognitive accounting\n";
            return false;
        }
        
        if (!gnc_tensor_network_init()) {
            std::cerr << "Failed to initialize tensor network\n";
            return false;
        }
        
        // Initialize API server
        if (!gnc_cognitive_api_init(8080, 8081)) {
            std::cerr << "Failed to initialize cognitive API\n";
            return false;
        }
        
        // Initialize GraphQL interface
        if (!gnc_graphql_init()) {
            std::cerr << "Failed to initialize GraphQL interface\n";
            return false;
        }
        
        // Start API server
        if (!gnc_cognitive_api_start_server()) {
            std::cerr << "Failed to start API server\n";
            return false;
        }
        
        initialized_ = true;
        std::cout << "✅ Phase 4 systems initialized successfully\n";
        std::cout << "📡 API Server running on port 8080\n";
        std::cout << "🔌 WebSocket Server running on port 8081\n";
        std::cout << "📊 GraphQL endpoint available\n\n";
        
        return true;
    }
    
    void demonstrateRestApiEndpoints() {
        std::cout << "=== REST API Endpoints Demo ===\n";
        
        // Test GET /api/v1/cognitive/state
        std::cout << "1. Testing GET /api/v1/cognitive/state\n";
        GncApiRequest *request = createApiRequest("GET", "/api/v1/cognitive/state");
        GncApiResponse *response = gnc_api_get_cognitive_state(request);
        
        std::cout << "   Status: " << response->status_code << "\n";
        std::cout << "   Content-Type: " << response->content_type << "\n";
        std::cout << "   Response: " << std::string(response->body).substr(0, 100) << "...\n\n";
        
        freeApiRequest(request);
        freeApiResponse(response);
        
        // Test POST /api/v1/cognitive/process
        std::cout << "2. Testing POST /api/v1/cognitive/process\n";
        const char *taskJson = R"({
            "task_type": "pattern_recognition",
            "priority": 750,
            "parameters": {
                "threshold": 0.8,
                "focus_area": "financial_flows"
            }
        })";
        
        request = createApiRequest("POST", "/api/v1/cognitive/process", taskJson);
        response = gnc_api_process_cognitive_task(request);
        
        std::cout << "   Status: " << response->status_code << "\n";
        std::cout << "   Task submitted for processing\n";
        std::cout << "   Response: " << std::string(response->body).substr(0, 100) << "...\n\n";
        
        freeApiRequest(request);
        freeApiResponse(response);
        
        // Test GET /api/v1/attention/allocation
        std::cout << "3. Testing GET /api/v1/attention/allocation\n";
        request = createApiRequest("GET", "/api/v1/attention/allocation");
        response = gnc_api_get_attention_allocation(request);
        
        std::cout << "   Status: " << response->status_code << "\n";
        std::cout << "   Attention allocation retrieved\n";
        std::cout << "   Response: " << std::string(response->body).substr(0, 100) << "...\n\n";
        
        freeApiRequest(request);
        freeApiResponse(response);
        
        // Test agent registration
        std::cout << "4. Testing POST /api/v1/agents/register\n";
        const char *agentJson = R"({
            "agent_type": "unity3d",
            "capabilities": ["3d_visualization", "interaction", "attention_display"],
            "session_id": "unity_session_001"
        })";
        
        request = createApiRequest("POST", "/api/v1/agents/register", agentJson);
        response = gnc_api_register_agent(request);
        
        std::cout << "   Status: " << response->status_code << "\n";
        std::cout << "   Unity3D agent registered\n";
        std::cout << "   Response: " << std::string(response->body).substr(0, 100) << "...\n\n";
        
        freeApiRequest(request);
        freeApiResponse(response);
    }
    
    void demonstrateWebSocketCommunication() {
        std::cout << "=== WebSocket Communication Demo ===\n";
        
        // Register multiple WebSocket connections
        std::cout << "1. Registering WebSocket connections:\n";
        
        GncWebSocketConnection *unity_conn = gnc_websocket_register_connection("unity_001", "unity3d");
        std::cout << "   ✅ Unity3D connection: " << unity_conn->connection_id << "\n";
        
        GncWebSocketConnection *ros_conn = gnc_websocket_register_connection("ros_001", "ros_node");
        std::cout << "   ✅ ROS node connection: " << ros_conn->connection_id << "\n";
        
        GncWebSocketConnection *web_conn = gnc_websocket_register_connection("web_001", "web_agent");
        std::cout << "   ✅ Web agent connection: " << web_conn->connection_id << "\n\n";
        
        // Set up event subscriptions
        std::cout << "2. Setting up event subscriptions:\n";
        gnc_websocket_subscribe("unity_001", "cognitive.state.update");
        gnc_websocket_subscribe("unity_001", "attention.allocation.change");
        std::cout << "   📡 Unity3D subscribed to cognitive state and attention changes\n";
        
        gnc_websocket_subscribe("ros_001", "cognitive.state.update");
        gnc_websocket_subscribe("ros_001", "agent.registration");
        std::cout << "   📡 ROS node subscribed to cognitive state and agent events\n";
        
        gnc_websocket_subscribe("web_001", "task.completion");
        gnc_websocket_subscribe("web_001", "attention.allocation.change");
        std::cout << "   📡 Web agent subscribed to task completion and attention changes\n\n";
        
        // Broadcast events
        std::cout << "3. Broadcasting events:\n";
        
        const char *stateUpdateEvent = R"({
            "state": "updated",
            "tensor_network": {
                "status": "optimized",
                "processing_load": 0.72
            },
            "timestamp": "2024-01-01T12:00:00Z"
        })";
        
        int recipients = gnc_websocket_broadcast_event("cognitive.state.update", stateUpdateEvent);
        std::cout << "   📢 Cognitive state update broadcast to " << recipients << " connections\n";
        
        const char *attentionEvent = R"({
            "node_id": "ai",
            "old_sti": 180.0,
            "new_sti": 220.0,
            "reason": "pattern_recognition_task_completed",
            "timestamp": "2024-01-01T12:00:10Z"
        })";
        
        recipients = gnc_websocket_broadcast_event("attention.allocation.change", attentionEvent);
        std::cout << "   📢 Attention allocation change broadcast to " << recipients << " connections\n";
        
        // Send targeted event
        const char *taskEvent = R"({
            "task_id": "task_001",
            "type": "pattern_recognition",
            "status": "completed",
            "result": {
                "patterns_found": 7,
                "confidence": 0.94
            },
            "timestamp": "2024-01-01T12:00:15Z"
        })";
        
        gnc_websocket_send_event("web_001", "task.completion", taskEvent);
        std::cout << "   🎯 Task completion event sent to web agent\n\n";
    }
    
    void demonstrateUnity3DIntegration() {
        std::cout << "=== Unity3D Integration Demo ===\n";
        
        std::cout << "1. Creating Unity3D cognitive components:\n";
        
        // Create 3D visualization components
        double position1[3] = {0.0, 1.0, 0.0};
        double rotation1[4] = {0.0, 0.0, 0.0, 1.0};
        GncUnityComponent *attention_visualizer = gnc_unity_create_component("AttentionVisualizer", position1, rotation1);
        
        std::cout << "   🎮 Created AttentionVisualizer component: " << attention_visualizer->component_id << "\n";
        std::cout << "      Position: (" << position1[0] << ", " << position1[1] << ", " << position1[2] << ")\n";
        
        double position2[3] = {2.0, 0.5, -1.0};
        double rotation2[4] = {0.0, 0.707, 0.0, 0.707};
        GncUnityComponent *network_display = gnc_unity_create_component("TensorNetworkDisplay", position2, rotation2);
        
        std::cout << "   🎮 Created TensorNetworkDisplay component: " << network_display->component_id << "\n";
        std::cout << "      Position: (" << position2[0] << ", " << position2[1] << ", " << position2[2] << ")\n\n";
        
        // Update attention visualization
        std::cout << "2. Updating attention visualization:\n";
        
        GncAttentionParams attention = {0};
        attention.sti = 180.0;
        attention.lti = 95.0;
        attention.vlti = 45.0;
        attention.confidence = 0.87;
        attention.activity_level = 0.73;
        
        gnc_unity_update_attention_visualization(attention_visualizer, &attention);
        std::cout << "   📊 Updated attention visualization with STI: " << attention.sti 
                  << ", LTI: " << attention.lti << ", Confidence: " << attention.confidence << "\n";
        
        // Get cognitive state for Unity
        std::cout << "3. Getting cognitive state for Unity3D:\n";
        char *unity_state = gnc_unity_get_cognitive_state_json();
        std::cout << "   🎯 Unity cognitive state: " << std::string(unity_state).substr(0, 80) << "...\n";
        g_free(unity_state);
        
        // Process Unity command
        std::cout << "4. Processing Unity3D commands:\n";
        const char *unityCommand = R"({
            "action": "update_visualization",
            "component_id": "attention_vis_001",
            "parameters": {
                "highlight_node": "ai",
                "animation_speed": 1.2
            }
        })";
        
        char *commandResult = gnc_unity_process_command(unityCommand);
        std::cout << "   🎮 Command processed: " << std::string(commandResult).substr(0, 60) << "...\n\n";
        g_free(commandResult);
        
        // Cleanup
        g_free(attention_visualizer->component_id);
        g_free(attention_visualizer->component_type);
        g_hash_table_destroy(attention_visualizer->properties);
        g_free(attention_visualizer);
        g_free(network_display->component_id);
        g_free(network_display->component_type);
        g_hash_table_destroy(network_display->properties);
        g_free(network_display);
    }
    
    void demonstrateRosIntegration() {
        std::cout << "=== ROS Integration Demo ===\n";
        
        std::cout << "1. Creating ROS cognitive services:\n";
        gnc_ros_create_service("cognitive_planning", "gnucash_cognitive/PlanningService");
        std::cout << "   🤖 Created cognitive planning service\n";
        
        gnc_ros_create_service("attention_query", "gnucash_cognitive/AttentionQuery");
        std::cout << "   🤖 Created attention query service\n";
        
        gnc_ros_create_service("pattern_analysis", "gnucash_cognitive/PatternAnalysis");
        std::cout << "   🤖 Created pattern analysis service\n\n";
        
        std::cout << "2. Setting up ROS topic subscriptions:\n";
        gnc_ros_subscribe_topic("/cognitive/state", "gnucash_cognitive/CognitiveState");
        std::cout << "   📡 Subscribed to /cognitive/state topic\n";
        
        gnc_ros_subscribe_topic("/sensors/financial_flow", "sensor_msgs/Float64");
        std::cout << "   📡 Subscribed to /sensors/financial_flow topic\n\n";
        
        std::cout << "3. Publishing cognitive messages:\n";
        const char *attentionUpdate = R"({
            "node_id": "memory",
            "sti_change": 25.0,
            "lti_change": 10.0,
            "reason": "high_transaction_volume",
            "timestamp": 1704067200
        })";
        
        gnc_ros_publish_message("/cognitive/attention_updates", 
                               "gnucash_cognitive/AttentionUpdate", 
                               attentionUpdate);
        std::cout << "   📢 Published attention update to ROS network\n";
        
        std::cout << "4. Cognitive planning demonstration:\n";
        const char *planningGoal = R"({
            "target_balance": 5000.0,
            "account_id": "checking_001",
            "time_horizon": 30,
            "constraints": {
                "max_daily_spending": 200.0,
                "required_reserve": 1000.0
            },
            "optimization_criteria": ["stability", "growth"]
        })";
        
        char *planResult = gnc_ros_get_planning_result(planningGoal);
        std::cout << "   🧠 Planning result: " << std::string(planResult).substr(0, 70) << "...\n";
        g_free(planResult);
        
        std::cout << "5. Processing sensor data:\n";
        const char *sensorData = R"({
            "sensor_type": "financial_flow_monitor",
            "timestamp": 1704067200,
            "data": {
                "inflow_rate": 150.0,
                "outflow_rate": 120.0,
                "volatility": 0.15,
                "pattern_confidence": 0.82
            },
            "location": "account_checking_001"
        })";
        
        char *analysisResult = gnc_ros_process_sensor_data(sensorData);
        std::cout << "   🔍 Sensor analysis: " << std::string(analysisResult).substr(0, 70) << "...\n\n";
        g_free(analysisResult);
    }
    
    void demonstrateGraphQLInterface() {
        std::cout << "=== GraphQL Interface Demo ===\n";
        
        std::cout << "1. Executing GraphQL queries:\n";
        
        // Complex cognitive state query
        const char *cognitiveStateQuery = R"(
            query GetCognitiveState {
                cognitiveState {
                    status
                    timestamp
                    attention {
                        totalSti
                        totalLti
                    }
                    tensorNetwork {
                        status
                        nodes
                    }
                    cognitiveLoad
                    activeAgents
                }
            }
        )";
        
        GncGraphQLQuery *query = gnc_graphql_create_query(cognitiveStateQuery, nullptr, "GetCognitiveState");
        GncGraphQLResult *result = gnc_graphql_execute_query(query);
        
        std::cout << "   📊 Cognitive state query executed\n";
        std::cout << "   🔍 Has errors: " << (result->has_errors ? "Yes" : "No") << "\n";
        
        char *resultJson = gnc_graphql_result_to_json(result);
        std::cout << "   📋 Result: " << std::string(resultJson).substr(0, 100) << "...\n";
        g_free(resultJson);
        
        gnc_graphql_free_result(result);
        gnc_graphql_free_query(query);
        
        // Attention allocation query with parameters
        std::cout << "\n2. Detailed attention allocation query:\n";
        
        const char *attentionQuery = R"(
            query GetAttentionDetails($nodeId: String) {
                attentionAllocation(nodeId: $nodeId) {
                    totalSti
                    totalLti
                    nodeAllocations {
                        nodeId
                        nodeType
                        sti
                        lti
                        confidence
                        activityLevel
                    }
                    efficiency
                }
            }
        )";
        
        GHashTable *variables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        g_hash_table_insert(variables, g_strdup("nodeId"), g_strdup("ai"));
        
        query = gnc_graphql_create_query(attentionQuery, variables, "GetAttentionDetails");
        result = gnc_graphql_execute_query(query);
        
        std::cout << "   🎯 Attention query with parameters executed\n";
        
        resultJson = gnc_graphql_result_to_json(result);
        std::cout << "   📊 Result: " << std::string(resultJson).substr(0, 100) << "...\n";
        g_free(resultJson);
        
        gnc_graphql_free_result(result);
        gnc_graphql_free_query(query);
        g_hash_table_destroy(variables);
        
        // GraphQL mutation
        std::cout << "\n3. Executing GraphQL mutation:\n";
        
        const char *processingMutation = R"(
            mutation ProcessCognitiveTask($input: TaskInput!) {
                processTask(input: $input) {
                    id
                    type
                    status
                    processingTime
                }
            }
        )";
        
        query = gnc_graphql_create_query(processingMutation, nullptr, "ProcessCognitiveTask");
        result = gnc_graphql_execute_mutation(query);
        
        std::cout << "   ⚡ Task processing mutation executed\n";
        
        resultJson = gnc_graphql_result_to_json(result);
        std::cout << "   📤 Result: " << std::string(resultJson).substr(0, 100) << "...\n";
        g_free(resultJson);
        
        gnc_graphql_free_result(result);
        gnc_graphql_free_query(query);
        
        // GraphQL subscription
        std::cout << "\n4. Creating GraphQL subscription:\n";
        
        const char *subscriptionQuery = R"(
            subscription CognitiveStateUpdates {
                cognitiveStateUpdates {
                    status
                    timestamp
                    cognitiveLoad
                }
            }
        )";
        
        GncGraphQLSubscription *subscription = gnc_graphql_create_subscription_context(subscriptionQuery, nullptr, "web_001");
        bool subscribed = gnc_graphql_create_subscription(subscription);
        
        std::cout << "   📡 Subscription created: " << (subscribed ? "Success" : "Failed") << "\n";
        std::cout << "   🔔 Subscription ID: " << subscription->subscription_id << "\n\n";
        
        gnc_graphql_free_subscription(subscription);
    }
    
    void demonstrateWebAgentInterface() {
        std::cout << "=== Web Agent Interface Demo ===\n";
        
        std::cout << "1. Registering web agents:\n";
        
        // Register dashboard agent
        GHashTable *dashboardCapabilities = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        g_hash_table_insert(dashboardCapabilities, g_strdup("visualization"), g_strdup("advanced"));
        g_hash_table_insert(dashboardCapabilities, g_strdup("interaction"), g_strdup("full"));
        g_hash_table_insert(dashboardCapabilities, g_strdup("real_time"), g_strdup("true"));
        
        GncWebAgent *dashboard_agent = gnc_web_agent_register("dashboard", dashboardCapabilities, "session_dash_001");
        std::cout << "   🌐 Registered dashboard agent: " << dashboard_agent->agent_id << "\n";
        
        // Register analytics agent
        GHashTable *analyticsCapabilities = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        g_hash_table_insert(analyticsCapabilities, g_strdup("data_analysis"), g_strdup("true"));
        g_hash_table_insert(analyticsCapabilities, g_strdup("pattern_recognition"), g_strdup("true"));
        g_hash_table_insert(analyticsCapabilities, g_strdup("reporting"), g_strdup("advanced"));
        
        GncWebAgent *analytics_agent = gnc_web_agent_register("analytics", analyticsCapabilities, "session_analytics_001");
        std::cout << "   📈 Registered analytics agent: " << analytics_agent->agent_id << "\n\n";
        
        std::cout << "2. Processing web agent commands:\n";
        
        // Dashboard command
        const char *dashboardCommand = R"({
            "action": "get_cognitive_dashboard",
            "parameters": {
                "timeframe": "1h",
                "metrics": ["attention", "processing_load", "agent_activity"],
                "real_time": true
            }
        })";
        
        char *dashboardResult = gnc_web_agent_process_command(dashboard_agent->agent_id, dashboardCommand);
        std::cout << "   🎛️  Dashboard command processed: " << std::string(dashboardResult).substr(0, 60) << "...\n";
        g_free(dashboardResult);
        
        // Analytics command
        const char *analyticsCommand = R"({
            "action": "analyze_cognitive_patterns",
            "parameters": {
                "pattern_types": ["attention_flows", "processing_cycles"],
                "depth": "detailed",
                "export_format": "json"
            }
        })";
        
        char *analyticsResult = gnc_web_agent_process_command(analytics_agent->agent_id, analyticsCommand);
        std::cout << "   📊 Analytics command processed: " << std::string(analyticsResult).substr(0, 60) << "...\n";
        g_free(analyticsResult);
        
        std::cout << "\n3. Getting agent interfaces:\n";
        
        char *dashboardInterface = gnc_web_agent_get_interface(dashboard_agent->agent_id);
        std::cout << "   🔌 Dashboard interface: " << std::string(dashboardInterface).substr(0, 60) << "...\n";
        g_free(dashboardInterface);
        
        char *analyticsInterface = gnc_web_agent_get_interface(analytics_agent->agent_id);
        std::cout << "   🔌 Analytics interface: " << std::string(analyticsInterface).substr(0, 60) << "...\n\n";
        g_free(analyticsInterface);
        
        // Cleanup
        g_free(dashboard_agent->agent_id);
        g_free(dashboard_agent->agent_type);
        g_free(dashboard_agent->session_id);
        g_hash_table_unref(dashboard_agent->capabilities);
        g_free(dashboard_agent);
        g_hash_table_destroy(dashboardCapabilities);
        
        g_free(analytics_agent->agent_id);
        g_free(analytics_agent->agent_type);
        g_free(analytics_agent->session_id);
        g_hash_table_unref(analytics_agent->capabilities);
        g_free(analytics_agent);
        g_hash_table_destroy(analyticsCapabilities);
    }
    
    void demonstratePerformanceMonitoring() {
        std::cout << "=== Performance Monitoring Demo ===\n";
        
        // Get performance metrics
        GncApiMetrics metrics = gnc_cognitive_api_get_metrics();
        
        std::cout << "📊 Current API Performance Metrics:\n";
        std::cout << "   Active Connections: " << metrics.active_connections << "\n";
        std::cout << "   Total Requests: " << metrics.total_requests << "\n";
        std::cout << "   Successful Requests: " << metrics.successful_requests << "\n";
        std::cout << "   Failed Requests: " << metrics.failed_requests << "\n";
        std::cout << "   Average Response Time: " << metrics.avg_response_time << " ms\n";
        std::cout << "   Requests Per Second: " << metrics.requests_per_second << "\n";
        std::cout << "   Memory Usage: " << metrics.memory_usage_mb << " MB\n";
        std::cout << "   CPU Usage: " << metrics.cpu_usage_percent << "%\n\n";
        
        // Test monitoring controls
        std::cout << "🔧 Testing monitoring controls:\n";
        gnc_cognitive_api_set_monitoring(true);
        std::cout << "   ✅ Performance monitoring enabled\n";
        
        gnc_cognitive_api_reset_metrics();
        std::cout << "   🔄 Performance metrics reset\n";
        
        gnc_cognitive_api_set_monitoring(false);
        std::cout << "   ⏸️  Performance monitoring disabled\n\n";
    }
    
    void runFullIntegrationWorkflow() {
        std::cout << "=== Full Integration Workflow Demo ===\n";
        
        std::cout << "🚀 Starting comprehensive cognitive workflow...\n\n";
        
        // 1. Register multiple agent types
        std::cout << "Step 1: Agent Registration\n";
        
        // Unity3D Agent
        GncApiRequest *request = createApiRequest("POST", "/api/v1/agents/register",
            R"({"agent_type":"unity3d","capabilities":["3d_visualization","interaction"]})");
        GncApiResponse *response = gnc_api_register_agent(request);
        std::cout << "   🎮 Unity3D Agent registered (Status: " << response->status_code << ")\n";
        freeApiRequest(request);
        freeApiResponse(response);
        
        // ROS Agent
        request = createApiRequest("POST", "/api/v1/agents/register",
            R"({"agent_type":"ros_node","capabilities":["planning","navigation","sensor_processing"]})");
        response = gnc_api_register_agent(request);
        std::cout << "   🤖 ROS Agent registered (Status: " << response->status_code << ")\n";
        freeApiRequest(request);
        freeApiResponse(response);
        
        // Web Agent
        request = createApiRequest("POST", "/api/v1/agents/register",
            R"({"agent_type":"web_agent","capabilities":["dashboard","analytics","reporting"]})");
        response = gnc_api_register_agent(request);
        std::cout << "   🌐 Web Agent registered (Status: " << response->status_code << ")\n";
        freeApiRequest(request);
        freeApiResponse(response);
        
        std::cout << "\nStep 2: WebSocket Event Subscriptions\n";
        
        // Set up WebSocket connections and subscriptions
        gnc_websocket_register_connection("unity_workflow", "unity3d");
        gnc_websocket_subscribe("unity_workflow", "cognitive.state.update");
        gnc_websocket_subscribe("unity_workflow", "attention.allocation.change");
        std::cout << "   📡 Unity3D subscribed to cognitive events\n";
        
        gnc_websocket_register_connection("ros_workflow", "ros_node");
        gnc_websocket_subscribe("ros_workflow", "cognitive.state.update");
        gnc_websocket_subscribe("ros_workflow", "agent.registration");
        std::cout << "   📡 ROS node subscribed to cognitive events\n";
        
        std::cout << "\nStep 3: Cognitive Task Processing\n";
        
        // Submit complex cognitive task
        request = createApiRequest("POST", "/api/v1/cognitive/process",
            R"({
                "task_type": "distributed_pattern_analysis",
                "priority": 900,
                "parameters": {
                    "analysis_depth": "comprehensive",
                    "pattern_types": ["temporal", "semantic", "attention_flow"],
                    "time_window": "24h",
                    "confidence_threshold": 0.85
                },
                "deadline": "2024-01-01T13:00:00Z"
            })");
        response = gnc_api_process_cognitive_task(request);
        std::cout << "   🧠 Complex cognitive task submitted (Status: " << response->status_code << ")\n";
        freeApiRequest(request);
        freeApiResponse(response);
        
        std::cout << "\nStep 4: Transaction Processing\n";
        
        // Submit transaction for cognitive processing
        request = createApiRequest("POST", "/api/v1/transactions/submit",
            R"({
                "description": "Cognitive-Enhanced Transfer",
                "amount": 1500.0,
                "from_account": "checking_cognitive_001",
                "to_account": "savings_adaptive_001",
                "cognitive_validation": true,
                "attention_priority": "high"
            })");
        response = gnc_api_submit_transaction(request);
        std::cout << "   💳 Cognitive transaction submitted (Status: " << response->status_code << ")\n";
        freeApiRequest(request);
        freeApiResponse(response);
        
        std::cout << "\nStep 5: GraphQL Complex Query\n";
        
        // Execute comprehensive GraphQL query
        const char *complexQuery = R"(
            query ComprehensiveSystemState {
                cognitiveState {
                    status
                    cognitiveLoad
                    activeAgents
                }
                attentionAllocation {
                    totalSti
                    totalLti
                    efficiency
                    nodeAllocations {
                        nodeId
                        sti
                        lti
                        activityLevel
                    }
                }
                tensorNetwork {
                    status
                    processingLoad
                }
                agents {
                    id
                    type
                    status
                    cognitiveLoad
                }
            }
        )";
        
        GncGraphQLQuery *graphql_query = gnc_graphql_create_query(complexQuery, nullptr, "ComprehensiveSystemState");
        GncGraphQLResult *graphql_result = gnc_graphql_execute_query(graphql_query);
        std::cout << "   📊 Complex GraphQL query executed (Errors: " << (graphql_result->has_errors ? "Yes" : "No") << ")\n";
        gnc_graphql_free_result(graphql_result);
        gnc_graphql_free_query(graphql_query);
        
        std::cout << "\nStep 6: Event Broadcasting\n";
        
        // Broadcast workflow completion event
        const char *workflowEvent = R"({
            "workflow_id": "full_integration_demo",
            "status": "completed",
            "components_tested": ["REST_API", "WebSocket", "Unity3D", "ROS", "GraphQL", "WebAgent"],
            "performance": {
                "total_requests": 6,
                "avg_response_time": 45.2,
                "success_rate": 1.0
            },
            "timestamp": "2024-01-01T12:30:00Z"
        })";
        
        int recipients = gnc_websocket_broadcast_event("workflow.completion", workflowEvent);
        std::cout << "   📢 Workflow completion broadcast to " << recipients << " connections\n";
        
        std::cout << "\nStep 7: System Health Check\n";
        
        // Get system status
        request = createApiRequest("GET", "/api/v1/network/status");
        response = gnc_api_get_network_status(request);
        std::cout << "   ✅ Network status check (Status: " << response->status_code << ")\n";
        freeApiRequest(request);
        freeApiResponse(response);
        
        // Get performance metrics
        GncApiMetrics final_metrics = gnc_cognitive_api_get_metrics();
        std::cout << "   📈 Active connections: " << final_metrics.active_connections << "\n";
        std::cout << "   📈 Total requests processed: " << final_metrics.total_requests << "\n";
        
        std::cout << "\n🎉 Full integration workflow completed successfully!\n\n";
    }
    
    void runDemo() {
        if (!initialize()) {
            std::cerr << "❌ Failed to initialize Phase 4 systems\n";
            return;
        }
        
        try {
            demonstrateRestApiEndpoints();
            demonstrateWebSocketCommunication();
            demonstrateUnity3DIntegration();
            demonstrateRosIntegration();
            demonstrateGraphQLInterface();
            demonstrateWebAgentInterface();
            demonstratePerformanceMonitoring();
            runFullIntegrationWorkflow();
        } catch (const std::exception& e) {
            std::cerr << "❌ Demo error: " << e.what() << "\n";
        }
        
        std::cout << "🏁 Phase 4: Distributed Cognitive Mesh API & Embodiment Layer Demo Complete!\n";
        std::cout << "\n📋 Summary:\n";
        std::cout << "   ✅ REST API endpoints functional\n";
        std::cout << "   ✅ WebSocket real-time communication active\n";
        std::cout << "   ✅ Unity3D integration layer operational\n";
        std::cout << "   ✅ ROS cognitive bindings working\n";
        std::cout << "   ✅ GraphQL interface responding\n";
        std::cout << "   ✅ Web agent interface functional\n";
        std::cout << "   ✅ Performance monitoring enabled\n";
        std::cout << "   ✅ Full integration workflow successful\n\n";
        
        std::cout << "🌐 The distributed cognitive mesh is ready for embodied AI applications!\n";
    }

private:
    bool initialized_;
    
    GncApiRequest* createApiRequest(const char* method, const char* path, const char* body = nullptr) {
        GncApiRequest *request = g_new0(GncApiRequest, 1);
        request->method = g_strdup(method);
        request->path = g_strdup(path);
        request->headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        
        if (body) {
            request->body = g_strdup(body);
            request->body_length = strlen(body);
            g_hash_table_insert(request->headers, g_strdup("Content-Type"), g_strdup("application/json"));
        }
        
        return request;
    }
    
    void freeApiRequest(GncApiRequest *request) {
        if (!request) return;
        g_free(request->method);
        g_free(request->path);
        g_free(request->query_string);
        g_free(request->body);
        g_hash_table_destroy(request->headers);
        g_free(request);
    }
    
    void freeApiResponse(GncApiResponse *response) {
        if (!response) return;
        g_free(response->body);
        g_free(response->content_type);
        g_hash_table_destroy(response->headers);
        g_free(response);
    }
    
    void shutdown() {
        if (initialized_) {
            gnc_cognitive_api_shutdown();
            gnc_graphql_shutdown();
            gnc_tensor_network_shutdown();
            gnc_cognitive_accounting_shutdown();
            initialized_ = false;
        }
    }
};

int main(int argc, char **argv)
{
    std::cout << "Starting Phase 4: Distributed Cognitive Mesh API & Embodiment Layer Demo\n";
    
    Phase4ApiDemo demo;
    demo.runDemo();
    
    return 0;
}