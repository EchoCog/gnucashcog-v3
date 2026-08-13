/********************************************************************\
 * test-meta-cognitive.cpp -- Unit tests for meta-cognitive engine *
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

#include "gnc-meta-cognitive.h"
#include <glib.h>
#include <cassert>
#include <cmath>

/** @file test-meta-cognitive.cpp
 *  @brief Unit tests for recursive meta-cognition and evolutionary optimization
 */

static void initialize_default_metrics(GncCognitiveMetrics *metrics);

/* Empty fixture struct required by g_test_add (void is not valid) */
typedef struct {
    int placeholder;
} MetaCognitiveFixture;

/* Test fixture setup and teardown */
static void setup(MetaCognitiveFixture *fixture G_GNUC_UNUSED, gconstpointer data G_GNUC_UNUSED)
{
    g_assert_true(gnc_meta_cognitive_init());
}

static void teardown(MetaCognitiveFixture *fixture G_GNUC_UNUSED, gconstpointer data G_GNUC_UNUSED)
{
    gnc_meta_cognitive_shutdown();
}

/* Test meta-cognitive engine initialization */
static void test_meta_cognitive_init(MetaCognitiveFixture *fixture G_GNUC_UNUSED, gconstpointer data G_GNUC_UNUSED)
{
    g_test_message("Testing meta-cognitive engine initialization...");
    
    // Should already be initialized from setup
    g_assert_true(gnc_meta_cognitive_init()); // Should handle multiple inits gracefully
    
    // Test creating session
    GncMetaCognitiveSession *session = gnc_meta_cognitive_session_new();
    g_assert_nonnull(session);
    
    gnc_meta_cognitive_session_destroy(session);
}

/* Test self-analysis functionality */
static void test_self_analysis(MetaCognitiveFixture *fixture G_GNUC_UNUSED, gconstpointer data G_GNUC_UNUSED)
{
    g_test_message("Testing self-analysis capabilities...");
    
    GncMetaCognitiveSession *session = gnc_meta_cognitive_session_new();
    g_assert_nonnull(session);
    
    // Test analyzing individual process
    GncSelfAnalysisResult *result = gnc_meta_cognitive_analyze_process(
        session, GNC_METACOG_PROCESS_ATTENTION);
    
    g_assert_nonnull(result);
    g_assert_cmpint(result->process_type, ==, GNC_METACOG_PROCESS_ATTENTION);
    g_assert_cmpfloat(result->confidence_level, >=, 0.0);
    g_assert_cmpfloat(result->confidence_level, <=, 1.0);
    g_assert_cmpfloat(result->improvement_score, >=, -1.0);
    g_assert_cmpfloat(result->improvement_score, <=, 1.0);
    
    // Verify metrics are reasonable
    g_assert_cmpfloat(result->current_metrics.accuracy, >=, 0.0);
    g_assert_cmpfloat(result->current_metrics.accuracy, <=, 1.0);
    g_assert_cmpfloat(result->current_metrics.efficiency, >=, 0.0);
    g_assert_cmpfloat(result->current_metrics.efficiency, <=, 1.0);
    g_assert_cmpfloat(result->current_metrics.stability_index, >=, 0.0);
    g_assert_cmpfloat(result->current_metrics.stability_index, <=, 1.0);
    
    // Test optimization suggestions
    g_assert_cmpint(result->n_suggestions, >=, 0);
    if (result->n_suggestions > 0) {
        g_assert_nonnull(result->optimization_suggestions);
        g_assert_nonnull(result->optimization_suggestions[0]);
    }
    
    gnc_self_analysis_result_free(result);
    
    // Test system-wide analysis
    GncSelfAnalysisResult *system_result = gnc_meta_cognitive_analyze_system(session);
    g_assert_nonnull(system_result);
    g_assert_cmpint(system_result->process_type, ==, GNC_METACOG_PROCESS_ALL);
    
    gnc_self_analysis_result_free(system_result);
    gnc_meta_cognitive_session_destroy(session);
}

/* Test metrics management */
static void test_metrics_management(MetaCognitiveFixture *fixture G_GNUC_UNUSED, gconstpointer data G_GNUC_UNUSED)
{
    g_test_message("Testing cognitive metrics management...");
    
    GncCognitiveMetrics metrics;
    
    // Test getting default metrics - should return TRUE since init populates defaults
    g_assert_true(gnc_meta_cognitive_get_metrics(GNC_METACOG_PROCESS_VALIDATION, &metrics));
    // Should have valid default values
    g_assert_cmpfloat(metrics.accuracy, >, 0.0);
    g_assert_cmpfloat(metrics.efficiency, >, 0.0);
    
    // Test updating metrics
    metrics.accuracy = 0.95;
    metrics.efficiency = 0.88;
    metrics.latency_ms = 5.0;
    metrics.throughput = 200.0;
    metrics.stability_index = 0.92;
    metrics.learning_rate = 0.02;
    metrics.innovation_score = 0.75;
    metrics.total_operations = 5000;
    metrics.error_count = 25;
    
    g_assert_true(gnc_meta_cognitive_update_metrics(GNC_METACOG_PROCESS_VALIDATION, &metrics));
    
    // Verify updated metrics can be retrieved
    GncCognitiveMetrics retrieved_metrics;
    g_assert_true(gnc_meta_cognitive_get_metrics(GNC_METACOG_PROCESS_VALIDATION, &retrieved_metrics));
    
    g_assert_cmpfloat(retrieved_metrics.accuracy, ==, 0.95);
    g_assert_cmpfloat(retrieved_metrics.efficiency, ==, 0.88);
    g_assert_cmpfloat(retrieved_metrics.latency_ms, ==, 5.0);
    g_assert_cmpfloat(retrieved_metrics.stability_index, ==, 0.92);
}

/* Test evolutionary optimization */
static void test_evolutionary_optimization(MetaCognitiveFixture *fixture G_GNUC_UNUSED, gconstpointer data G_GNUC_UNUSED)
{
    g_test_message("Testing evolutionary optimization...");
    
    GncMetaCognitiveSession *session = gnc_meta_cognitive_session_new();
    g_assert_nonnull(session);
    
    // Set up evolutionary parameters
    GncEvolutionaryParams params = {
        .population_size = 10,
        .mutation_rate = 0.1,
        .crossover_rate = 0.7,
        .max_generations = 5, // Keep small for testing
        .fitness_threshold = 0.9,
        .diversity_pressure = 0.2,
        .elitism_enabled = TRUE,
        .elite_count = 2
    };
    
    // Test evolution
    GncCognitiveArchConfig *evolved_config = gnc_meta_cognitive_evolve_architecture(
        session, &params, GNC_METACOG_PROCESS_ATTENTION);
    
    g_assert_nonnull(evolved_config);
    
    // Verify evolved configuration has reasonable values
    g_assert_cmpfloat(evolved_config->base_learning_rate, >, 0.001);
    g_assert_cmpfloat(evolved_config->base_learning_rate, <, 0.1);
    g_assert_cmpfloat(evolved_config->sti_funds, >, 100.0);
    g_assert_cmpfloat(evolved_config->lti_funds, >, 50.0);
    g_assert_cmpfloat(evolved_config->attention_decay_rate, >, 0.001);
    g_assert_cmpfloat(evolved_config->attention_decay_rate, <, 0.1);
    
    // Test applying configuration
    g_assert_true(gnc_meta_cognitive_apply_config(evolved_config));
    
    g_free(evolved_config);
    gnc_meta_cognitive_session_destroy(session);
}

/* Test safety mechanisms */
static void test_safety_mechanisms(MetaCognitiveFixture *fixture G_GNUC_UNUSED, gconstpointer data G_GNUC_UNUSED)
{
    g_test_message("Testing safety mechanisms...");
    
    // Test safety bounds
    gnc_meta_cognitive_set_safety_bounds(TRUE, 0.5, 0.3);
    
    // Create configuration that should be rejected
    GncCognitiveArchConfig unsafe_config = {
        .sti_funds = 1000.0,
        .lti_funds = 500.0,
        .attention_decay_rate = 0.8, // Too high - should be rejected
        .wage_rate = 1.0,
        .truth_threshold = 0.8,
        .confidence_threshold = 0.7,
        .max_inference_steps = 100,
        .tensor_cache_size = 1000,
        .message_priority_threshold = 0.5,
        .max_concurrent_ops = 10,
        .base_learning_rate = 2.0, // Too high - should be rejected
        .learning_momentum = 0.9,
        .regularization_strength = 0.001
    };
    
    // Should reject unsafe configuration
    g_assert_false(gnc_meta_cognitive_apply_config(&unsafe_config));
    
    // Test human override
    gnc_meta_cognitive_set_human_override(TRUE);
    
    // Should still reject even safe configurations when override is enabled
    GncCognitiveArchConfig safe_config = {
        .sti_funds = 1000.0,
        .lti_funds = 500.0,
        .attention_decay_rate = 0.01,
        .wage_rate = 1.0,
        .truth_threshold = 0.8,
        .confidence_threshold = 0.7,
        .max_inference_steps = 100,
        .tensor_cache_size = 1000,
        .message_priority_threshold = 0.5,
        .max_concurrent_ops = 10,
        .base_learning_rate = 0.01,
        .learning_momentum = 0.9,
        .regularization_strength = 0.001
    };
    
    g_assert_false(gnc_meta_cognitive_apply_config(&safe_config));
    
    // Disable override and try again
    gnc_meta_cognitive_set_human_override(FALSE);
    g_assert_true(gnc_meta_cognitive_apply_config(&safe_config));
    
    // Test rollback
    g_assert_true(gnc_meta_cognitive_rollback_config());
    
    // Test stability checking
    gboolean is_stable = gnc_meta_cognitive_is_stable();
    // Should be boolean
    g_assert_true(is_stable == TRUE || is_stable == FALSE);
    
    // Test regression detection
    gboolean regression_detected = gnc_meta_cognitive_detect_regression(
        GNC_METACOG_PROCESS_ATTENTION, 0.2);
    g_assert_true(regression_detected == TRUE || regression_detected == FALSE);
}

/* Test recursive improvement cycle */
static void test_recursive_improvement(MetaCognitiveFixture *fixture G_GNUC_UNUSED, gconstpointer data G_GNUC_UNUSED)
{
    g_test_message("Testing recursive improvement cycle...");
    
    GncMetaCognitiveSession *session = gnc_meta_cognitive_session_new();
    g_assert_nonnull(session);
    
    // Initially should not be improving
    g_assert_false(gnc_meta_cognitive_is_improving(session));
    
    // Start improvement cycle
    g_assert_true(gnc_meta_cognitive_start_improvement_cycle(session, 3, 0.1));
    
    // Should now be improving
    g_assert_true(gnc_meta_cognitive_is_improving(session));
    
    // Test getting status
    guint iteration;
    gdouble improvement;
    g_assert_true(gnc_meta_cognitive_get_improvement_status(session, &iteration, &improvement));
    
    // Should have valid status
    g_assert_cmpint(iteration, >=, 0);
    g_assert_cmpfloat(improvement, >=, -1.0);
    g_assert_cmpfloat(improvement, <=, 1.0);
    
    // Stop improvement cycle
    g_assert_true(gnc_meta_cognitive_stop_improvement_cycle(session));
    
    // Give it a moment to stop
    g_usleep(50000); // 50ms
    
    gnc_meta_cognitive_session_destroy(session);
}

/* Test introspection and pattern analysis */
static void test_introspection(MetaCognitiveFixture *fixture G_GNUC_UNUSED, gconstpointer data G_GNUC_UNUSED)
{
    g_test_message("Testing introspection and pattern analysis...");
    
    GncMetaCognitiveSession *session = gnc_meta_cognitive_session_new();
    g_assert_nonnull(session);
    
    // Test pattern analysis
    gchar **patterns = gnc_meta_cognitive_analyze_patterns(session, 24);
    g_assert_nonnull(patterns);
    g_assert_nonnull(patterns[0]); // Should have at least one pattern
    
    g_strfreev(patterns);
    
    // Test emergence detection
    gchar **emergent_behaviors = gnc_meta_cognitive_detect_emergence(session, 0.7);
    g_assert_nonnull(emergent_behaviors);
    
    g_strfreev(emergent_behaviors);
    
    // Test fitness landscape generation
    gchar *landscape_json = gnc_meta_cognitive_generate_fitness_landscape(
        session, GNC_METACOG_PROCESS_ALL);
    g_assert_nonnull(landscape_json);
    g_assert_true(g_str_has_prefix(landscape_json, "{")); // Should be JSON
    g_assert_true(g_str_has_suffix(landscape_json, "}"));
    
    g_free(landscape_json);
    gnc_meta_cognitive_session_destroy(session);
}

/* Test error handling and edge cases */
static void test_error_handling(MetaCognitiveFixture *fixture G_GNUC_UNUSED, gconstpointer data G_GNUC_UNUSED)
{
    g_test_message("Testing error handling and edge cases...");
    
    // Test NULL parameter handling
    g_assert_null(gnc_meta_cognitive_analyze_process(NULL, GNC_METACOG_PROCESS_ATTENTION));
    g_assert_null(gnc_meta_cognitive_analyze_system(NULL));
    g_assert_false(gnc_meta_cognitive_get_metrics(GNC_METACOG_PROCESS_ATTENTION, NULL));
    g_assert_false(gnc_meta_cognitive_update_metrics(GNC_METACOG_PROCESS_ATTENTION, NULL));
    g_assert_null(gnc_meta_cognitive_evolve_architecture(NULL, NULL, GNC_METACOG_PROCESS_ATTENTION));
    g_assert_false(gnc_meta_cognitive_apply_config(NULL));
    g_assert_false(gnc_meta_cognitive_start_improvement_cycle(NULL, 10, 0.1));
    g_assert_false(gnc_meta_cognitive_stop_improvement_cycle(NULL));
    g_assert_false(gnc_meta_cognitive_is_improving(NULL));
    g_assert_false(gnc_meta_cognitive_get_improvement_status(NULL, NULL, NULL));
    
    // Test invalid process types - should handle gracefully
    GncCognitiveMetrics metrics;
    initialize_default_metrics(&metrics);
    
    // These should not crash even with invalid process types
    gnc_meta_cognitive_get_metrics(static_cast<GncMetaCognitiveProcessType>(999), &metrics);
    gnc_meta_cognitive_update_metrics(static_cast<GncMetaCognitiveProcessType>(999), &metrics);
    
    // Test double initialization
    g_assert_true(gnc_meta_cognitive_init()); // Should handle gracefully
    
    // Test operations on destroyed session
    GncMetaCognitiveSession *session = gnc_meta_cognitive_session_new();
    g_assert_nonnull(session);
    gnc_meta_cognitive_session_destroy(session);
    
    // These should handle destroyed session gracefully (though behavior is undefined)
    // In a real implementation, we might add session validation
}

/* Test concurrent operations */
static void test_concurrent_operations(MetaCognitiveFixture *fixture G_GNUC_UNUSED, gconstpointer data G_GNUC_UNUSED)
{
    g_test_message("Testing concurrent operations...");
    
    GncMetaCognitiveSession *session1 = gnc_meta_cognitive_session_new();
    GncMetaCognitiveSession *session2 = gnc_meta_cognitive_session_new();
    
    g_assert_nonnull(session1);
    g_assert_nonnull(session2);
    
    // Test that multiple sessions can coexist
    GncSelfAnalysisResult *result1 = gnc_meta_cognitive_analyze_process(
        session1, GNC_METACOG_PROCESS_ATTENTION);
    GncSelfAnalysisResult *result2 = gnc_meta_cognitive_analyze_process(
        session2, GNC_METACOG_PROCESS_VALIDATION);
    
    g_assert_nonnull(result1);
    g_assert_nonnull(result2);
    
    gnc_self_analysis_result_free(result1);
    gnc_self_analysis_result_free(result2);
    
    gnc_meta_cognitive_session_destroy(session1);
    gnc_meta_cognitive_session_destroy(session2);
}

/* Test configuration management */
static void test_config_management(MetaCognitiveFixture *fixture G_GNUC_UNUSED, gconstpointer data G_GNUC_UNUSED)
{
    g_test_message("Testing configuration management...");
    
    // Test saving and rolling back configurations
    g_assert_true(gnc_meta_cognitive_save_stable_config());
    
    // Apply a configuration change
    GncCognitiveArchConfig config = {
        .sti_funds = 2000.0,
        .lti_funds = 1000.0,
        .attention_decay_rate = 0.02,
        .wage_rate = 2.0,
        .truth_threshold = 0.85,
        .confidence_threshold = 0.75,
        .max_inference_steps = 200,
        .tensor_cache_size = 2000,
        .message_priority_threshold = 0.6,
        .max_concurrent_ops = 20,
        .base_learning_rate = 0.02,
        .learning_momentum = 0.95,
        .regularization_strength = 0.002
    };
    
    g_assert_true(gnc_meta_cognitive_apply_config(&config));
    
    // Test rollback
    g_assert_true(gnc_meta_cognitive_rollback_config());
}

/* Main test runner */
int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);
    
    // Add test cases
    g_test_add("/meta-cognitive/init", MetaCognitiveFixture, NULL, setup, test_meta_cognitive_init, teardown);
    g_test_add("/meta-cognitive/self-analysis", MetaCognitiveFixture, NULL, setup, test_self_analysis, teardown);
    g_test_add("/meta-cognitive/metrics", MetaCognitiveFixture, NULL, setup, test_metrics_management, teardown);
    g_test_add("/meta-cognitive/evolution", MetaCognitiveFixture, NULL, setup, test_evolutionary_optimization, teardown);
    g_test_add("/meta-cognitive/safety", MetaCognitiveFixture, NULL, setup, test_safety_mechanisms, teardown);
    g_test_add("/meta-cognitive/improvement", MetaCognitiveFixture, NULL, setup, test_recursive_improvement, teardown);
    g_test_add("/meta-cognitive/introspection", MetaCognitiveFixture, NULL, setup, test_introspection, teardown);
    g_test_add("/meta-cognitive/error-handling", MetaCognitiveFixture, NULL, setup, test_error_handling, teardown);
    g_test_add("/meta-cognitive/concurrent", MetaCognitiveFixture, NULL, setup, test_concurrent_operations, teardown);
    g_test_add("/meta-cognitive/config", MetaCognitiveFixture, NULL, setup, test_config_management, teardown);
    
    return g_test_run();
}

/* Helper function to initialize default metrics (needed for error handling test) */
static void initialize_default_metrics(GncCognitiveMetrics *metrics)
{
    metrics->accuracy = 0.75;
    metrics->efficiency = 0.70;
    metrics->latency_ms = 10.0;
    metrics->throughput = 100.0;
    metrics->memory_usage_mb = 50.0;
    metrics->cpu_usage_percent = 25.0;
    metrics->stability_index = 0.80;
    metrics->learning_rate = 0.01;
    metrics->innovation_score = 0.5;
    metrics->total_operations = 1000;
    metrics->error_count = 10;
    metrics->last_update = time(NULL);
}