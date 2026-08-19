# Phase 4: Distributed Cognitive Mesh API Documentation

> **Honesty banner (v3.0):** This document describes the **target design** and
> the **in-process library API**. There is **no** listen/accept HTTP or
> WebSocket server in-tree. Unity3D and ROS sections are **experimental /
> deferred** adapter sketches — do not treat them as shipped product features.
> See [COGNITIVE_CAPABILITY_MATRIX.md](COGNITIVE_CAPABILITY_MATRIX.md) and
> [PHASE4_IMPLEMENTATION_SUMMARY.md](PHASE4_IMPLEMENTATION_SUMMARY.md).

## Overview

Phase 4 exposes cognitive state through an **in-process** request/response and
GraphQL helper layer suitable for embedding and tests. Network REST/WebSocket
servers and Unity3D/ROS embodiment bindings remain **deferred (v3.2+)**; the
sections below that document ports, live sockets, or game/robot runtimes are
design notes, not a claim of production readiness.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Phase 4 API Layer                           │
├─────────────────┬─────────────────┬─────────────────────────────┤
│   REST API      │   WebSocket     │      GraphQL Interface      │
│   Port 8080     │   Port 8081     │    /graphql endpoint        │
├─────────────────┼─────────────────┼─────────────────────────────┤
│                 │                 │                             │
│  GET /api/v1/   │  Event-driven   │  Complex queries &          │
│  POST /api/v1/  │  Real-time      │  Subscriptions              │
│  PUT /api/v1/   │  Bidirectional  │                             │
│  DELETE /api/v1/│  Communication  │                             │
│                 │                 │                             │
└─────────────────┴─────────────────┴─────────────────────────────┘
                              │
    ┌─────────────────────────┼─────────────────────────┐
    │                         │                         │
┌───▼───┐              ┌─────▼─────┐              ┌────▼────┐
│Unity3D│              │    ROS    │              │Web Agent│
│ 3D    │              │ Robotics  │              │Interface│
│Visual │              │ Planning  │              │Dashboard│
│& Inter│              │ Sensor    │              │Analytics│
└───────┘              └───────────┘              └─────────┘
                              │
    ┌─────────────────────────┼─────────────────────────┐
    │                         │                         │
┌───▼──────────────┐  ┌──────▼──────────┐  ┌──────────▼──────┐
│  Cognitive       │  │ Tensor Network  │  │   Attention     │
│  Accounting      │  │   Memory, Task, │  │   Economics     │
│  AtomSpace, PLN, │  │   AI, Autonomy  │  │   STI/LTI       │
│  ECAN, MOSES,    │  │   Nodes         │  │   Allocation    │
│  URE             │  │                 │  │                 │
└──────────────────┘  └─────────────────┘  └─────────────────┘
```

## REST API Endpoints

### Cognitive State Management

#### `GET /api/v1/cognitive/state`
Get current cognitive system state.

**Response:**
```json
{
  "status": "active",
  "timestamp": "2024-01-01T12:00:00Z",
  "attention": {
    "total_sti": 750.0,
    "total_lti": 400.0
  },
  "tensor_network": {
    "status": "active",
    "nodes": 4,
    "processing_load": 0.65
  },
  "cognitive_load": 0.65,
  "active_agents": 3
}
```

#### `POST /api/v1/cognitive/process`
Submit cognitive task for processing.

**Request:**
```json
{
  "task_type": "pattern_recognition",
  "priority": 750,
  "parameters": {
    "threshold": 0.8,
    "focus_area": "financial_flows"
  },
  "deadline": "2024-01-01T13:00:00Z"
}
```

**Response:**
```json
{
  "task_id": "task_001",
  "task_type": "pattern_recognition",
  "status": "processing",
  "timestamp": "2024-01-01T12:00:00Z"
}
```

### Attention Allocation

#### `GET /api/v1/attention/allocation`
Get current attention allocation across cognitive nodes.

**Response:**
```json
{
  "attention_allocation": {
    "total_sti_circulation": 750.0,
    "total_lti_circulation": 400.0,
    "node_allocations": [
      {
        "node_id": "memory",
        "node_type": "MEMORY",
        "sti": 200.0,
        "lti": 100.0,
        "confidence": 0.85,
        "activity_level": 0.7
      },
      {
        "node_id": "ai",
        "node_type": "AI",
        "sti": 220.0,
        "lti": 120.0,
        "confidence": 0.92,
        "activity_level": 0.8
      }
    ],
    "sti_fund_balance": 250.0,
    "lti_fund_balance": 100.0,
    "efficiency": 0.82
  }
}
```

### Agent Management

#### `POST /api/v1/agents/register`
Register a new agent for cognitive interaction.

**Request:**
```json
{
  "agent_type": "unity3d",
  "capabilities": ["3d_visualization", "interaction", "attention_display"],
  "session_id": "unity_session_001"
}
```

**Response:**
```json
{
  "agent_id": "agent_12345",
  "agent_type": "unity3d",
  "status": "registered",
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### `DELETE /api/v1/agents/{id}`
Unregister an agent.

**Response:**
```json
{
  "agent_id": "agent_12345",
  "status": "unregistered",
  "timestamp": "2024-01-01T12:00:00Z"
}
```

### Network Status

#### `GET /api/v1/network/status`
Get tensor network status.

**Response:**
```json
{
  "network_status": "active",
  "nodes": 4,
  "connections": 12,
  "uptime": 3600,
  "message_queue_size": 24,
  "processing_load": 0.68
}
```

### Transaction Processing

#### `POST /api/v1/transactions/submit`
Submit transaction for cognitive processing.

**Request:**
```json
{
  "description": "Cognitive-Enhanced Transfer",
  "amount": 1500.0,
  "from_account": "checking_cognitive_001",
  "to_account": "savings_adaptive_001",
  "cognitive_validation": true
}
```

**Response:**
```json
{
  "transaction_id": "txn_001",
  "status": "processing",
  "validation": "passed",
  "cognitive_confidence": 0.94
}
```

#### `GET /api/v1/accounts/{id}`
Get account information with cognitive data.

**Response:**
```json
{
  "account_id": "acc_001",
  "name": "Cognitive Checking",
  "type": "BANK",
  "balance": 1000.00,
  "cognitive_type": "ADAPTIVE",
  "attention": {
    "sti": 150.0,
    "lti": 75.0,
    "confidence": 0.87
  }
}
```

## WebSocket Communication

### Connection

Connect to WebSocket server on port 8081:
```javascript
const ws = new WebSocket('ws://localhost:8081/cognitive');
```

### Event Types

#### `cognitive.state.update`
Real-time cognitive state changes.

```json
{
  "event_type": "cognitive.state.update",
  "data": {
    "state": "updated",
    "tensor_network": {
      "status": "optimized",
      "processing_load": 0.72
    },
    "timestamp": "2024-01-01T12:00:00Z"
  },
  "timestamp": 1704067200,
  "priority": 0.8
}
```

#### `attention.allocation.change`
Attention allocation updates.

```json
{
  "event_type": "attention.allocation.change",
  "data": {
    "node_id": "ai",
    "old_sti": 180.0,
    "new_sti": 220.0,
    "reason": "pattern_recognition_completed",
    "timestamp": "2024-01-01T12:00:10Z"
  }
}
```

#### `agent.registration`
New agent connections.

```json
{
  "event_type": "agent.registration",
  "data": {
    "agent_id": "agent_12345",
    "agent_type": "unity3d",
    "action": "registered",
    "timestamp": "2024-01-01T12:00:00Z"
  }
}
```

#### `task.completion`
Task processing results.

```json
{
  "event_type": "task.completion",
  "data": {
    "task_id": "task_001",
    "type": "pattern_recognition",
    "status": "completed",
    "result": {
      "patterns_found": 7,
      "confidence": 0.94
    },
    "timestamp": "2024-01-01T12:00:15Z"
  }
}
```

### Subscription Management

```javascript
// Subscribe to events
ws.send(JSON.stringify({
  "action": "subscribe",
  "event_types": ["cognitive.state.update", "attention.allocation.change"]
}));

// Unsubscribe
ws.send(JSON.stringify({
  "action": "unsubscribe",
  "event_types": ["cognitive.state.update"]
}));
```

## GraphQL Interface

### Endpoint
`POST /graphql`

### Schema Highlights

#### Queries
- `cognitiveState`: Get comprehensive cognitive system state
- `attentionAllocation(nodeId: String)`: Get attention allocation details
- `tensorNetwork`: Get tensor network status
- `account(id: String!)`: Get account with cognitive data
- `patterns(threshold: Float)`: Get discovered patterns
- `agents`: Get registered agent list

#### Mutations
- `processTask(input: TaskInput!)`: Submit cognitive task
- `registerAgent(input: AgentInput!)`: Register new agent
- `updateAttention(input: AttentionInput!)`: Update attention allocation
- `submitTransaction(input: TransactionInput!)`: Submit transaction

#### Subscriptions
- `cognitiveStateUpdates`: Real-time state changes
- `attentionChanges(nodeId: String)`: Attention allocation updates
- `transactionEvents`: Transaction processing events
- `patternDiscovery`: Pattern discovery events

### Example Queries

#### Complex Cognitive State Query
```graphql
query GetComprehensiveState {
  cognitiveState {
    status
    timestamp
    cognitiveLoad
    activeAgents
    attention {
      totalSti
      totalLti
    }
    tensorNetwork {
      status
      nodes {
        id
        type
        active
        attentionWeight
      }
    }
  }
  attentionAllocation {
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
```

#### Processing Task Mutation
```graphql
mutation ProcessCognitiveTask($input: TaskInput!) {
  processTask(input: $input) {
    id
    type
    status
    processingTime
  }
}
```

Variables:
```json
{
  "input": {
    "type": "distributed_pattern_analysis",
    "priority": 900,
    "parameters": {
      "analysis_depth": "comprehensive",
      "confidence_threshold": 0.85
    }
  }
}
```

#### Real-time Subscription
```graphql
subscription CognitiveUpdates {
  cognitiveStateUpdates {
    status
    timestamp
    cognitiveLoad
    tensorNetwork {
      processingLoad
    }
  }
}
```

## Unity3D Integration

### Cognitive Component System

```csharp
using UnityEngine;
using GnuCashCognitive;

public class CognitiveVisualizer : MonoBehaviour
{
    public string componentId;
    public CognitiveComponentType componentType;
    
    void Start()
    {
        // Register with cognitive system
        var position = new float[] { transform.position.x, transform.position.y, transform.position.z };
        var rotation = new float[] { transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w };
        
        componentId = GnuCashCognitive.CreateComponent("AttentionVisualizer", position, rotation);
    }
    
    void Update()
    {
        // Update attention visualization
        var attentionParams = GnuCashCognitive.GetAttentionParams(componentId);
        UpdateVisualization(attentionParams);
    }
    
    void UpdateVisualization(AttentionParams attention)
    {
        // Scale object based on STI
        var scale = Mathf.Lerp(0.5f, 2.0f, attention.sti / 300.0f);
        transform.localScale = Vector3.one * scale;
        
        // Color based on confidence
        var renderer = GetComponent<Renderer>();
        renderer.material.color = Color.Lerp(Color.red, Color.green, attention.confidence);
    }
}
```

### Cognitive State Interface

```csharp
public class CognitiveStateManager : MonoBehaviour
{
    private WebSocketSharp.WebSocket ws;
    
    void Start()
    {
        // Connect to WebSocket
        ws = new WebSocket("ws://localhost:8081/cognitive");
        ws.OnMessage += OnCognitiveMessage;
        ws.Connect();
        
        // Subscribe to events
        var subscription = new {
            action = "subscribe",
            event_types = new[] { "cognitive.state.update", "attention.allocation.change" }
        };
        ws.Send(JsonUtility.ToJson(subscription));
    }
    
    void OnCognitiveMessage(object sender, MessageEventArgs e)
    {
        var cognitiveEvent = JsonUtility.FromJson<CognitiveEvent>(e.Data);
        HandleCognitiveEvent(cognitiveEvent);
    }
    
    void HandleCognitiveEvent(CognitiveEvent evt)
    {
        switch (evt.event_type)
        {
            case "cognitive.state.update":
                UpdateCognitiveState(evt.data);
                break;
            case "attention.allocation.change":
                UpdateAttentionVisualization(evt.data);
                break;
        }
    }
}
```

## ROS Integration

### Cognitive Planning Service

```cpp
// cognitive_planner_node.cpp
#include <ros/ros.h>
#include <gnucash_cognitive/PlanningService.h>
#include <gnucash_cognitive/AttentionQuery.h>

class CognitivePlannerNode
{
public:
    CognitivePlannerNode()
    {
        ros::NodeHandle nh;
        
        // Create cognitive services
        planning_service_ = nh.advertiseService("cognitive_planning", 
            &CognitivePlannerNode::planningCallback, this);
        attention_service_ = nh.advertiseService("attention_query",
            &CognitivePlannerNode::attentionCallback, this);
            
        // Subscribe to cognitive state
        state_sub_ = nh.subscribe("/cognitive/state", 10,
            &CognitivePlannerNode::stateCallback, this);
            
        ROS_INFO("Cognitive planner node initialized");
    }
    
private:
    ros::ServiceServer planning_service_;
    ros::ServiceServer attention_service_;
    ros::Subscriber state_sub_;
    
    bool planningCallback(gnucash_cognitive::PlanningService::Request& req,
                         gnucash_cognitive::PlanningService::Response& res)
    {
        // Process planning request with cognitive system
        std::string goal_json = serializeGoal(req.goal);
        std::string plan_result = gnc_ros_get_planning_result(goal_json.c_str());
        
        res.plan = deserializePlan(plan_result);
        res.success = true;
        
        return true;
    }
    
    bool attentionCallback(gnucash_cognitive::AttentionQuery::Request& req,
                          gnucash_cognitive::AttentionQuery::Response& res)
    {
        // Query attention allocation
        // Implementation calls cognitive API
        return true;
    }
    
    void stateCallback(const gnucash_cognitive::CognitiveState::ConstPtr& msg)
    {
        ROS_INFO("Cognitive state update: load=%f", msg->cognitive_load);
        // Process cognitive state updates
    }
};

int main(int argc, char** argv)
{
    ros::init(argc, argv, "cognitive_planner");
    CognitivePlannerNode node;
    ros::spin();
    return 0;
}
```

### ROS Message Definitions

```
# gnucash_cognitive/CognitiveState.msg
std_msgs/Header header
string status
float64 cognitive_load
int32 active_agents
AttentionSummary attention
TensorNetworkStatus tensor_network

# gnucash_cognitive/AttentionSummary.msg
float64 total_sti
float64 total_lti
float64 efficiency
NodeAttention[] node_allocations

# gnucash_cognitive/NodeAttention.msg
string node_id
string node_type
float64 sti
float64 lti
float64 confidence
float64 activity_level
```

## Web Agent Interface

### Dashboard Integration

```javascript
class CognitiveDashboard {
    constructor() {
        this.apiBase = 'http://localhost:8080/api/v1';
        this.ws = new WebSocket('ws://localhost:8081/cognitive');
        this.setupWebSocket();
        this.registerAgent();
    }
    
    async registerAgent() {
        const response = await fetch(`${this.apiBase}/agents/register`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                agent_type: 'web_dashboard',
                capabilities: ['visualization', 'interaction', 'reporting'],
                session_id: this.generateSessionId()
            })
        });
        
        const result = await response.json();
        this.agentId = result.agent_id;
        console.log('Dashboard agent registered:', this.agentId);
    }
    
    setupWebSocket() {
        this.ws.onmessage = (event) => {
            const cognitiveEvent = JSON.parse(event.data);
            this.handleCognitiveEvent(cognitiveEvent);
        };
        
        this.ws.onopen = () => {
            // Subscribe to relevant events
            this.ws.send(JSON.stringify({
                action: 'subscribe',
                event_types: [
                    'cognitive.state.update',
                    'attention.allocation.change',
                    'task.completion'
                ]
            }));
        };
    }
    
    handleCognitiveEvent(event) {
        switch (event.event_type) {
            case 'cognitive.state.update':
                this.updateCognitiveStateDisplay(event.data);
                break;
            case 'attention.allocation.change':
                this.updateAttentionVisualization(event.data);
                break;
            case 'task.completion':
                this.showTaskCompletion(event.data);
                break;
        }
    }
    
    async getCognitiveState() {
        const response = await fetch(`${this.apiBase}/cognitive/state`);
        return await response.json();
    }
    
    async submitCognitiveTask(taskData) {
        const response = await fetch(`${this.apiBase}/cognitive/process`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(taskData)
        });
        return await response.json();
    }
    
    async executeGraphQLQuery(query, variables = {}) {
        const response = await fetch('/graphql', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ query, variables })
        });
        return await response.json();
    }
}

// Initialize dashboard
const dashboard = new CognitiveDashboard();
```

## Performance Requirements

### API Response Times
- Simple queries: < 50ms
- Complex GraphQL queries: < 200ms
- Cognitive task submission: < 100ms
- Agent registration: < 50ms

### WebSocket Performance
- Event delivery latency: < 10ms
- Concurrent connections: 1000+
- Message throughput: 10,000+ messages/second

### System Throughput
- HTTP requests: 10,000+ requests/second
- Cognitive tasks: 100+ tasks/second
- Attention updates: 1,000+ updates/second

### Memory and CPU
- Memory usage: < 1GB for core API
- CPU usage: < 50% under normal load
- Response time degradation: < 20% under peak load

## Integration Testing

### API Integration Test
```bash
# Start the API server
./phase4-api-demo

# Run integration tests
make test-cognitive-api

# Performance testing
curl -w "@curl-format.txt" -s -o /dev/null http://localhost:8080/api/v1/cognitive/state
```

### WebSocket Testing
```javascript
const WebSocket = require('ws');
const ws = new WebSocket('ws://localhost:8081/cognitive');

ws.on('open', () => {
    // Test subscription
    ws.send(JSON.stringify({
        action: 'subscribe',
        event_types: ['cognitive.state.update']
    }));
});

ws.on('message', (data) => {
    const event = JSON.parse(data);
    console.log('Received event:', event.event_type);
});
```

## Error Handling

### HTTP Status Codes
- `200 OK`: Successful request
- `201 Created`: Resource created successfully
- `400 Bad Request`: Invalid request data
- `401 Unauthorized`: Authentication required
- `404 Not Found`: Resource not found
- `500 Internal Server Error`: Server error
- `503 Service Unavailable`: System overloaded

### Error Response Format
```json
{
  "error": "Invalid request format",
  "status": 400,
  "timestamp": 1704067200,
  "details": {
    "field": "task_type",
    "message": "Required field missing"
  }
}
```

### WebSocket Error Handling
```javascript
ws.on('error', (error) => {
    console.error('WebSocket error:', error);
    // Implement reconnection logic
    setTimeout(reconnect, 5000);
});
```

## Security Considerations

### API Security
- Input validation on all endpoints
- Rate limiting: 1000 requests/minute per IP
- JSON payload size limit: 10MB
- Request timeout: 30 seconds

### WebSocket Security
- Connection authentication
- Message validation
- Connection limits per IP
- Heartbeat/keepalive mechanism

### Data Privacy
- No sensitive financial data in logs
- Secure handling of cognitive state data
- Agent session management
- Connection encryption (HTTPS/WSS in production)

## Deployment

### Docker Configuration
```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libglib2.0-dev \
    libjson-glib-dev \
    pkg-config

COPY . /app
WORKDIR /app

RUN make phase4-api-demo
EXPOSE 8080 8081

CMD ["./phase4-api-demo"]
```

### Production Considerations
- Load balancing for multiple API instances
- WebSocket connection persistence
- Database integration for persistent storage
- Monitoring and logging
- Health check endpoints
- Graceful shutdown handling

This completes the Phase 4 Distributed Cognitive Mesh API & Embodiment Layer implementation, providing comprehensive REST endpoints, real-time WebSocket communication, Unity3D integration, ROS bindings, GraphQL interface, and web agent support for embodied cognitive applications.