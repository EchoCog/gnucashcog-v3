/********************************************************************\
 * gnc-cognitive-accounting.h -- OpenCog integration for accounting *
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

/** @addtogroup Engine
    @{ */
/** @addtogroup CognitiveAccounting
    Cognitive accounting functionality using OpenCog AtomSpace for
    representing Chart of Accounts as hypergraph structures and PLN
    for reasoning about ledger rules and balance validation.
    @{ */

/** @file gnc-cognitive-accounting.h
    @brief Cognitive accounting with AtomSpace and PLN integration
    @author Copyright (C) 2024 GnuCash Cognitive Engine
*/

#ifndef GNC_COGNITIVE_ACCOUNTING_H
#define GNC_COGNITIVE_ACCOUNTING_H

#include "Account.h"
#include "Transaction.h"
#include "gnc-engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @name AtomSpace Account Representation */
/** @{ */

/** AtomSpace handle type for representing accounts as atoms */
typedef guint64 GncAtomHandle;

/** Account node types in AtomSpace hierarchy - aligned with OpenCog atom types */
typedef enum {
    /* Basic node types */
    GNC_ATOM_CONCEPT_NODE,        /**< ConceptNode - basic account concepts */
    GNC_ATOM_PREDICATE_NODE,      /**< PredicateNode - balance and validation predicates */
    GNC_ATOM_SCHEMA_NODE,         /**< SchemaNode - transaction schemas and rules */
    GNC_ATOM_GROUNDED_SCHEMA,     /**< GroundedSchemaNode - executable accounting procedures */
    
    /* Link types for relationships */
    GNC_ATOM_INHERITANCE_LINK,    /**< InheritanceLink - account type hierarchy */
    GNC_ATOM_SIMILARITY_LINK,     /**< SimilarityLink - account pattern relationships */
    GNC_ATOM_MEMBER_LINK,         /**< MemberLink - account membership in categories */
    GNC_ATOM_EVALUATION_LINK,     /**< EvaluationLink - balance and rule evaluations */
    GNC_ATOM_EXECUTION_LINK,      /**< ExecutionLink - transaction execution */
    
    /* PLN-specific types */
    GNC_ATOM_IMPLICATION_LINK,    /**< ImplicationLink - ledger rules and constraints */
    GNC_ATOM_AND_LINK,            /**< AndLink - conjunction of conditions */
    GNC_ATOM_OR_LINK,             /**< OrLink - disjunction of alternatives */
    
    /* MOSES-specific types */
    GNC_ATOM_COMBO_NODE,          /**< ComboNode - evolved accounting strategies */
    
    /* Legacy compatibility */
    GNC_ATOM_ACCOUNT_CONCEPT = GNC_ATOM_CONCEPT_NODE,
    GNC_ATOM_ACCOUNT_CATEGORY = GNC_ATOM_CONCEPT_NODE,
    GNC_ATOM_ACCOUNT_HIERARCHY = GNC_ATOM_INHERITANCE_LINK,
    GNC_ATOM_ACCOUNT_BALANCE = GNC_ATOM_PREDICATE_NODE,
    GNC_ATOM_TRANSACTION_RULE = GNC_ATOM_SCHEMA_NODE,
    GNC_ATOM_DOUBLE_ENTRY_RULE = GNC_ATOM_IMPLICATION_LINK,
    GNC_ATOM_N_ENTRY_RULE = GNC_ATOM_IMPLICATION_LINK
} GncAtomType;

/** Enhanced OpenCog ECAN-style attention allocation parameters for Phase 2 */
typedef struct {
    /* Short-term importance (STI) */
    gdouble sti;                  /**< Short-term importance value */
    gdouble sti_funds;            /**< Available STI funds for allocation */
    gdouble sti_decay_rate;       /**< STI decay rate per cycle */
    
    /* Long-term importance (LTI) */ 
    gdouble lti;                  /**< Long-term importance value */
    gdouble lti_funds;            /**< Available LTI funds for allocation */
    gdouble lti_decay_rate;       /**< LTI decay rate per cycle */
    
    /* Very long-term importance (VLTI) */
    gdouble vlti;                 /**< Very long-term importance value */
    gdouble vlti_threshold;       /**< Threshold for VLTI promotion */
    
    /* Confidence and truth values */
    gdouble confidence;           /**< PLN confidence level */
    gdouble strength;             /**< PLN strength/truth value */
    
    /* Activity and wages */
    gdouble activity_level;       /**< Account activity level */
    gdouble wage;                 /**< Cognitive wage for attention allocation */
    gdouble rent;                 /**< Cognitive rent for maintaining attention */
    gdouble wage_multiplier;      /**< Activity-based wage multiplier */
    
    /* Attention spreading */
    gdouble spreading_rate;       /**< Rate of attention spreading to neighbors */
    gdouble spreading_threshold;  /**< Minimum attention for spreading */
    gdouble focus_factor;         /**< Attention focus concentration factor */
    
    /* Resource competition */
    gdouble competition_strength; /**< Strength in resource competition */
    gdouble starvation_threshold; /**< Threshold below which node is starved */
    gint64 last_activity_time;    /**< Timestamp of last activity */
    
    /* Legacy compatibility */
    gdouble importance;           /**< Legacy: maps to STI */
    gdouble attention_value;      /**< Legacy: maps to total attention */
} GncAttentionParams;

/** @} */

/** @name OpenCog-style AtomSpace Operations */
/** @{ */

/** Create OpenCog-style ConceptNode for account
 * @param name Concept name
 * @return Handle to concept node atom
 */
GncAtomHandle gnc_atomspace_create_concept_node(const char* name);

/** Create OpenCog-style PredicateNode for balance evaluation
 * @param name Predicate name  
 * @return Handle to predicate node atom
 */
GncAtomHandle gnc_atomspace_create_predicate_node(const char* name);

/** Create OpenCog-style EvaluationLink for account balance
 * @param predicate_atom Predicate atom handle
 * @param account_atom Account atom handle
 * @param truth_value Truth value for the evaluation
 * @return Handle to evaluation link atom
 */
GncAtomHandle gnc_atomspace_create_evaluation_link(GncAtomHandle predicate_atom,
                                                   GncAtomHandle account_atom,
                                                   gdouble truth_value);

/** Create OpenCog-style InheritanceLink for account hierarchy
 * @param child_atom Child account atom handle
 * @param parent_atom Parent account atom handle  
 * @return Handle to inheritance link atom
 */
GncAtomHandle gnc_atomspace_create_inheritance_link(GncAtomHandle child_atom,
                                                    GncAtomHandle parent_atom);

/** Set truth value for atom (OpenCog-style)
 * @param atom_handle Atom to update
 * @param strength Truth strength (0.0-1.0)
 * @param confidence Truth confidence (0.0-1.0)
 */
void gnc_atomspace_set_truth_value(GncAtomHandle atom_handle, 
                                   gdouble strength, gdouble confidence);

/** Get truth value for atom (OpenCog-style)
 * @param atom_handle Atom to query
 * @param strength Output parameter for truth strength
 * @param confidence Output parameter for truth confidence
 * @return TRUE if truth value exists, FALSE otherwise
 */
gboolean gnc_atomspace_get_truth_value(GncAtomHandle atom_handle,
                                       gdouble* strength, gdouble* confidence);

/** Callback type for gnc_atomspace_foreach_atom().
 * @param handle Atom handle
 * @param type Atom type (ConceptNode, InheritanceLink, etc.)
 * @param name Atom name (may encode link participants, e.g.
 *             "InheritanceLink:<child>-><parent>" -- see
 *             gnc_atomspace_create_inheritance_link())
 * @param strength Truth strength (0.0-1.0)
 * @param confidence Truth confidence (0.0-1.0)
 * @param user_data Caller-supplied context pointer
 */
typedef void (*GncAtomForeachCB) (GncAtomHandle handle, GncAtomType type,
                                  const char* name, gdouble strength,
                                  gdouble confidence, gpointer user_data);

/** Iterate over every atom currently registered in the cognitive AtomSpace.
 *  Intended for external export/sync bridges -- see gnc-fincosys-bridge.h,
 *  which uses this to export the whole AtomSpace to the shared
 *  "fincosys-ecosystem-sync/v1" schema.
 * @param callback Function called once per atom
 * @param user_data Passed through to @a callback
 * @return TRUE on success, FALSE if the AtomSpace has not been initialized
 */
gboolean gnc_atomspace_foreach_atom(GncAtomForeachCB callback, gpointer user_data);

/** @} */

/** @name AtomSpace Integration Functions */
/** @{ */

/** Initialize cognitive accounting AtomSpace 
 * @return TRUE on success, FALSE on failure
 */
gboolean gnc_cognitive_accounting_init(void);

/** Shutdown cognitive accounting and cleanup AtomSpace */
void gnc_cognitive_accounting_shutdown(void);

/** @return TRUE if the cognitive AtomSpace is currently initialized */
gboolean gnc_cognitive_accounting_is_initialized(void);

/** @return TRUE if lifecycle hooks should run (env GNC_COGNITIVE_ENABLED=1/true/yes
 *  or explicit enable via gnc_cognitive_set_enabled). Independent of init state. */
gboolean gnc_cognitive_is_enabled(void);

/** Explicitly enable/disable automatic cognitive lifecycle processing. */
void gnc_cognitive_set_enabled(gboolean enabled);

/**
 * Register book open/close/save danglers so the cognitive engine auto-inits
 * when GNC_COGNITIVE_ENABLED is set. Safe to call multiple times.
 * Called automatically from gnc_cognitive_accounting_init() and from
 * gnc_engine_init when the feature flag is on.
 */
void gnc_cognitive_register_lifecycle_hooks(void);

/** Convert traditional account to AtomSpace representation
 * @param account The GnuCash account to convert
 * @return AtomSpace handle for the account atom
 */
GncAtomHandle gnc_account_to_atomspace(const Account *account);

/** Create account hierarchy links in AtomSpace
 * @param parent_atom Parent account atom handle
 * @param child_atom Child account atom handle
 * @return Handle to hierarchy link atom
 */
GncAtomHandle gnc_atomspace_create_hierarchy_link(GncAtomHandle parent_atom, 
                                                  GncAtomHandle child_atom);

/** Map an entire book's account hierarchy into the AtomSpace.
 *  Creates ConceptNodes and Inheritance/Hierarchy links for every account.
 * @param book Book whose root account tree is walked
 * @return Number of accounts mapped, or -1 on error
 */
gint gnc_book_to_atomspace(QofBook *book);

/** Lifecycle: react to a committed (non-destroyed) transaction.
 *  Runs PLN double-entry validation and ECAN attention updates for
 *  every account touched by the transaction. No-op if not initialized.
 */
void gnc_cognitive_on_transaction_committed(Transaction *trans);

/** Lifecycle: ensure account is represented in the AtomSpace (e.g. after create/edit). */
void gnc_cognitive_on_account_changed(Account *account);

/** Persist the current AtomSpace to a fincosys-ecosystem-sync/v1 JSON sidecar.
 * @param path Destination file path
 * @return TRUE on success
 */
gboolean gnc_cognitive_save_snapshot(const char *path);

/** Load a fincosys-ecosystem-sync/v1 (or cognitive_atoms) JSON snapshot into the AtomSpace.
 *  Initializes the cognitive engine if needed.
 * @param path Source file path
 * @return Number of atoms/links imported, or -1 on error
 */
gint gnc_cognitive_load_snapshot(const char *path);

/** Dump a machine-readable cognitive state report (JSON string, caller g_free).
 *  Includes capability matrix, atom counts, and ECAN fund stats.
 */
gchar *gnc_cognitive_dump_state_json(void);

/** Human-readable capability matrix string (caller g_free). */
gchar *gnc_cognitive_capability_report(void);

/** @return Number of atoms currently in the cognitive AtomSpace (0 if not init). */
guint gnc_atomspace_count_atoms(void);

/** Retrieve stored outgoing participant handles for a link atom.
 * @param link Link handle
 * @param out_handles Caller-allocated array (may be NULL to query count only)
 * @param n_handles In: capacity of out_handles; Out: number of participants
 * @return TRUE if the link has stored outgoing set
 */
gboolean gnc_atomspace_get_outgoing(GncAtomHandle link,
                                    GncAtomHandle *out_handles,
                                    gsize *n_handles);

/** @} */

/** @name PLN Ledger Rules */
/** @{ */

/** Validate double-entry transaction using PLN
 * @param transaction The transaction to validate
 * @return Confidence level (0.0-1.0) of double-entry validity
 */
gdouble gnc_pln_validate_double_entry(const Transaction *transaction);

/** Validate n-entry transaction for multi-party scenarios
 * @param transaction The transaction to validate
 * @param n_parties Number of parties involved
 * @return Confidence level of n-entry validity
 */
gdouble gnc_pln_validate_n_entry(const Transaction *transaction, gint n_parties);

/** Generate trial balance proof using PLN reasoning
 * @param root_account Root account for balance calculation
 * @return PLN proof handle for trial balance
 */
GncAtomHandle gnc_pln_generate_trial_balance_proof(const Account *root_account);

/** Generate profit & loss proof using PLN
 * @param income_account Income account root
 * @param expense_account Expense account root
 * @return PLN proof handle for P&L statement
 */
GncAtomHandle gnc_pln_generate_pl_proof(const Account *income_account,
                                        const Account *expense_account);

/** @} */

/** @name ECAN Attention Allocation */
/** @{ */

/** Update attention allocation for account based on activity
 * @param account The account to update
 * @param transaction Recent transaction affecting the account
 */
void gnc_ecan_update_account_attention(Account *account, 
                                       const Transaction *transaction);

/** Get current attention parameters for account
 * @param account The account to query
 * @return Attention allocation parameters
 */
GncAttentionParams gnc_ecan_get_attention_params(const Account *account);

/** Allocate cognitive resources based on attention dynamics
 * @param accounts Array of accounts to consider
 * @param n_accounts Number of accounts in array
 */
void gnc_ecan_allocate_attention(Account **accounts, gint n_accounts);

/** Enhanced Phase 2 ECAN functions for resource kernel construction */

/** Initialize ECAN attention economy with fund pools
 * @param total_sti_funds Total STI fund pool size
 * @param total_lti_funds Total LTI fund pool size
 * @return TRUE on successful initialization
 */
gboolean gnc_ecan_init_attention_economy(gdouble total_sti_funds, gdouble total_lti_funds);

/** Apply attention spreading from source to connected accounts
 * @param source_account Source account for attention spreading
 * @param connected_accounts Array of connected accounts
 * @param n_connected Number of connected accounts
 * @param spreading_rate Rate of attention spreading (0.0-1.0)
 */
void gnc_ecan_spread_attention(const Account *source_account, 
                               Account **connected_accounts, 
                               gint n_connected,
                               gdouble spreading_rate);

/** Collect rent from all accounts and redistribute to fund pools
 * @param accounts Array of accounts to collect rent from
 * @param n_accounts Number of accounts
 * @return Total rent collected
 */
gdouble gnc_ecan_collect_rent_and_redistribute(Account **accounts, gint n_accounts);

/** Pay wages to accounts based on activity levels
 * @param accounts Array of accounts to pay wages to
 * @param n_accounts Number of accounts
 * @param wage_pool_size Available wage pool size
 */
void gnc_ecan_pay_activity_wages(Account **accounts, gint n_accounts, gdouble wage_pool_size);

/** Apply attention decay to all accounts in the system
 * @param accounts Array of accounts
 * @param n_accounts Number of accounts
 * @param decay_cycle_time Time since last decay cycle
 */
void gnc_ecan_apply_attention_decay(Account **accounts, gint n_accounts, gdouble decay_cycle_time);

/** Check for attention starvation and apply emergency allocation
 * @param accounts Array of accounts to check
 * @param n_accounts Number of accounts
 * @return Number of accounts that were starved and required emergency allocation
 */
gint gnc_ecan_prevent_attention_starvation(Account **accounts, gint n_accounts);

/** Get global ECAN system statistics
 * @param total_sti_in_circulation Output parameter for total STI in circulation
 * @param total_lti_in_circulation Output parameter for total LTI in circulation
 * @param sti_fund_balance Output parameter for STI fund balance
 * @param lti_fund_balance Output parameter for LTI fund balance
 */
void gnc_ecan_get_system_stats(gdouble *total_sti_in_circulation,
                               gdouble *total_lti_in_circulation,
                               gdouble *sti_fund_balance,
                               gdouble *lti_fund_balance);

/** @} */

/** @name MOSES Integration */
/** @{ */

/** Discover novel ledger balancing strategies using MOSES
 * @param historical_transactions Array of historical transactions
 * @param n_transactions Number of transactions
 * @return Handle to evolved balancing rule set
 */
GncAtomHandle gnc_moses_discover_balancing_strategies(Transaction **historical_transactions,
                                                      gint n_transactions);

/** Apply MOSES-evolved rules to optimize transaction patterns
 * @param transaction Transaction to optimize
 * @return Optimized transaction structure
 */
Transaction* gnc_moses_optimize_transaction(const Transaction *transaction);

/** @} */

/** @name URE Uncertain Reasoning */
/** @{ */

/** Apply uncertain reasoning to account balance predictions
 * @param account Account for prediction
 * @param future_date Date for balance prediction
 * @return Predicted balance with uncertainty bounds
 */
gnc_numeric gnc_ure_predict_balance(const Account *account, time64 future_date);

/** Reason about transaction validity under uncertainty
 * @param transaction Transaction to analyze
 * @return Validity probability with uncertainty quantification
 */
gdouble gnc_ure_transaction_validity(const Transaction *transaction);

/** @} */

/** @name Scheme-based Cognitive Representations */
/** @{ */

/** Generate Scheme representation of account in AtomSpace
 * @param account The account to represent
 * @return Allocated string containing Scheme code (caller must free)
 */
char* gnc_account_to_scheme_representation(const Account *account);

/** Generate Scheme representation of transaction pattern
 * @param transaction The transaction to represent
 * @return Allocated string containing Scheme code (caller must free)
 */
char* gnc_transaction_to_scheme_pattern(const Transaction *transaction);

/** Evaluate Scheme expression in cognitive context
 * @param scheme_expr Scheme expression to evaluate
 * @return Result of evaluation (implementation-dependent)
 */
GncAtomHandle gnc_evaluate_scheme_expression(const char* scheme_expr);

/** Create hypergraph pattern encoding for account hierarchy
 * @param root_account Root of account hierarchy
 * @return Allocated string with hypergraph pattern (caller must free)
 */
char* gnc_create_hypergraph_pattern_encoding(const Account *root_account);

/** @} */

/** @name Inter-Module Communication Protocols */
/** @{ */

/** Cognitive message for inter-module communication */
typedef struct {
    const char* source_module;    /**< Source module name */
    const char* target_module;    /**< Target module name */
    const char* message_type;     /**< Type of cognitive message */
    GncAtomHandle payload_atom;   /**< Atom containing message payload */
    gdouble priority;             /**< Message priority for attention allocation */
    time64 timestamp;             /**< Message timestamp */
} GncAccountCognitiveMessage;

/** Send cognitive message between modules
 * @param message Cognitive message to send
 * @return TRUE on successful delivery, FALSE otherwise
 */
gboolean gnc_send_cognitive_message(const GncAccountCognitiveMessage* message);

/** Register cognitive message handler for module
 * @param module_name Name of module to register for
 * @param handler_func Function to handle received messages
 * @return TRUE on successful registration, FALSE otherwise
 */
typedef void (*GncCognitiveMessageHandler)(const GncAccountCognitiveMessage* message);
gboolean gnc_register_cognitive_message_handler(const char* module_name,
                                               GncCognitiveMessageHandler handler_func);

/** @} */

/** @name Distributed Cognition and Emergent Behavior */
/** @{ */

/** Cognitive emergence detection parameters */
typedef struct {
    gdouble complexity_threshold;  /**< Minimum complexity for emergence detection */
    gdouble coherence_measure;     /**< Coherence in cognitive patterns */
    gdouble novelty_score;         /**< Novelty of emerging patterns */
    gint pattern_frequency;        /**< Frequency of pattern occurrence */
} GncEmergenceParams;

/** Detect emergent cognitive patterns in account activity
 * @param accounts Array of accounts to analyze
 * @param n_accounts Number of accounts
 * @param params Parameters for emergence detection
 * @return Handle to atom representing detected emergent pattern
 */
GncAtomHandle gnc_detect_account_emergent_patterns(Account** accounts, gint n_accounts,
                                          const GncEmergenceParams* params);

/** Optimize attention allocation for distributed cognition
 * @param cognitive_load Current cognitive load across modules
 * @param available_resources Available cognitive resources
 * @return Optimized attention allocation strategy
 */
GncAtomHandle gnc_optimize_distributed_attention(gdouble cognitive_load,
                                                gdouble available_resources);

/** @} */

/** @name Cognitive Account Types */
/** @{ */

/** Enhanced account types for cognitive accounting */
typedef enum {
    GNC_COGNITIVE_ACCT_TRADITIONAL = 0x0000,  /**< Standard account */
    GNC_COGNITIVE_ACCT_ADAPTIVE    = 0x0001,  /**< Adaptive learning account */
    GNC_COGNITIVE_ACCT_PREDICTIVE  = 0x0002,  /**< Predictive account */
    GNC_COGNITIVE_ACCT_MULTIMODAL  = 0x0004,  /**< Multi-modal transaction account */
    GNC_COGNITIVE_ACCT_ATTENTION   = 0x0008   /**< Attention-driven account */
} GncCognitiveAccountType;

/** Set cognitive account type flags
 * @param account Account to modify
 * @param cognitive_type Cognitive type flags
 */
void gnc_account_set_cognitive_type(Account *account, GncCognitiveAccountType cognitive_type);

/** Get cognitive account type flags
 * @param account Account to query
 * @return Current cognitive type flags
 */
GncCognitiveAccountType gnc_account_get_cognitive_type(const Account *account);

/** Check if account has specific cognitive behavior
 * @param account Account to check
 * @param behavior Cognitive behavior to test for
 * @return TRUE if account has the behavior, FALSE otherwise
 */
gboolean gnc_account_has_cognitive_behavior(const Account *account, GncCognitiveAccountType behavior);

/** Enable adaptive learning behavior for cognitive account
 * @param account Account to adapt
 * @param transaction Transaction to learn from
 */
void gnc_account_adapt_cognitive_behavior(Account *account, const Transaction *transaction);

/** @} */

/** @name Phase 2: Distributed Mesh Attention Integration */
/** @{ */

/** Attention mesh node for distributed processing */
typedef struct {
    gchar *node_id;               /**< Unique node identifier */
    gdouble attention_capacity;   /**< Maximum attention this node can handle */
    gdouble current_attention;    /**< Current attention allocated to this node */
    gint64 last_sync_time;        /**< Last synchronization timestamp */
    GHashTable *neighbor_nodes;   /**< Map of neighbor node IDs to connection strengths */
    GncAttentionParams local_params; /**< Local attention parameters */
} GncAttentionMeshNode;

/** Initialize distributed attention mesh for multi-node processing
 * @param mesh_size Number of nodes in the mesh
 * @param total_mesh_attention Total attention pool for the mesh
 * @return TRUE on successful mesh initialization
 */
gboolean gnc_ecan_init_attention_mesh(gint mesh_size, gdouble total_mesh_attention);

/** Add a node to the distributed attention mesh
 * @param node_id Unique identifier for the node
 * @param attention_capacity Maximum attention capacity for this node
 * @return TRUE on successful node addition
 */
gboolean gnc_ecan_mesh_add_node(const gchar *node_id, gdouble attention_capacity);

/** Create connection between mesh nodes for attention flow
 * @param node_id1 First node identifier
 * @param node_id2 Second node identifier  
 * @param connection_strength Strength of connection (0.0-1.0)
 * @return TRUE on successful connection establishment
 */
gboolean gnc_ecan_mesh_connect_nodes(const gchar *node_id1, 
                                      const gchar *node_id2, 
                                      gdouble connection_strength);

/** Propagate attention changes across the mesh
 * @param source_node_id Node initiating the attention change
 * @param attention_change Amount of attention change (positive or negative)
 * @param propagation_depth Maximum depth for attention propagation
 */
void gnc_ecan_mesh_propagate_attention(const gchar *source_node_id, 
                                       gdouble attention_change,
                                       gint propagation_depth);

/** Synchronize attention state across all mesh nodes
 * @param force_sync TRUE to force synchronization even if recently synced
 * @return TRUE if synchronization was successful
 */
gboolean gnc_ecan_mesh_synchronize_attention(gboolean force_sync);

/** Balance attention load across mesh nodes to prevent bottlenecks
 * @param load_threshold Threshold above which load balancing is triggered
 * @return Number of attention transfers performed during balancing
 */
gint gnc_ecan_mesh_balance_attention_load(gdouble load_threshold);

/** Get mesh topology statistics for monitoring and optimization
 * @param total_nodes Output parameter for total number of nodes
 * @param total_connections Output parameter for total connections
 * @param avg_node_capacity Output parameter for average node attention capacity
 * @param mesh_utilization Output parameter for overall mesh utilization (0.0-1.0)
 */
void gnc_ecan_mesh_get_topology_stats(gint *total_nodes,
                                       gint *total_connections,
                                       gdouble *avg_node_capacity,
                                       gdouble *mesh_utilization);

/** Shutdown the distributed attention mesh and cleanup resources */
void gnc_ecan_mesh_shutdown(void);

/** @} */

/** @name Phase 2: Priority-Based Task Scheduling */
/** @{ */

/** Task priority levels for attention-driven scheduling */
typedef enum {
    GNC_TASK_PRIORITY_EMERGENCY = 1000,  /**< Emergency tasks (e.g., preventing attention starvation) */
    GNC_TASK_PRIORITY_HIGH = 750,        /**< High priority tasks (e.g., transaction validation) */
    GNC_TASK_PRIORITY_NORMAL = 500,      /**< Normal priority tasks (e.g., balance updates) */
    GNC_TASK_PRIORITY_LOW = 250,         /**< Low priority tasks (e.g., pattern discovery) */
    GNC_TASK_PRIORITY_BACKGROUND = 100   /**< Background tasks (e.g., cleanup, optimization) */
} GncTaskPriority;

/** Cognitive task for attention-driven scheduling */
typedef struct {
    gchar *task_id;                    /**< Unique task identifier */
    gchar *task_type;                  /**< Type of cognitive task */
    GncTaskPriority priority;          /**< Task priority level */
    gdouble attention_requirement;     /**< Required attention resources */
    gdouble attention_allocated;       /**< Currently allocated attention */
    gint64 creation_time;              /**< Task creation timestamp */
    gint64 deadline;                   /**< Task deadline (0 = no deadline) */
    gpointer task_data;                /**< Task-specific data */
    GDestroyNotify data_destroy_func;  /**< Function to destroy task data */
} GncCognitiveTask;

/** Initialize the attention-driven task scheduler
 * @param max_concurrent_tasks Maximum number of tasks that can run concurrently
 * @param scheduler_attention_pool Attention pool dedicated to task scheduling
 * @return TRUE on successful initialization
 */
gboolean gnc_ecan_scheduler_init(gint max_concurrent_tasks, gdouble scheduler_attention_pool);

/** Submit a cognitive task for attention-driven scheduling
 * @param task_type Type of task to schedule
 * @param priority Priority level for the task
 * @param attention_requirement Attention resources required
 * @param task_data Task-specific data
 * @param data_destroy_func Function to destroy task data
 * @param deadline Optional deadline for task completion (0 = no deadline)
 * @return Task ID on successful submission, NULL on failure
 */
gchar* gnc_ecan_scheduler_submit_task(const gchar *task_type,
                                      GncTaskPriority priority,
                                      gdouble attention_requirement,
                                      gpointer task_data,
                                      GDestroyNotify data_destroy_func,
                                      gint64 deadline);

/** Process pending tasks based on attention availability and priority
 * @param available_attention Amount of attention available for task processing
 * @return Number of tasks processed in this cycle
 */
gint gnc_ecan_scheduler_process_tasks(gdouble available_attention);

/** Cancel a scheduled task
 * @param task_id Task identifier to cancel
 * @return TRUE if task was found and cancelled, FALSE otherwise
 */
gboolean gnc_ecan_scheduler_cancel_task(const gchar *task_id);

/** Get task scheduler statistics
 * @param pending_tasks Output parameter for number of pending tasks
 * @param running_tasks Output parameter for number of running tasks
 * @param total_attention_allocated Output parameter for total attention allocated
 * @param scheduler_efficiency Output parameter for scheduler efficiency (0.0-1.0)
 */
void gnc_ecan_scheduler_get_stats(gint *pending_tasks,
                                  gint *running_tasks,
                                  gdouble *total_attention_allocated,
                                  gdouble *scheduler_efficiency);

/** Shutdown the task scheduler and cleanup resources */
void gnc_ecan_scheduler_shutdown(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* GNC_COGNITIVE_ACCOUNTING_H */
/** @} */
/** @} */