/********************************************************************\
 * test-neural-symbolic-kernels.cpp -- Test suite for Phase 3     *
 * neural-symbolic synthesis via custom ggml kernels               *
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

#include <glib.h>
#include <math.h>
#include <string.h>
#include "gnc-neural-symbolic-kernels.h"
#include "gnc-cognitive-accounting.h"

/** Test fixture for neural-symbolic kernels */
typedef struct {
    GncTensorData *test_tensor_a;
    GncTensorData *test_tensor_b;
    GncTensorData *output_tensor;
    GncNeuralSymbolicKernel *test_kernel;
    GncNeuralSymbolicInference *inference_engine;
} NeuralSymbolicFixture;

static void neural_symbolic_fixture_setup(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    // Initialize the kernel system
    g_assert_true(gnc_neural_symbolic_kernels_init());
    
    // Create test tensors
    gsize shape[] = {64}; // 64-dimensional tensors
    fixture->test_tensor_a = gnc_tensor_data_create("test_a", 1, shape);
    fixture->test_tensor_b = gnc_tensor_data_create("test_b", 1, shape);
    fixture->output_tensor = gnc_tensor_data_create("output", 1, shape);
    
    // Initialize test data with known values
    for (gsize i = 0; i < 64; i++) {
        fixture->test_tensor_a->data[i] = 0.5f + (gfloat)i / 128.0f; // [0.5, 1.0]
        fixture->test_tensor_b->data[i] = 0.3f + (gfloat)i / 256.0f; // [0.3, 0.55]
        fixture->output_tensor->data[i] = 0.0f;
    }
    
    // Create test kernel
    fixture->test_kernel = gnc_neural_symbolic_kernel_create(GNC_SYMBOLIC_OP_LOGICAL_AND);
    
    // Create inference engine
    fixture->inference_engine = gnc_neural_symbolic_inference_create();
}

static void neural_symbolic_fixture_teardown(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    gnc_tensor_data_destroy(fixture->test_tensor_a);
    gnc_tensor_data_destroy(fixture->test_tensor_b);
    gnc_tensor_data_destroy(fixture->output_tensor);
    gnc_neural_symbolic_kernel_destroy(fixture->test_kernel);
    gnc_neural_symbolic_inference_destroy(fixture->inference_engine);
    
    gnc_neural_symbolic_kernels_shutdown();
}

/** Test Phase 3 Requirement: Custom ggml kernels initialization */
static void test_neural_symbolic_kernels_init(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    g_test_message("Testing neural-symbolic kernels initialization");
    
    // Kernel system should be initialized from fixture setup
    // (re-initialization returns TRUE when already initialized)
    g_assert_true(gnc_neural_symbolic_kernels_init());
    
    // Test double initialization (should be safe)
    g_assert_true(gnc_neural_symbolic_kernels_init());
}

/** Test Phase 3 Requirement: Symbolic tensor operations */
static void test_symbolic_tensor_logical_operations(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    g_test_message("Testing symbolic tensor logical operations");
    
    // Test logical AND operation
    g_assert_true(gnc_symbolic_tensor_logical_and(fixture->test_tensor_a,
                                                fixture->test_tensor_b,
                                                fixture->output_tensor));
    
    // Verify logical AND results (should be minimum of inputs with confidence adjustment)
    for (gsize i = 0; i < 10; i++) { // Check first 10 elements
        gfloat expected_min = fminf(fixture->test_tensor_a->data[i], 
                                   fixture->test_tensor_b->data[i]);
        gfloat confidence_factor = (fixture->test_tensor_a->data[i] + 
                                   fixture->test_tensor_b->data[i]) / 2.0f;
        gfloat expected = expected_min * confidence_factor;
        
        g_assert_cmpfloat_with_epsilon(fixture->output_tensor->data[i], expected, 0.001f);
    }
    
    // Reset output tensor
    memset(fixture->output_tensor->data, 0, sizeof(gfloat) * 64);
    
    // Test logical OR operation
    g_assert_true(gnc_symbolic_tensor_logical_or(fixture->test_tensor_a,
                                               fixture->test_tensor_b,
                                               fixture->output_tensor));
    
    // Verify logical OR results (probabilistic combination)
    for (gsize i = 0; i < 10; i++) {
        gfloat a = fixture->test_tensor_a->data[i];
        gfloat b = fixture->test_tensor_b->data[i];
        gfloat expected = fminf(a + b - (a * b), 1.0f);
        
        g_assert_cmpfloat_with_epsilon(fixture->output_tensor->data[i], expected, 0.001f);
    }
}

/** Test Phase 3 Requirement: PLN truth value revision */
static void test_symbolic_tensor_truth_revision(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    g_test_message("Testing PLN truth value revision");
    
    // Create prior and evidence tensors
    gsize shape[] = {32};
    GncTensorData *prior_tensor = gnc_tensor_data_create("prior", 1, shape);
    GncTensorData *evidence_tensor = gnc_tensor_data_create("evidence", 1, shape);
    GncTensorData *revised_tensor = gnc_tensor_data_create("revised", 1, shape);
    
    // Initialize with test values
    for (gsize i = 0; i < 32; i++) {
        prior_tensor->data[i] = 0.6f; // Prior belief
        evidence_tensor->data[i] = 0.8f; // Strong evidence
    }
    
    // Apply truth revision
    g_assert_true(gnc_symbolic_tensor_truth_revision(prior_tensor, 
                                                   evidence_tensor,
                                                   revised_tensor));
    
    // Verify revision results
    for (gsize i = 0; i < 10; i++) {
        gfloat weight = evidence_tensor->data[i];
        gfloat expected = (1.0f - weight) * prior_tensor->data[i] + 
                         weight * evidence_tensor->data[i];
        
        g_assert_cmpfloat_with_epsilon(revised_tensor->data[i], expected, 0.001f);
    }
    
    gnc_tensor_data_destroy(prior_tensor);
    gnc_tensor_data_destroy(evidence_tensor);
    gnc_tensor_data_destroy(revised_tensor);
}

/** Test Phase 3 Requirement: ECAN attention flow dynamics */
static void test_symbolic_tensor_attention_flow(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    g_test_message("Testing ECAN attention flow dynamics");
    
    // Create attention state and flow matrix
    gsize attention_shape[] = {4}; // 4 nodes
    gsize flow_shape[] = {4, 4};   // 4x4 flow matrix
    
    GncTensorData *attention_state = gnc_tensor_data_create("attention", 1, attention_shape);
    GncTensorData *flow_matrix = gnc_tensor_data_create("flow", 2, flow_shape);
    GncTensorData *updated_attention = gnc_tensor_data_create("updated", 1, attention_shape);
    
    // Initialize attention state
    attention_state->data[0] = 10.0f; // High attention node
    attention_state->data[1] = 5.0f;
    attention_state->data[2] = 3.0f;
    attention_state->data[3] = 1.0f;  // Low attention node
    
    // Initialize flow matrix (simplified)
    memset(flow_matrix->data, 0, sizeof(gfloat) * 16);
    flow_matrix->data[0 * 4 + 1] = 0.1f; // Flow from node 0 to node 1
    flow_matrix->data[1 * 4 + 2] = 0.1f; // Flow from node 1 to node 2
    flow_matrix->data[2 * 4 + 3] = 0.1f; // Flow from node 2 to node 3
    
    // Apply attention flow
    g_assert_true(gnc_symbolic_tensor_attention_flow(attention_state,
                                                   flow_matrix,
                                                   updated_attention));
    
    // Verify attention conservation (no negative values)
    for (gsize i = 0; i < 4; i++) {
        g_assert_cmpfloat(updated_attention->data[i], >=, 0.01f);
    }
    
    // Verify that high attention nodes lose some attention
    g_assert_cmpfloat(updated_attention->data[0], <, attention_state->data[0]);
    
    gnc_tensor_data_destroy(attention_state);
    gnc_tensor_data_destroy(flow_matrix);
    gnc_tensor_data_destroy(updated_attention);
}

/** Test Phase 3 Requirement: Pattern matching for cognitive primitives */
static void test_symbolic_tensor_pattern_matching(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    g_test_message("Testing pattern matching for cognitive primitives");
    
    gsize shape[] = {128}; // 128-dimensional patterns
    GncTensorData *pattern_tensor = gnc_tensor_data_create("pattern", 1, shape);
    GncTensorData *data_tensor = gnc_tensor_data_create("data", 1, shape);
    gsize score_shape[] = {1};
    GncTensorData *match_scores = gnc_tensor_data_create("scores", 1, score_shape);
    
    // Create similar patterns
    for (gsize i = 0; i < 128; i++) {
        pattern_tensor->data[i] = sinf((gfloat)i / 20.0f);
        data_tensor->data[i] = sinf((gfloat)i / 20.0f) + 0.1f; // Slightly different
    }
    
    // Perform pattern matching
    g_assert_true(gnc_symbolic_tensor_pattern_match(pattern_tensor,
                                                  data_tensor,
                                                  match_scores));
    
    // Verify match score is reasonable (should be high similarity)
    g_assert_cmpfloat(match_scores->data[0], >, 0.8f);
    g_assert_cmpfloat(match_scores->data[0], <=, 1.0f);
    
    gnc_tensor_data_destroy(pattern_tensor);
    gnc_tensor_data_destroy(data_tensor);
    gnc_tensor_data_destroy(match_scores);
}

/** Test Phase 3 Requirement: Neural-symbolic inference engine */
static void test_neural_symbolic_inference_engine(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    g_test_message("Testing neural-symbolic inference engine");
    
    // Test inference step
    g_assert_true(gnc_neural_symbolic_inference_step(fixture->inference_engine,
                                                   fixture->test_tensor_a,
                                                   fixture->output_tensor));
    
    // Verify inference state was updated
    g_assert_cmpfloat(fixture->inference_engine->logical_consistency, >, 0.0f);
    g_assert_cmpint(fixture->inference_engine->inference_timestamp, >, 0);
    
    // Test mode switching
    g_assert_true(gnc_neural_symbolic_inference_switch_mode(fixture->inference_engine, FALSE));
    g_assert_false(fixture->inference_engine->neural_mode);
    g_assert_true(fixture->inference_engine->symbolic_mode);
    
    // Switch back
    g_assert_true(gnc_neural_symbolic_inference_switch_mode(fixture->inference_engine, TRUE));
    g_assert_true(fixture->inference_engine->neural_mode);
    g_assert_false(fixture->inference_engine->symbolic_mode);
    
    // Test inference chain
    GList *input_tensors = g_list_append(NULL, fixture->test_tensor_a);
    input_tensors = g_list_append(input_tensors, fixture->test_tensor_b);
    
    GList *output_tensors = NULL;
    g_assert_true(gnc_neural_symbolic_inference_chain(fixture->inference_engine,
                                                    input_tensors,
                                                    &output_tensors));
    
    g_assert_cmpint(g_list_length(output_tensors), ==, 2);
    
    // Cleanup
    g_list_free(input_tensors);
    g_list_free_full(output_tensors, (GDestroyNotify)gnc_tensor_data_destroy);
}

/** Test Phase 3 Requirement: AtomSpace integration hooks */
static void test_atomspace_neural_integration(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    g_test_message("Testing AtomSpace neural integration hooks");
    
    // Test AtomSpace to neural tensor conversion
    GncAtomHandle test_atom = 12345;
    gsize shape[] = {32};
    GncTensorData *neural_tensor = gnc_tensor_data_create("neural", 1, shape);
    
    g_assert_true(gnc_atomspace_to_neural_tensor(test_atom, neural_tensor));
    
    // Verify tensor was populated
    gboolean has_non_zero = FALSE;
    for (gsize i = 0; i < 32; i++) {
        if (neural_tensor->data[i] != 0.0f) {
            has_non_zero = TRUE;
            break;
        }
    }
    g_assert_true(has_non_zero);
    
    // Test neural tensor to AtomSpace conversion
    GncAtomHandle result_atom;
    g_assert_true(gnc_neural_tensor_to_atomspace(neural_tensor, &result_atom));
    g_assert_cmpint(result_atom, !=, 0);
    
    // Test consistency check
    GList *symbolic_atoms = g_list_append(NULL, GSIZE_TO_POINTER(test_atom));
    gdouble consistency_score;
    
    g_assert_true(gnc_neural_symbolic_consistency_check(neural_tensor,
                                                      symbolic_atoms,
                                                      &consistency_score));
    
    g_assert_cmpfloat(consistency_score, >=, 0.0f);
    g_assert_cmpfloat(consistency_score, <=, 1.0f);
    
    gnc_tensor_data_destroy(neural_tensor);
    g_list_free(symbolic_atoms);
}

/** Test Phase 3 Requirement: Cognitive primitive operations */
static void test_cognitive_primitive_operations(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    g_test_message("Testing cognitive primitive operations");
    
    // Test concept formation
    gsize feature_shape[] = {256};
    gsize concept_shape[] = {64};
    
    GncTensorData *feature_tensor = gnc_tensor_data_create("features", 1, feature_shape);
    GncTensorData *concept_tensor = gnc_tensor_data_create("concept", 1, concept_shape);
    
    // Initialize features
    for (gsize i = 0; i < 256; i++) {
        feature_tensor->data[i] = sinf((gfloat)i / 10.0f) + 1.0f;
    }
    
    g_assert_true(gnc_cognitive_primitive_concept_formation(feature_tensor, concept_tensor));
    
    // Verify concept was formed (should have non-zero values)
    gboolean concept_formed = FALSE;
    for (gsize i = 0; i < 64; i++) {
        if (concept_tensor->data[i] != 0.0f) {
            concept_formed = TRUE;
            break;
        }
    }
    g_assert_true(concept_formed);
    
    // Test analogical reasoning
    gsize analog_shape[] = {64};
    GncTensorData *source_tensor = gnc_tensor_data_create("source", 1, analog_shape);
    GncTensorData *target_tensor = gnc_tensor_data_create("target", 1, analog_shape);
    GncTensorData *analogy_tensor = gnc_tensor_data_create("analogy", 1, analog_shape);
    
    // Initialize analogy test data
    for (gsize i = 0; i < 64; i++) {
        source_tensor->data[i] = (gfloat)i / 64.0f;
        target_tensor->data[i] = (gfloat)i / 64.0f + 0.5f;
    }
    
    g_assert_true(gnc_cognitive_primitive_analogical_reasoning(source_tensor,
                                                             target_tensor,
                                                             analogy_tensor));
    
    // Verify analogical mapping was computed
    g_assert_cmpfloat(analogy_tensor->data[10], !=, 0.0f);
    
    gnc_tensor_data_destroy(feature_tensor);
    gnc_tensor_data_destroy(concept_tensor);
    gnc_tensor_data_destroy(source_tensor);
    gnc_tensor_data_destroy(target_tensor);
    gnc_tensor_data_destroy(analogy_tensor);
}

/** Test Phase 3 Requirement: Performance benchmarking */
static void test_kernel_performance_benchmarking(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    g_test_message("Testing kernel performance benchmarking");
    
    GncKernelPerformanceMetrics metrics;
    
    g_assert_true(gnc_neural_symbolic_kernel_benchmark(fixture->test_kernel,
                                                     fixture->test_tensor_a,
                                                     &metrics));
    
    // Verify benchmark metrics
    g_assert_cmpfloat(metrics.computation_time_ms, >, 0.0);
    g_assert_cmpint(metrics.operations_count, >, 0);
    g_assert_cmpfloat(metrics.throughput_ops_per_sec, >, 0.0);
    g_assert_cmpint(metrics.memory_usage_bytes, >, 0);
    g_assert_cmpfloat(metrics.logical_accuracy, >=, 0.0);
    g_assert_cmpfloat(metrics.neural_convergence, >=, 0.0);
    
    // Test memory profiling
    gsize peak_memory, steady_memory;
    g_assert_true(gnc_tensor_operation_memory_profile(GNC_SYMBOLIC_OP_LOGICAL_AND,
                                                    64 * sizeof(gfloat),
                                                    &peak_memory,
                                                    &steady_memory));
    
    g_assert_cmpint(peak_memory, >, steady_memory);
    g_assert_cmpint(steady_memory, >, 0);
}

/** Test Phase 3 Requirement: Tensor signature validation */
static void test_tensor_signature_validation(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    g_test_message("Testing tensor signature validation");
    
    const gchar *expected_signature = "dims:1,size:64,type:float32";
    gboolean signature_valid;
    
    g_assert_true(gnc_tensor_signature_validate(fixture->test_tensor_a,
                                              expected_signature,
                                              &signature_valid));
    
    g_assert_true(signature_valid);
    
    // Test with incorrect signature
    const gchar *wrong_signature = "dims:2,size:128,type:int32";
    g_assert_true(gnc_tensor_signature_validate(fixture->test_tensor_a,
                                              wrong_signature,
                                              &signature_valid));
    
    g_assert_false(signature_valid);
}

/** Test Phase 3 Requirement: Gradient-free symbolic reasoning */
static void test_gradient_free_symbolic_reasoning(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    g_test_message("Testing gradient-free symbolic reasoning");
    
    GncGradientFreeReasoning *reasoning = gnc_gradient_free_reasoning_create();
    g_assert_nonnull(reasoning);
    
    // Test rule application
    gsize rule_shape[] = {16};
    GncTensorData *premises = gnc_tensor_data_create("premises", 1, rule_shape);
    GncTensorData *conclusion = gnc_tensor_data_create("conclusion", 1, rule_shape);
    
    // Set high-confidence premises
    for (gsize i = 0; i < 16; i++) {
        premises->data[i] = 0.8f; // Above confidence threshold
    }
    
    g_assert_true(gnc_gradient_free_apply_rule(reasoning, "test_rule", 
                                             premises, conclusion));
    
    // Verify conclusion was derived
    g_assert_cmpfloat(conclusion->data[0], >, 0.0f);
    
    // Test forward chaining
    GncTensorData *facts = gnc_tensor_data_create("facts", 1, rule_shape);
    GncTensorData *derived_facts = gnc_tensor_data_create("derived", 1, rule_shape);
    
    for (gsize i = 0; i < 16; i++) {
        facts->data[i] = 0.7f; // Above threshold
    }
    
    g_assert_true(gnc_gradient_free_forward_chain(reasoning, facts, derived_facts));
    
    // Verify facts were derived
    g_assert_cmpfloat(derived_facts->data[0], >, 0.0f);
    g_assert_cmpfloat(derived_facts->data[0], <, facts->data[0]); // Reduced confidence
    
    gnc_tensor_data_destroy(premises);
    gnc_tensor_data_destroy(conclusion);
    gnc_tensor_data_destroy(facts);
    gnc_tensor_data_destroy(derived_facts);
    gnc_gradient_free_reasoning_destroy(reasoning);
}

/** Test Phase 3 Requirement: End-to-end neural-symbolic pipeline */
static void test_end_to_end_neural_symbolic_pipeline(NeuralSymbolicFixture *fixture, gconstpointer user_data)
{
    g_test_message("Testing end-to-end neural-symbolic inference pipeline");
    
    // Create a complete pipeline test
    GncNeuralSymbolicInference *pipeline = gnc_neural_symbolic_inference_create();
    
    // Phase 1: Neural processing
    g_assert_true(gnc_neural_symbolic_inference_switch_mode(pipeline, TRUE));
    
    gsize shape[] = {32};
    GncTensorData *neural_input = gnc_tensor_data_create("neural_input", 1, shape);
    GncTensorData *neural_output = gnc_tensor_data_create("neural_output", 1, shape);
    
    // Initialize neural input with financial data patterns
    for (gsize i = 0; i < 32; i++) {
        neural_input->data[i] = sinf((gfloat)i / 5.0f) * 0.5f + 0.5f; // [0, 1] range
    }
    
    g_assert_true(gnc_neural_symbolic_inference_step(pipeline, neural_input, neural_output));
    
    // Phase 2: Convert to symbolic representation
    GncAtomHandle symbolic_atom;
    g_assert_true(gnc_neural_tensor_to_atomspace(neural_output, &symbolic_atom));
    
    // Phase 3: Symbolic reasoning
    g_assert_true(gnc_neural_symbolic_inference_switch_mode(pipeline, FALSE));
    
    GncTensorData *symbolic_tensor = gnc_tensor_data_create("symbolic", 1, shape);
    g_assert_true(gnc_atomspace_to_neural_tensor(symbolic_atom, symbolic_tensor));
    
    GncTensorData *symbolic_output = gnc_tensor_data_create("symbolic_output", 1, shape);
    g_assert_true(gnc_neural_symbolic_inference_step(pipeline, symbolic_tensor, symbolic_output));
    
    // Phase 4: Consistency check
    GList *atoms = g_list_append(NULL, GSIZE_TO_POINTER(symbolic_atom));
    gdouble consistency;
    g_assert_true(gnc_neural_symbolic_consistency_check(symbolic_output, atoms, &consistency));
    
    // Verify pipeline maintained reasonable consistency
    g_assert_cmpfloat(consistency, >=, 0.0f);
    
    g_test_message("End-to-end pipeline consistency: %f", consistency);
    
    gnc_tensor_data_destroy(neural_input);
    gnc_tensor_data_destroy(neural_output);
    gnc_tensor_data_destroy(symbolic_tensor);
    gnc_tensor_data_destroy(symbolic_output);
    gnc_neural_symbolic_inference_destroy(pipeline);
    g_list_free(atoms);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    
    // Initialize cognitive accounting system
    g_assert_true(gnc_cognitive_accounting_init());
    
    g_test_message("=== Phase 3: Neural-Symbolic Synthesis via Custom ggml Kernels Test Suite ===");
    
    // Test kernel initialization and management
    g_test_add("/neural-symbolic/kernels/init", NeuralSymbolicFixture, NULL,
               neural_symbolic_fixture_setup, test_neural_symbolic_kernels_init,
               neural_symbolic_fixture_teardown);
    
    // Test symbolic tensor operations
    g_test_add("/neural-symbolic/tensors/logical-ops", NeuralSymbolicFixture, NULL,
               neural_symbolic_fixture_setup, test_symbolic_tensor_logical_operations,
               neural_symbolic_fixture_teardown);
    
    g_test_add("/neural-symbolic/tensors/truth-revision", NeuralSymbolicFixture, NULL,
               neural_symbolic_fixture_setup, test_symbolic_tensor_truth_revision,
               neural_symbolic_fixture_teardown);
    
    g_test_add("/neural-symbolic/tensors/attention-flow", NeuralSymbolicFixture, NULL,
               neural_symbolic_fixture_setup, test_symbolic_tensor_attention_flow,
               neural_symbolic_fixture_teardown);
    
    g_test_add("/neural-symbolic/tensors/pattern-matching", NeuralSymbolicFixture, NULL,
               neural_symbolic_fixture_setup, test_symbolic_tensor_pattern_matching,
               neural_symbolic_fixture_teardown);
    
    // Test neural-symbolic inference engine
    g_test_add("/neural-symbolic/inference/engine", NeuralSymbolicFixture, NULL,
               neural_symbolic_fixture_setup, test_neural_symbolic_inference_engine,
               neural_symbolic_fixture_teardown);
    
    // Test AtomSpace integration
    g_test_add("/neural-symbolic/atomspace/integration", NeuralSymbolicFixture, NULL,
               neural_symbolic_fixture_setup, test_atomspace_neural_integration,
               neural_symbolic_fixture_teardown);
    
    // Test cognitive primitives
    g_test_add("/neural-symbolic/cognitive/primitives", NeuralSymbolicFixture, NULL,
               neural_symbolic_fixture_setup, test_cognitive_primitive_operations,
               neural_symbolic_fixture_teardown);
    
    // Test performance and benchmarking
    g_test_add("/neural-symbolic/performance/benchmarking", NeuralSymbolicFixture, NULL,
               neural_symbolic_fixture_setup, test_kernel_performance_benchmarking,
               neural_symbolic_fixture_teardown);
    
    g_test_add("/neural-symbolic/validation/tensor-signature", NeuralSymbolicFixture, NULL,
               neural_symbolic_fixture_setup, test_tensor_signature_validation,
               neural_symbolic_fixture_teardown);
    
    // Test gradient-free reasoning
    g_test_add("/neural-symbolic/reasoning/gradient-free", NeuralSymbolicFixture, NULL,
               neural_symbolic_fixture_setup, test_gradient_free_symbolic_reasoning,
               neural_symbolic_fixture_teardown);
    
    // Test end-to-end pipeline
    g_test_add("/neural-symbolic/pipeline/end-to-end", NeuralSymbolicFixture, NULL,
               neural_symbolic_fixture_setup, test_end_to_end_neural_symbolic_pipeline,
               neural_symbolic_fixture_teardown);
    
    gint test_result = g_test_run();
    
    gnc_cognitive_accounting_shutdown();
    
    if (test_result == 0) {
        g_test_message("=== Phase 3: Neural-Symbolic Synthesis Test Suite PASSED ===");
        g_test_message("✓ Custom ggml kernels for symbolic tensor operations");
        g_test_message("✓ Neural inference hooks for AtomSpace integration");
        g_test_message("✓ Custom operations for cognitive primitives");
        g_test_message("✓ Gradient-free symbolic reasoning");
        g_test_message("✓ Performance benchmarking and validation");
        g_test_message("✓ End-to-end neural-symbolic inference pipeline");
    }
    
    return test_result;
}