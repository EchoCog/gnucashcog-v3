/********************************************************************\
 * gnc-cognitive-primitives.cpp -- Foundational Cognitive Implementation *
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

/**
 * @file gnc-cognitive-primitives.cpp
 * @brief Phase 1: Implementation of Cognitive Primitives & Foundational Hypergraph Encoding
 */

#include <atomic>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "gnc-cognitive-primitives.h"
#include "gnc-cognitive-accounting.h"
#include "qof.h"

/** Static variables for system state */
static gboolean primitives_initialized = FALSE;
static GHashTable *primitive_registry = NULL;
static GHashTable *hypergraph_registry = NULL;
static GHashTable *agent_registry = NULL;
/* std::atomic rather than g_atomic_int_add(), which only operates on
 * 4-byte gint -- these counters are guint64. */
static std::atomic<guint64> next_primitive_id{1};
static std::atomic<guint64> next_hypergraph_node_id{1};
static GMutex primitive_mutex;

/** Forward declarations for internal functions */
static void initialize_primitive_registry(void);
static void cleanup_primitive_registry(void);
static GncCognitivePrimitive generate_primitive_id(void);
static GncHypergraphNode generate_hypergraph_node_id(void);
static void update_system_coherence(void);

/**
 * @name Core Primitive Operations Implementation
 */

gboolean gnc_cognitive_primitives_init(void)
{
    if (primitives_initialized) {
        g_warning("Cognitive primitives already initialized");
        return TRUE;
    }

    std::cout << "\n🧠 Initializing Phase 1: Cognitive Primitives & Foundational Hypergraph Encoding...\n";

    g_mutex_init(&primitive_mutex);
    
    // Initialize registries
    initialize_primitive_registry();
    
    // Create foundational primitive types
    GncCognitivePrimitiveUnit *account_primitive = gnc_cognitive_primitive_create(
        "AccountPrimitive", "FOUNDATIONAL", 0.8);
    GncCognitivePrimitiveUnit *transaction_primitive = gnc_cognitive_primitive_create(
        "TransactionPrimitive", "FOUNDATIONAL", 0.9);
    GncCognitivePrimitiveUnit *balance_primitive = gnc_cognitive_primitive_create(
        "BalancePrimitive", "FOUNDATIONAL", 0.7);
    
    // Suppress unused variable warnings for foundational primitives  
    (void)account_primitive;
    (void)transaction_primitive;
    (void)balance_primitive;
    
    primitives_initialized = TRUE;
    
    std::cout << "✅ Cognitive primitives system initialized\n";
    std::cout << "✅ Foundational hypergraph encoding ready\n";
    std::cout << "✅ Neural-symbolic bridge primitives active\n";
    std::cout << "✅ Distributed agentic coordination enabled\n";
    
    return TRUE;
}

void gnc_cognitive_primitives_shutdown(void)
{
    if (!primitives_initialized) {
        return;
    }

    std::cout << "🔄 Shutting down cognitive primitives system...\n";
    
    cleanup_primitive_registry();
    g_mutex_clear(&primitive_mutex);
    
    primitives_initialized = FALSE;
    std::cout << "✅ Cognitive primitives shutdown complete\n";
}

GncCognitivePrimitiveUnit* gnc_cognitive_primitive_create(
    const gchar *name,
    const gchar *type,
    gdouble initial_activation)
{
    g_return_val_if_fail(name != NULL, NULL);
    g_return_val_if_fail(type != NULL, NULL);
    g_return_val_if_fail(initial_activation >= 0.0 && initial_activation <= 1.0, NULL);
    
    GncCognitivePrimitiveUnit *primitive = g_new0(GncCognitivePrimitiveUnit, 1);
    
    g_mutex_lock(&primitive_mutex);
    
    primitive->id = generate_primitive_id();
    primitive->name = g_strdup(name);
    primitive->type = g_strdup(type);
    primitive->activation_level = initial_activation;
    primitive->confidence = 0.5; // Default confidence
    primitive->properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    primitive->last_updated = g_get_real_time();
    
    // Register in global registry
    if (primitive_registry) {
        g_hash_table_insert(primitive_registry, 
                          GUINT_TO_POINTER(primitive->id), 
                          primitive);
    }
    
    g_mutex_unlock(&primitive_mutex);
    
    return primitive;
}

void gnc_cognitive_primitive_free(GncCognitivePrimitiveUnit *primitive)
{
    if (!primitive) return;
    
    g_mutex_lock(&primitive_mutex);
    
    // Remove from registry
    if (primitive_registry) {
        g_hash_table_remove(primitive_registry, GUINT_TO_POINTER(primitive->id));
    }
    
    g_free(primitive->name);
    g_free(primitive->type);
    if (primitive->properties) {
        g_hash_table_destroy(primitive->properties);
    }
    
    g_free(primitive);
    
    g_mutex_unlock(&primitive_mutex);
}

void gnc_cognitive_primitive_update_activation(
    GncCognitivePrimitiveUnit *primitive,
    gdouble stimulus_strength,
    gdouble decay_factor)
{
    g_return_if_fail(primitive != NULL);
    g_return_if_fail(decay_factor >= 0.0 && decay_factor <= 1.0);
    
    g_mutex_lock(&primitive_mutex);
    
    // Apply decay
    primitive->activation_level *= (1.0 - decay_factor);
    
    // Apply stimulus with non-linear activation function (sigmoid)
    gdouble sigmoid_input = stimulus_strength * 6.0 - 3.0; // Scale to [-3, 3]
    gdouble sigmoid_output = 1.0 / (1.0 + exp(-sigmoid_input));
    
    // Combine current activation with new stimulus
    primitive->activation_level = 0.7 * primitive->activation_level + 0.3 * sigmoid_output;
    
    // Clamp to [0, 1] range
    if (primitive->activation_level > 1.0) primitive->activation_level = 1.0;
    if (primitive->activation_level < 0.0) primitive->activation_level = 0.0;
    
    primitive->last_updated = g_get_real_time();
    
    g_mutex_unlock(&primitive_mutex);
}

const gchar* gnc_cognitive_primitive_get_property(
    GncCognitivePrimitiveUnit *primitive,
    const gchar *property_name)
{
    g_return_val_if_fail(primitive != NULL, NULL);
    g_return_val_if_fail(property_name != NULL, NULL);
    
    if (!primitive->properties) return NULL;
    
    return (const gchar*)g_hash_table_lookup(primitive->properties, property_name);
}

void gnc_cognitive_primitive_set_property(
    GncCognitivePrimitiveUnit *primitive,
    const gchar *property_name,
    const gchar *property_value)
{
    g_return_if_fail(primitive != NULL);
    g_return_if_fail(property_name != NULL);
    
    if (!primitive->properties) {
        primitive->properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    }
    
    g_hash_table_insert(primitive->properties, 
                       g_strdup(property_name), 
                       g_strdup(property_value));
}

/**
 * @name Hypergraph Operations Implementation
 */

GncFinancialHypergraph* gnc_financial_hypergraph_create(
    const gchar *graph_type,
    guint max_nodes,
    guint max_edges)
{
    g_return_val_if_fail(graph_type != NULL, NULL);
    g_return_val_if_fail(max_nodes > 0, NULL);
    g_return_val_if_fail(max_edges > 0, NULL);
    
    GncFinancialHypergraph *hypergraph = g_new0(GncFinancialHypergraph, 1);
    
    hypergraph->nodes = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    hypergraph->edges = NULL;
    hypergraph->graph_type = g_strdup(graph_type);
    hypergraph->coherence_measure = 0.5; // Initial coherence
    hypergraph->max_nodes = max_nodes;
    hypergraph->max_edges = max_edges;
    
    // Register in hypergraph registry
    g_mutex_lock(&primitive_mutex);
    if (hypergraph_registry) {
        g_hash_table_insert(hypergraph_registry, hypergraph, hypergraph);
    }
    g_mutex_unlock(&primitive_mutex);
    
    return hypergraph;
}

void gnc_financial_hypergraph_free(GncFinancialHypergraph *hypergraph)
{
    if (!hypergraph) return;
    
    g_mutex_lock(&primitive_mutex);
    
    // Remove from registry
    if (hypergraph_registry) {
        g_hash_table_remove(hypergraph_registry, hypergraph);
    }
    
    if (hypergraph->nodes) {
        g_hash_table_destroy(hypergraph->nodes);
    }
    
    // Free edges
    g_list_free_full(hypergraph->edges, (GDestroyNotify)g_free);
    
    g_free(hypergraph->graph_type);
    g_free(hypergraph);
    
    g_mutex_unlock(&primitive_mutex);
}

GncHypergraphNode gnc_financial_hypergraph_add_node(
    GncFinancialHypergraph *hypergraph,
    const gchar *node_type,
    gpointer node_data)
{
    g_return_val_if_fail(hypergraph != NULL, 0);
    g_return_val_if_fail(node_type != NULL, 0);
    
    g_mutex_lock(&primitive_mutex);
    
    // Check capacity
    if (g_hash_table_size(hypergraph->nodes) >= hypergraph->max_nodes) {
        g_mutex_unlock(&primitive_mutex);
        g_warning("Hypergraph node capacity exceeded");
        return 0;
    }
    
    GncHypergraphNode node_id = generate_hypergraph_node_id();
    
    // Create node data structure
    gchar *node_info = g_strdup_printf("%s:%p", node_type, node_data);
    g_hash_table_insert(hypergraph->nodes, GUINT_TO_POINTER(node_id), node_info);
    
    // Update coherence
    update_system_coherence();
    
    g_mutex_unlock(&primitive_mutex);
    
    return node_id;
}

GncHypergraphEdge* gnc_financial_hypergraph_create_edge(
    GncFinancialHypergraph *hypergraph,
    GncHypergraphNode *nodes,
    guint node_count,
    const gchar *relation_type,
    gdouble weight)
{
    g_return_val_if_fail(hypergraph != NULL, NULL);
    g_return_val_if_fail(nodes != NULL, NULL);
    g_return_val_if_fail(node_count > 1, NULL);
    g_return_val_if_fail(relation_type != NULL, NULL);
    
    // Check edge capacity
    if (g_list_length(hypergraph->edges) >= hypergraph->max_edges) {
        g_warning("Hypergraph edge capacity exceeded");
        return NULL;
    }
    
    GncHypergraphEdge *edge = g_new0(GncHypergraphEdge, 1);
    edge->nodes = g_new(GncHypergraphNode, node_count);
    edge->node_count = node_count;
    edge->weight = weight;
    edge->relation_type = g_strdup(relation_type);
    
    // Copy node IDs
    for (guint i = 0; i < node_count; i++) {
        edge->nodes[i] = nodes[i];
    }
    
    g_mutex_lock(&primitive_mutex);
    hypergraph->edges = g_list_append(hypergraph->edges, edge);
    g_mutex_unlock(&primitive_mutex);
    
    return edge;
}

GncFinancialHypergraph* gnc_account_to_hypergraph_pattern(
    Account *account,
    gboolean include_children)
{
    g_return_val_if_fail(account != NULL, NULL);
    
    GncFinancialHypergraph *pattern = gnc_financial_hypergraph_create(
        "AccountPattern", 100, 200);
    
    // Create node for the account
    GncHypergraphNode account_node = gnc_financial_hypergraph_add_node(
        pattern, "Account", account);
    
    // Add balance node
    GncHypergraphNode balance_node = gnc_financial_hypergraph_add_node(
        pattern, "Balance", NULL);
    
    // Create edge connecting account to balance
    GncHypergraphNode edge_nodes[2] = {account_node, balance_node};
    gnc_financial_hypergraph_create_edge(pattern, edge_nodes, 2, "has_balance", 1.0);
    
    // Include children if requested
    if (include_children) {
        GList *children = gnc_account_get_children(account);
        for (GList *child_iter = children; child_iter; child_iter = child_iter->next) {
            Account *child = (Account*)child_iter->data;
            GncHypergraphNode child_node = gnc_financial_hypergraph_add_node(
                pattern, "ChildAccount", child);
            
            // Create parent-child relationship
            GncHypergraphNode parent_child_nodes[2] = {account_node, child_node};
            gnc_financial_hypergraph_create_edge(pattern, parent_child_nodes, 2, "parent_of", 0.8);
        }
        g_list_free(children);
    }
    
    return pattern;
}

GncFinancialHypergraph* gnc_transaction_to_hypergraph_pattern(
    Transaction *transaction,
    gboolean include_splits)
{
    g_return_val_if_fail(transaction != NULL, NULL);
    
    GncFinancialHypergraph *pattern = gnc_financial_hypergraph_create(
        "TransactionPattern", 50, 100);
    
    // Create node for transaction
    GncHypergraphNode trans_node = gnc_financial_hypergraph_add_node(
        pattern, "Transaction", transaction);
    
    if (include_splits) {
        GList *splits = xaccTransGetSplitList(transaction);
        GArray *split_nodes = g_array_new(FALSE, FALSE, sizeof(GncHypergraphNode));
        
        for (GList *split_iter = splits; split_iter; split_iter = split_iter->next) {
            Split *split = (Split*)split_iter->data;
            GncHypergraphNode split_node = gnc_financial_hypergraph_add_node(
                pattern, "Split", split);
            g_array_append_val(split_nodes, split_node);
            
            // Connect transaction to split
            GncHypergraphNode trans_split_nodes[2] = {trans_node, split_node};
            gnc_financial_hypergraph_create_edge(pattern, trans_split_nodes, 2, "contains_split", 1.0);
        }
        
        // Create hyperedge connecting all splits (double-entry constraint)
        if (split_nodes->len > 1) {
            gnc_financial_hypergraph_create_edge(pattern, 
                                               (GncHypergraphNode*)split_nodes->data,
                                               split_nodes->len,
                                               "double_entry_constraint", 
                                               1.0);
        }
        
        g_array_free(split_nodes, TRUE);
    }
    
    return pattern;
}

gdouble gnc_financial_hypergraph_coherence(GncFinancialHypergraph *hypergraph)
{
    g_return_val_if_fail(hypergraph != NULL, 0.0);
    
    guint node_count = g_hash_table_size(hypergraph->nodes);
    guint edge_count = g_list_length(hypergraph->edges);
    
    if (node_count == 0) return 0.0;
    
    // Calculate coherence based on connectivity and structure
    gdouble connectivity_ratio = (gdouble)edge_count / (gdouble)node_count;
    gdouble structure_penalty = 0.0;
    
    // Penalty for overly sparse or dense graphs
    if (connectivity_ratio < 0.5) {
        structure_penalty = (0.5 - connectivity_ratio) * 0.3;
    } else if (connectivity_ratio > 3.0) {
        structure_penalty = (connectivity_ratio - 3.0) * 0.1;
    }
    
    gdouble base_coherence = tanh(connectivity_ratio); // Use tanh for bounded output
    gdouble coherence = base_coherence - structure_penalty;
    
    // Clamp to [0, 1]
    if (coherence < 0.0) coherence = 0.0;
    if (coherence > 1.0) coherence = 1.0;
    
    // Update internal coherence measure
    g_mutex_lock(&primitive_mutex);
    hypergraph->coherence_measure = coherence;
    g_mutex_unlock(&primitive_mutex);
    
    return coherence;
}

/**
 * @name Grammar Pattern Operations Implementation
 */

GncCognitiveGrammarPattern* gnc_cognitive_grammar_pattern_create(
    const gchar *pattern_name,
    const gchar *grammar_rule)
{
    g_return_val_if_fail(pattern_name != NULL, NULL);
    g_return_val_if_fail(grammar_rule != NULL, NULL);
    
    GncCognitiveGrammarPattern *pattern = g_new0(GncCognitiveGrammarPattern, 1);
    
    pattern->pattern_name = g_strdup(pattern_name);
    pattern->grammar_rule = g_strdup(grammar_rule);
    pattern->components = NULL;
    pattern->component_count = 0;
    pattern->pattern_strength = 0.5;
    pattern->semantic_coherence = 0.5;
    
    return pattern;
}

void gnc_cognitive_grammar_pattern_free(GncCognitiveGrammarPattern *pattern)
{
    if (!pattern) return;
    
    g_free(pattern->pattern_name);
    g_free(pattern->grammar_rule);
    g_free(pattern->components);
    g_free(pattern);
}

void gnc_cognitive_grammar_pattern_add_component(
    GncCognitiveGrammarPattern *pattern,
    GncCognitivePrimitive component)
{
    g_return_if_fail(pattern != NULL);
    
    // Resize component array
    pattern->component_count++;
    pattern->components = (GncCognitivePrimitive *)g_realloc(pattern->components, 
                                  sizeof(GncCognitivePrimitive) * pattern->component_count);
    pattern->components[pattern->component_count - 1] = component;
}

gdouble gnc_cognitive_grammar_pattern_match(
    GncCognitiveGrammarPattern *pattern,
    gpointer financial_data,
    const gchar *data_type)
{
    g_return_val_if_fail(pattern != NULL, 0.0);
    g_return_val_if_fail(financial_data != NULL, 0.0);
    g_return_val_if_fail(data_type != NULL, 0.0);
    
    // Simple pattern matching based on data type and pattern complexity
    gdouble type_match = 0.0;
    
    if (g_strcmp0(data_type, "Account") == 0) {
        type_match = 0.8;
    } else if (g_strcmp0(data_type, "Transaction") == 0) {
        type_match = 0.9;
    } else if (g_strcmp0(data_type, "Split") == 0) {
        type_match = 0.7;
    } else {
        type_match = 0.3; // Unknown type
    }
    
    // Combine with pattern strength and semantic coherence
    gdouble match_score = type_match * pattern->pattern_strength * pattern->semantic_coherence;
    
    return match_score;
}

GList* gnc_generate_financial_grammar_rules(Account *root_account, guint max_depth)
{
    g_return_val_if_fail(root_account != NULL, NULL);
    
    GList *rules = NULL;
    
    // Create basic account hierarchy rule
    GncCognitiveGrammarPattern *hierarchy_rule = gnc_cognitive_grammar_pattern_create(
        "AccountHierarchy", "ACCOUNT -> PARENT_ACCOUNT CHILD_ACCOUNTS*");
    hierarchy_rule->pattern_strength = 0.9;
    hierarchy_rule->semantic_coherence = 0.8;
    rules = g_list_append(rules, hierarchy_rule);
    
    // Create transaction-split rule
    GncCognitiveGrammarPattern *transaction_rule = gnc_cognitive_grammar_pattern_create(
        "TransactionStructure", "TRANSACTION -> SPLIT+ WHERE sum(SPLIT.value) = 0");
    transaction_rule->pattern_strength = 1.0;
    transaction_rule->semantic_coherence = 0.95;
    rules = g_list_append(rules, transaction_rule);
    
    // Create balance validation rule
    GncCognitiveGrammarPattern *balance_rule = gnc_cognitive_grammar_pattern_create(
        "BalanceValidation", "ACCOUNT_BALANCE -> sum(TRANSACTION.splits[account])");
    balance_rule->pattern_strength = 0.85;
    balance_rule->semantic_coherence = 0.9;
    rules = g_list_append(rules, balance_rule);
    
    return rules;
}

/**
 * @name Neural-Symbolic Bridge Operations Implementation
 */

GncNeuralSymbolicEmbedding* gnc_neural_symbolic_embedding_create(
    GncCognitivePrimitive concept_id,
    guint vector_dimension)
{
    g_return_val_if_fail(vector_dimension > 0, NULL);
    
    GncNeuralSymbolicEmbedding *embedding = g_new0(GncNeuralSymbolicEmbedding, 1);
    
    embedding->concept_id = concept_id;
    embedding->vector_dimension = vector_dimension;
    embedding->embedding_vector = g_new(gdouble, vector_dimension);
    embedding->symbolic_representation = g_strdup_printf("concept_%" G_GUINT64_FORMAT, concept_id);
    embedding->embedding_confidence = 0.5;
    
    // Initialize with random values (simple initialization)
    for (guint i = 0; i < vector_dimension; i++) {
        embedding->embedding_vector[i] = (gdouble)rand() / RAND_MAX - 0.5; // Range [-0.5, 0.5]
    }
    
    return embedding;
}

void gnc_neural_symbolic_embedding_free(GncNeuralSymbolicEmbedding *embedding)
{
    if (!embedding) return;
    
    g_free(embedding->embedding_vector);
    g_free(embedding->symbolic_representation);
    g_free(embedding);
}

void gnc_neural_symbolic_embedding_update(
    GncNeuralSymbolicEmbedding *embedding,
    gdouble *context_vector,
    gdouble learning_rate)
{
    g_return_if_fail(embedding != NULL);
    g_return_if_fail(context_vector != NULL);
    g_return_if_fail(learning_rate > 0.0 && learning_rate < 1.0);
    
    // Simple gradient-like update
    for (guint i = 0; i < embedding->vector_dimension; i++) {
        embedding->embedding_vector[i] += learning_rate * 
            (context_vector[i] - embedding->embedding_vector[i]);
    }
    
    // Update confidence based on stability of updates
    gdouble update_magnitude = 0.0;
    for (guint i = 0; i < embedding->vector_dimension; i++) {
        gdouble diff = context_vector[i] - embedding->embedding_vector[i];
        update_magnitude += diff * diff;
    }
    update_magnitude = sqrt(update_magnitude);
    
    // Lower update magnitude indicates more stable embedding
    embedding->embedding_confidence = 1.0 / (1.0 + update_magnitude);
}

/**
 * @name System Integration Implementation
 */

GList* gnc_account_to_cognitive_primitives(Account *account, gboolean recursive)
{
    g_return_val_if_fail(account != NULL, NULL);
    
    GList *primitives = NULL;
    
    // Create primitive for the account itself
    gchar *account_name = g_strdup_printf("Account_%s", xaccAccountGetName(account));
    GncCognitivePrimitiveUnit *account_primitive = gnc_cognitive_primitive_create(
        account_name, "ACCOUNT", 0.8);
    
    // Set account-specific properties
    gnc_cognitive_primitive_set_property(account_primitive, "account_type", 
                                       xaccAccountTypeEnumAsString(xaccAccountGetType(account)));
    gnc_cognitive_primitive_set_property(account_primitive, "account_code", 
                                       xaccAccountGetCode(account));
    
    primitives = g_list_append(primitives, account_primitive);
    g_free(account_name);
    
    // Add children if recursive
    if (recursive) {
        GList *children = gnc_account_get_children(account);
        for (GList *child_iter = children; child_iter; child_iter = child_iter->next) {
            GList *child_primitives = gnc_account_to_cognitive_primitives(
                (Account*)child_iter->data, TRUE);
            primitives = g_list_concat(primitives, child_primitives);
        }
        g_list_free(children);
    }
    
    return primitives;
}

GList* gnc_transaction_to_cognitive_primitives(Transaction *transaction)
{
    g_return_val_if_fail(transaction != NULL, NULL);
    
    GList *primitives = NULL;
    
    // Create primitive for the transaction
    gchar *trans_name = g_strdup_printf("Transaction_%s", 
                                       xaccTransGetDescription(transaction));
    GncCognitivePrimitiveUnit *trans_primitive = gnc_cognitive_primitive_create(
        trans_name, "TRANSACTION", 0.9);
    
    primitives = g_list_append(primitives, trans_primitive);
    g_free(trans_name);
    
    // Create primitives for splits
    GList *splits = xaccTransGetSplitList(transaction);
    for (GList *split_iter = splits; split_iter; split_iter = split_iter->next) {
        Split *split = (Split*)split_iter->data;
        gchar *split_name = g_strdup_printf("Split_%p", split);
        
        GncCognitivePrimitiveUnit *split_primitive = gnc_cognitive_primitive_create(
            split_name, "SPLIT", 0.7);
        
        primitives = g_list_append(primitives, split_primitive);
        g_free(split_name);
    }
    
    return primitives;
}

GncCognitivePrimitiveStats* gnc_cognitive_primitives_get_stats(void)
{
    if (!primitives_initialized) return NULL;
    
    GncCognitivePrimitiveStats *stats = g_new0(GncCognitivePrimitiveStats, 1);
    
    g_mutex_lock(&primitive_mutex);
    
    stats->total_primitives = primitive_registry ? g_hash_table_size(primitive_registry) : 0;
    stats->hypergraph_nodes = 0;
    stats->hypergraph_edges = 0;
    stats->active_primitives = 0;
    stats->average_activation = 0.0;
    stats->system_coherence = 0.0;
    stats->grammar_patterns = 0;
    stats->distributed_agents = agent_registry ? g_hash_table_size(agent_registry) : 0;
    
    // Calculate active primitives and average activation
    if (primitive_registry) {
        GHashTableIter iter;
        gpointer key, value;
        gdouble total_activation = 0.0;
        
        g_hash_table_iter_init(&iter, primitive_registry);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            GncCognitivePrimitiveUnit *primitive = (GncCognitivePrimitiveUnit*)value;
            if (primitive->activation_level > 0.1) {
                stats->active_primitives++;
            }
            total_activation += primitive->activation_level;
        }
        
        if (stats->total_primitives > 0) {
            stats->average_activation = total_activation / stats->total_primitives;
        }
    }
    
    // Calculate hypergraph statistics
    if (hypergraph_registry) {
        GHashTableIter iter;
        gpointer key, value;
        gdouble total_coherence = 0.0;
        guint coherence_count = 0;
        
        g_hash_table_iter_init(&iter, hypergraph_registry);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            GncFinancialHypergraph *hypergraph = (GncFinancialHypergraph*)value;
            stats->hypergraph_nodes += g_hash_table_size(hypergraph->nodes);
            stats->hypergraph_edges += g_list_length(hypergraph->edges);
            total_coherence += hypergraph->coherence_measure;
            coherence_count++;
        }
        
        if (coherence_count > 0) {
            stats->system_coherence = total_coherence / coherence_count;
        }
    }
    
    g_mutex_unlock(&primitive_mutex);
    
    return stats;
}

void gnc_cognitive_primitive_stats_free(GncCognitivePrimitiveStats *stats)
{
    if (stats) {
        g_free(stats);
    }
}

/**
 * @name Internal Helper Functions
 */

static void initialize_primitive_registry(void)
{
    primitive_registry = g_hash_table_new_full(g_direct_hash, g_direct_equal, 
                                             NULL, NULL); // Values freed manually
    hypergraph_registry = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                               NULL, NULL);
    agent_registry = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                         NULL, NULL);
}

static void cleanup_primitive_registry(void)
{
    if (primitive_registry) {
        // Free all primitives
        GHashTableIter iter;
        gpointer key, value;
        
        g_hash_table_iter_init(&iter, primitive_registry);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            GncCognitivePrimitiveUnit *primitive = (GncCognitivePrimitiveUnit*)value;
            // Clear from registry first to avoid double-free
            g_hash_table_iter_steal(&iter);
            gnc_cognitive_primitive_free(primitive);
        }
        
        g_hash_table_destroy(primitive_registry);
        primitive_registry = NULL;
    }
    
    if (hypergraph_registry) {
        g_hash_table_destroy(hypergraph_registry);
        hypergraph_registry = NULL;
    }
    
    if (agent_registry) {
        g_hash_table_destroy(agent_registry);
        agent_registry = NULL;
    }
}

static GncCognitivePrimitive generate_primitive_id(void)
{
    return next_primitive_id.fetch_add(1);
}

static GncHypergraphNode generate_hypergraph_node_id(void)
{
    return next_hypergraph_node_id.fetch_add(1);
}

static void update_system_coherence(void)
{
    // This would be implemented to update overall system coherence
    // based on the current state of all hypergraphs and primitives
    // For now, it's a placeholder for future implementation
}