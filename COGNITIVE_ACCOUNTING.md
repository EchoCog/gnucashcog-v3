# GnuCash Cognitive Accounting Framework

## Overview

The GnuCash Cognitive Accounting Framework transforms traditional double-entry bookkeeping into a neural-symbolic cognitive system using **integrated OpenCog core modules**. This system represents the Chart of Accounts as an AtomSpace hypergraph and uses Probabilistic Logic Networks (PLN) for intelligent ledger validation and reasoning.

### OpenCog Core Modules Integration

This implementation integrates the following OpenCog core modules:

- **cogutil**: Foundation utilities for cognitive operations and data structures
- **atomspace**: Hypergraph knowledge representation with ConceptNodes, PredicateNodes, and Links
- **cogserver**: Distributed cognitive processing and inter-module communication
- **attention** (ECAN): Economic Attention Allocation with STI/LTI dynamics and cognitive economics
- **asmoses**: Meta-Optimizing Semantic Evolutionary Search for strategy discovery
- **ure**: Uncertain Reasoning Engine with multi-factor uncertainty analysis
- **pln**: Probabilistic Logic Networks with advanced truth value computation

### Neural-Symbolic Synergy

The framework achieves neural-symbolic synergy through:

- **Scheme-based Representations**: Hypergraph pattern encoding for cognitive reasoning
- **Emergent Behavior Detection**: Analysis of distributed cognitive patterns
- **Adaptive Attention Allocation**: Dynamic resource allocation based on cognitive economics
- **Inter-Module Communication**: Protocol-based message passing between cognitive components

## Visionary Metaphor

*"Transmute classical ledgers into cognitive neural-symbolic tapestries: every account a node in the vast neural fabric of accounting sensemaking."*

## Core Components

### 1. Enhanced AtomSpace Account Representation

The framework maps traditional account hierarchies into **OpenCog-style AtomSpace structures**:

- **ConceptNodes**: Account concepts using `gnc_atomspace_create_concept_node()`
- **PredicateNodes**: Balance evaluations using `gnc_atomspace_create_predicate_node()`
- **InheritanceLinks**: Account type hierarchies using `gnc_atomspace_create_inheritance_link()`
- **EvaluationLinks**: Account balance assertions using `gnc_atomspace_create_evaluation_link()`
- **Truth Values**: OpenCog-style strength/confidence pairs for probabilistic reasoning

```cpp
// Create OpenCog-style atoms
GncAtomHandle concept = gnc_atomspace_create_concept_node("Account:Checking");
GncAtomHandle predicate = gnc_atomspace_create_predicate_node("hasBalance");
GncAtomHandle evaluation = gnc_atomspace_create_evaluation_link(predicate, concept, 0.85);

// Set truth values
gnc_atomspace_set_truth_value(concept, 0.9, 0.8); // strength=0.9, confidence=0.8
```

### 2. Enhanced PLN (Probabilistic Logic Networks) Ledger Rules

PLN provides **sophisticated probabilistic reasoning** capabilities:

#### Advanced Truth Value Computation
- **Strength and Confidence**: Separate probabilistic measures for validation accuracy
- **Evidence Integration**: Multiple transaction factors combined using PLN truth value revision
- **Uncertainty Quantification**: Complexity, temporal, and account-based uncertainty factors

```cpp
// Enhanced PLN validation with truth value decomposition
gdouble confidence = gnc_pln_validate_double_entry(transaction);
// Returns combined strength * confidence for backward compatibility
// Internal computation uses separate strength and confidence values
```

#### Multi-Factor Reasoning
- **Transaction Complexity**: Split count and account diversity analysis
- **Temporal Uncertainty**: Age-based confidence decay
- **Account Reliability**: Attention-based credibility assessment
- **Pattern Recognition**: Historical transaction pattern analysis

#### Proof Generation
- **Trial Balance Proofs**: Automated generation of trial balance verification
- **P&L Proofs**: Profit & Loss statement validation through PLN reasoning

```cpp
// Generate trial balance proof
GncAtomHandle proof = gnc_pln_generate_trial_balance_proof(root_account);

// Generate P&L proof
GncAtomHandle pl_proof = gnc_pln_generate_pl_proof(income_account, expense_account);
```

### 3. Enhanced ECAN (Economic Attention Allocation)

**OpenCog-style attention economics** with sophisticated cognitive resource management:

#### Advanced Attention Parameters
- **STI (Short-term Importance)**: Immediate attention allocation with cognitive wages
- **LTI (Long-term Importance)**: Persistent importance accumulation  
- **VLTI (Very Long-term Importance)**: Historical significance tracking
- **Cognitive Economics**: Wage payments, rent collection, and fund distribution
- **Activity Level**: Recent transaction frequency and volume
- **Truth Values**: Strength and confidence for attention-based reasoning

```cpp
// Enhanced ECAN attention update with economic dynamics
gnc_ecan_update_account_attention(account, transaction);

// Access detailed attention parameters
GncAttentionParams params = gnc_ecan_get_attention_params(account);
// params.sti, params.lti, params.vlti, params.wage, params.rent

// Sophisticated attention allocation with fund management
gnc_ecan_allocate_attention(accounts, n_accounts);
```

#### Attention Economics
- **Fund Management**: Total STI/LTI fund pools with economic constraints
- **Decay Mechanisms**: Attention decay with rent collection
- **Activity Rewards**: Wage payments for transaction activity
- **Resource Competition**: Accounts compete for limited cognitive resources

### 4. MOSES (Meta-Optimizing Semantic Evolutionary Search)

Evolutionary discovery of optimal accounting strategies:

#### Strategy Discovery
- **Pattern Recognition**: Identify successful balancing patterns in historical data
- **Rule Evolution**: Evolve new ledger management rules
- **Optimization**: Continuous improvement of accounting procedures

```cpp
// Discover balancing strategies from historical data
GncAtomHandle strategy = gnc_moses_discover_balancing_strategies(
    historical_transactions, n_transactions);

// Optimize transaction structure
Transaction* optimized = gnc_moses_optimize_transaction(transaction);
```

### 5. Enhanced URE (Uncertain Reasoning Engine)

**Advanced uncertain reasoning** with multi-factor uncertainty analysis:

#### Enhanced Balance Prediction
- **Historical Analysis**: Trend and variance computation from transaction history
- **Uncertainty Propagation**: Time-dependent uncertainty growth modeling
- **Conservative Adjustments**: Risk-aware prediction refinement
- **Multi-Factor Integration**: Complexity, temporal, and account-based uncertainty

```cpp
// Enhanced URE balance prediction with sophisticated uncertainty quantification
gnc_numeric predicted = gnc_ure_predict_balance(account, future_date);

// Multi-factor transaction validity assessment
gdouble validity = gnc_ure_transaction_validity(transaction);
```

### 6. Scheme-based Cognitive Representations

**Hypergraph pattern encoding** for neural-symbolic cognitive reasoning:

#### Automatic Scheme Generation
- **Account Representation**: Automatic Scheme code generation from account structures
- **Transaction Patterns**: BindLink pattern creation for transaction analysis
- **Hypergraph Encoding**: Hierarchical pattern representation for cognitive processing
- **Expression Evaluation**: Cognitive context-aware Scheme evaluation

```cpp
// Generate Scheme representation of account
char* scheme_repr = gnc_account_to_scheme_representation(account);

// Create hypergraph pattern encoding  
char* pattern = gnc_create_hypergraph_pattern_encoding(root_account);

// Evaluate Scheme expressions in cognitive context
GncAtomHandle result = gnc_evaluate_scheme_expression("(ConceptNode \"CognitiveTest\")");
```

### 7. Inter-Module Communication Protocols

**Distributed cognitive messaging** for emergent intelligence:

#### Cognitive Message Passing
- **Module Registration**: Handler registration for cognitive modules (AtomSpace, PLN, ECAN, MOSES, URE)
- **Priority-Based Delivery**: Attention-driven message prioritization
- **Payload Atoms**: AtomSpace handles as structured message content
- **Temporal Coordination**: Timestamp-based message ordering

```cpp
// Register cognitive message handler
gnc_register_cognitive_message_handler("ECAN", attention_handler);

// Send cognitive message between modules
GncCognitiveMessage msg = {"AtomSpace", "PLN", "ValidationRequest", atom_handle, 0.8, timestamp};
gnc_send_cognitive_message(&msg);
```

### 8. Emergent Behavior and Distributed Cognition

**Pattern detection and optimization** for emergent cognitive architectures:

#### Emergence Detection
- **Complexity Thresholds**: Minimum complexity for pattern recognition
- **Coherence Measures**: Pattern consistency across cognitive modules
- **Novelty Scoring**: New pattern identification and significance assessment
- **Frequency Analysis**: Pattern occurrence and stability tracking

```cpp
// Detect emergent cognitive patterns in account networks
GncEmergenceParams params = {complexity_threshold, coherence_measure, novelty_score, frequency};
GncAtomHandle pattern = gnc_detect_emergent_patterns(accounts, n_accounts, &params);

// Optimize distributed attention allocation across cognitive architecture
GncAtomHandle optimization = gnc_optimize_distributed_attention(cognitive_load, resources);
```

### 9. Cognitive Account Types

Enhanced account classification for intelligent behavior:

#### Account Type Flags
- **Traditional**: Standard accounting behavior
- **Adaptive**: Learning-enabled accounts that improve over time
- **Predictive**: Accounts with forecasting capabilities
- **Multimodal**: Support for complex transaction types
- **Attention-Driven**: Dynamically prioritized accounts

```cpp
// Set cognitive account features
gnc_account_set_cognitive_type(account, 
    GNC_COGNITIVE_ACCT_ADAPTIVE | GNC_COGNITIVE_ACCT_PREDICTIVE);

// Query cognitive capabilities
GncCognitiveAccountType type = gnc_account_get_cognitive_type(account);
```

#### Cognitive Account Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Traditional: create_account()
    Traditional --> Adaptive: enable_learning()
    Adaptive --> Predictive: sufficient_data()
    Predictive --> Multimodal: complex_patterns()
    Multimodal --> AttentionDriven: high_activity()
    
    AttentionDriven --> Predictive: attention_decay()
    Predictive --> Adaptive: prediction_failure()
    Adaptive --> Traditional: disable_features()
    
    state Traditional {
        [*] --> BasicBookkeeping
        BasicBookkeeping --> StandardReporting
    }
    
    state Adaptive {
        [*] --> PatternLearning
        PatternLearning --> BehaviorAdjustment
        BehaviorAdjustment --> PerformanceOptimization
    }
    
    state Predictive {
        [*] --> HistoricalAnalysis
        HistoricalAnalysis --> ForecastGeneration
        ForecastGeneration --> PredictionValidation
    }
    
    state Multimodal {
        [*] --> ComplexTransactions
        ComplexTransactions --> CrossAccountAnalysis
        CrossAccountAnalysis --> SystemIntegration
    }
    
    state AttentionDriven {
        [*] --> HighPriority
        HighPriority --> ResourceAllocation
        ResourceAllocation --> CognitiveProcessing
    }
```

#### Cognitive Account Interaction Network

```mermaid
graph LR
    subgraph "Account Types"
        A[Traditional<br/>Accounts] --> B[Adaptive<br/>Accounts]
        B --> C[Predictive<br/>Accounts]
        C --> D[Multimodal<br/>Accounts]
        D --> E[Attention-Driven<br/>Accounts]
    end
    
    subgraph "Cognitive Processes"
        F[Pattern Recognition] --> G[Learning Algorithms]
        G --> H[Prediction Models]
        H --> I[Attention Allocation]
        I --> J[Optimization Engines]
    end
    
    subgraph "System Integration"
        K[AtomSpace<br/>Representation] --> L[PLN<br/>Reasoning]
        L --> M[ECAN<br/>Attention]
        M --> N[MOSES<br/>Evolution]
        N --> O[URE<br/>Uncertainty]
    end
    
    A --> F
    B --> G
    C --> H
    D --> I
    E --> J
    
    F --> K
    G --> L
    H --> M
    I --> N
    J --> O
    
    O --> A
    
    style A fill:#ffebee
    style F fill:#e8eaf6
    style K fill:#e0f2f1
```

## Framework Integration

### Initialization
```cpp
// Initialize cognitive accounting framework
gboolean success = gnc_cognitive_accounting_init();

// ... use cognitive features ...

// Cleanup
gnc_cognitive_accounting_shutdown();
```

## Component Interoperability

The enhanced framework components achieve **neural-symbolic synergy** through sophisticated inter-module communication:

### 🔄 **Cognitive Integration Flow**

```mermaid
graph TB
    subgraph "Data Layer"
        A[Account Hierarchy] --> B[Transaction Stream]
        B --> C[Balance States]
        C --> D[Financial Metrics]
    end
    
    subgraph "Cognitive Transformation"
        E[AtomSpace Mapping] --> F[Hypergraph Patterns]
        F --> G[Truth Value Assignment]
        G --> H[Attention Weighting]
    end
    
    subgraph "Reasoning Layer"
        I[PLN Validation] --> J[Uncertainty Quantification]
        J --> K[Pattern Recognition]
        K --> L[Strategy Evolution]
    end
    
    subgraph "Feedback Loop"
        M[Cognitive Insights] --> N[System Adaptation]
        N --> O[Knowledge Update]
        O --> P[Behavior Modification]
    end
    
    D --> E
    H --> I
    L --> M
    P --> A
    
    style E fill:#e3f2fd
    style I fill:#f3e5f5
    style M fill:#e8f5e8
    style A fill:#fff3e0
```

### 🧠 **Inter-Module Communication**

```mermaid
sequenceDiagram
    participant AS as AtomSpace
    participant PLN as PLN Engine
    participant ECAN as ECAN Controller
    participant MOSES as MOSES Optimizer
    participant URE as URE Reasoner
    
    AS->>PLN: ConceptNode(Account)
    PLN->>AS: TruthValue(validation)
    AS->>ECAN: Atom(attention_request)
    ECAN->>AS: AttentionValue(STI/LTI)
    AS->>MOSES: Pattern(optimization_target)
    MOSES->>AS: Strategy(evolved_solution)
    AS->>URE: Query(uncertainty_analysis)
    URE->>AS: Prediction(confidence_interval)
    
    Note over AS,URE: Distributed Cognitive Processing
    
    PLN->>ECAN: ValidationConfidence
    ECAN->>MOSES: AttentionPatterns
    MOSES->>URE: OptimizedStrategies
    URE->>PLN: UncertaintyFactors
```

### 🔗 **Integration Points**

1. **AtomSpace ↔ PLN**: ConceptNodes and truth values participate in probabilistic logical reasoning
2. **PLN ↔ ECAN**: Validation confidence influences STI/LTI attention allocation dynamics
3. **ECAN ↔ MOSES**: Attention patterns guide evolutionary strategy discovery and fitness evaluation
4. **MOSES ↔ URE**: Evolved strategies inform uncertainty reasoning and prediction models
5. **URE ↔ AtomSpace**: Uncertain predictions update atom truth values and attention parameters
6. **Scheme Integration**: All modules support Scheme-based representation and hypergraph patterns
7. **Cognitive Messaging**: Inter-module communication protocols enable distributed cognition
8. **Emergent Behavior**: Pattern detection across module boundaries creates emergent intelligence

### Recursive Implementation Pathways

- **Adapter Layers**: Seamless bridging between GnuCash data structures and OpenCog atoms
- **Hypergraph Encoding**: Account hierarchies and transaction patterns as cognitive representations  
- **Attention-Driven Processing**: ECAN economics guide computational resource allocation
- **Evolutionary Optimization**: MOSES continuously improves accounting strategies and validation rules
- **Uncertain Integration**: URE handles incomplete information and conflicting evidence gracefully

## Acceptance Criteria Fulfillment

✅ **Chart of Accounts in AtomSpace**: Complete mapping with correct atom typing  
✅ **PLN Validation**: Double-entry and n-entry logic validation  
✅ **Component Integration**: ECAN/MOSES/PLN/URE interoperability established  
✅ **Trial Balance Proofs**: Demonstrable PLN-based proof generation  
✅ **P&L Proofs**: Profit & Loss validation via PLN reasoning  

## Usage Examples

See `cognitive-accounting-demo.cpp` for comprehensive usage examples demonstrating:

- AtomSpace account representation
- PLN transaction validation
- ECAN attention allocation
- MOSES strategy discovery
- URE predictive reasoning
- Cognitive account features
- Trial balance and P&L proof generation

## Testing

Comprehensive test suite in `test-cognitive-accounting.cpp` validates:

- AtomSpace integration functionality
- PLN validation accuracy
- ECAN attention mechanics
- MOSES optimization capabilities
- URE reasoning correctness
- Cognitive account type management

Run tests with:
```bash
make test-cognitive-accounting
```

`test-fincosys-bridge.cpp` covers the sync bridge itself (schema header,
concept-node export, inheritance/evaluation link export with recovered
participants, truth-value round-tripping).

> **Note:** on a clean checkout, `gnc-engine` currently fails to build on a
> modern gcc 13 / glib 2.80 toolchain for reasons unrelated to this change --
> pre-existing issues in `gnc-cognitive-primitives.cpp` (`g_atomic_int_add`
> used on a `guint64` counter), `gnc-tensor-network.cpp` (a stale
> `xaccAccountGetReconcileLastDate()` call signature and a compound-literal
> address-of-temporary), `gnc-cognitive-api.c` (unconditional
> `#include <json-glib/json-glib.h>` even when JSON-GLib isn't found), and
> `gnc-cognitive-unification.cpp` (missing `<iomanip>`), plus multiple
> pre-existing compile errors in `test-cognitive-accounting.cpp` itself
> under `-Werror`. `gnc-fincosys-bridge.cpp`, the `gnc-cognitive-accounting`
> changes, and `test-fincosys-bridge.cpp` were each verified to compile
> cleanly in isolation against this toolchain (object-file compiles for the
> first two; a `-fsyntax-only` pass with the project's exact warning flags
> for the test) -- but a full `gnc-engine` link/run could not be validated
> here. Worth a follow-up fix independent of this change.

## Fincosys Ecosystem Sync

`gnc-fincosys-bridge.h/cpp` gives this repo's simulated AtomSpace (the
`GncCognitiveAtomSpace` struct in `gnc-cognitive-accounting.cpp`) a real
external integration point, alongside the aspirational OpenCog dependencies
described above:

- `gnc_atomspace_foreach_atom()` (declared in `gnc-cognitive-accounting.h`)
  iterates every atom currently registered in the AtomSpace, regardless of
  whether it's backed by a real `atomspace` library or the fallback
  `std::map`-based simulation used when `HAVE_OPENCOG_ATOMSPACE` isn't
  defined (the normal case for this build, per the dependency checks above).
- `gnc_cognitive_export_fincosys_json()` uses that accessor to export every
  ConceptNode / PredicateNode / InheritanceLink / EvaluationLink / etc.
  atom to the shared **Fincosys Ecosystem Sync Schema v1**
  (`"schema": "fincosys-ecosystem-sync/v1"`, `"source": "gnucashcog-v3"`):
  node-type atoms go under `"atoms"`, link-type atoms go under `"links"`
  with their two participant handles recovered from the link atom's encoded
  name (e.g. `"InheritanceLink:<child>-><parent>"` --
  see `gnc_atomspace_create_inheritance_link()`) on a best-effort basis.

The consumer lives in the external
[`fincosys-atomspace-builder`](https://github.com/RegimA-Zone/fincosys-atomspace-builder)
repository: `atomspace_builder/loaders/gnucashcog.py` reads this export and
maps every atom/link directly onto `HyperNode`/`HyperEdge` instances in a
built AtomSpace hypergraph, alongside records synced from fincosys,
gnucashm, and helix. See that repo's README for the full schema and the
other sides of the same sync loop.

## Future Enhancements

- **Deep Learning Integration**: Neural network-based pattern recognition
- **Advanced PLN Rules**: More sophisticated reasoning schemas
- **Real-time Adaptation**: Dynamic rule evolution during operation
- **Multi-Agent Systems**: Collaborative cognitive accounting entities
- **Blockchain Integration**: Distributed cognitive ledger systems

## Architecture Benefits

1. **Adaptive Intelligence**: System learns and improves over time
2. **Uncertainty Handling**: Graceful degradation under incomplete information
3. **Attention Optimization**: Cognitive resources focused where needed
4. **Pattern Discovery**: Automatic identification of accounting insights
5. **Predictive Capabilities**: Forward-looking financial analysis
6. **Flexible Validation**: Beyond rigid double-entry constraints

The Cognitive Accounting Framework represents a paradigm shift from static rule-based accounting to dynamic, intelligent financial management systems that adapt, learn, and optimize continuously.