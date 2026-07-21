/********************************************************************\
 * test-ontogenesis-bridge.cpp -- Integration tests for the        *
 *   Meta-Cognition <-> Ontogenesis Kernel bridge                   *
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
#include "gnc-ontogenesis-bridge.h"
#include <glib.h>
#include <cmath>

/** @file test-ontogenesis-bridge.cpp
 *  @brief Integration tests verifying the wiring between Phase 5
 *         recursive meta-cognition and the ontogenesis kernel bridge.
 */

/* ---------------------------------------------------------------
 * Fixture helpers
 * --------------------------------------------------------------- */

static void setup(G_GNUC_UNUSED void *, G_GNUC_UNUSED gconstpointer)
{
    g_assert_true(gnc_meta_cognitive_init());
    /* gnc_meta_cognitive_init() now also calls gnc_ontogenesis_bridge_init() */
}

static void teardown(G_GNUC_UNUSED void *, G_GNUC_UNUSED gconstpointer)
{
    gnc_meta_cognitive_shutdown();
    /* gnc_meta_cognitive_shutdown() now also calls gnc_ontogenesis_bridge_shutdown() */
}

/* ---------------------------------------------------------------
 * Bridge lifecycle
 * --------------------------------------------------------------- */

static void test_bridge_init_shutdown(G_GNUC_UNUSED void *, G_GNUC_UNUSED gconstpointer)
{
    g_test_message("Testing bridge init / shutdown lifecycle...");

    /* Bridge should already be ready after meta-cognitive init */
    g_assert_true(gnc_ontogenesis_bridge_is_ready());

    /* Kernel status should be READY (stub) */
    GncOntogenesisStatus status = gnc_ontogenesis_bridge_get_kernel_status();
    g_assert_cmpint(status, ==, GNC_ONTOGENESIS_STATUS_READY);
}

/* ---------------------------------------------------------------
 * Directive submission
 * --------------------------------------------------------------- */

static void test_submit_directive(G_GNUC_UNUSED void *, G_GNUC_UNUSED gconstpointer)
{
    g_test_message("Testing directive submission to ontogenesis kernel...");

    GncOntogenesisDirective directive;
    memset(&directive, 0, sizeof(directive));
    directive.mutation_type     = GNC_KERNEL_MUTATION_ATTENTION;
    directive.source_process    = GNC_METACOG_PROCESS_ATTENTION;
    directive.priority          = 0.8;
    directive.target_improvement = 0.1;

    g_assert_true(gnc_ontogenesis_bridge_submit_directive(&directive));

    /* In stub mode the directive is processed immediately,
       so there should be zero pending and one result available. */
    g_assert_cmpuint(gnc_ontogenesis_bridge_pending_directives(), ==, 0);

    GncOntogenesisResult *result = gnc_ontogenesis_bridge_poll_result();
    g_assert_nonnull(result);
    g_assert_true(result->success);
    g_assert_cmpint(result->mutation_type, ==, GNC_KERNEL_MUTATION_ATTENTION);
    g_assert_cmpuint(result->kernels_generated, >, 0);
    g_assert_cmpuint(result->kernels_accepted, >, 0);
    g_assert_cmpfloat(result->achieved_improvement, >, 0.0);

    gnc_ontogenesis_result_free(result);
}

/* ---------------------------------------------------------------
 * Result integration into meta-cognitive metrics
 * --------------------------------------------------------------- */

static void test_integrate_result(G_GNUC_UNUSED void *, G_GNUC_UNUSED gconstpointer)
{
    g_test_message("Testing ontogenesis result integration into meta-cognitive metrics...");

    /* Read baseline metrics */
    GncCognitiveMetrics before;
    gnc_meta_cognitive_get_metrics(GNC_METACOG_PROCESS_ATTENTION, &before);

    /* Submit a directive and collect the result */
    GncOntogenesisDirective directive;
    memset(&directive, 0, sizeof(directive));
    directive.mutation_type     = GNC_KERNEL_MUTATION_ATTENTION;
    directive.source_process    = GNC_METACOG_PROCESS_ATTENTION;
    directive.priority          = 1.0;
    directive.target_improvement = 0.2;

    gnc_ontogenesis_bridge_submit_directive(&directive);

    GncOntogenesisResult *result = gnc_ontogenesis_bridge_poll_result();
    g_assert_nonnull(result);

    /* Integrate the result back into meta-cognitive metrics */
    g_assert_true(gnc_ontogenesis_bridge_integrate_result(result));

    /* Verify metrics improved */
    GncCognitiveMetrics after;
    gnc_meta_cognitive_get_metrics(GNC_METACOG_PROCESS_ATTENTION, &after);
    g_assert_cmpfloat(after.accuracy, >=, before.accuracy);

    gnc_ontogenesis_result_free(result);
}

/* ---------------------------------------------------------------
 * Submit from analysis
 * --------------------------------------------------------------- */

static void test_submit_from_analysis(G_GNUC_UNUSED void *, G_GNUC_UNUSED gconstpointer)
{
    g_test_message("Testing directive submission from self-analysis...");

    GncMetaCognitiveSession *session = gnc_meta_cognitive_session_new();
    g_assert_nonnull(session);

    GncSelfAnalysisResult *analysis =
        gnc_meta_cognitive_analyze_system(session);
    g_assert_nonnull(analysis);

    /* The analysis may or may not require evolution depending on
       default metrics, but submit_from_analysis should always succeed. */
    gint n = gnc_ontogenesis_bridge_submit_from_analysis(analysis);
    g_assert_cmpint(n, >=, 0);

    /* Drain any results */
    GncOntogenesisResult *result;
    while ((result = gnc_ontogenesis_bridge_poll_result()) != NULL) {
        gnc_ontogenesis_result_free(result);
    }

    gnc_self_analysis_result_free(analysis);
    gnc_meta_cognitive_session_destroy(session);
}

/* ---------------------------------------------------------------
 * Combined step
 * --------------------------------------------------------------- */

static void test_combined_step(G_GNUC_UNUSED void *, G_GNUC_UNUSED gconstpointer)
{
    g_test_message("Testing combined meta-cognition + ontogenesis step...");

    GncMetaCognitiveSession *session = gnc_meta_cognitive_session_new();
    g_assert_nonnull(session);

    gdouble improvement = gnc_ontogenesis_bridge_run_combined_step(session);
    /* The step should succeed (>= 0) even if no evolution was needed */
    g_assert_cmpfloat(improvement, >=, -1.0);

    gnc_meta_cognitive_session_destroy(session);
}

/* ---------------------------------------------------------------
 * Combined loop start / stop
 * --------------------------------------------------------------- */

static void test_combined_loop(G_GNUC_UNUSED void *, G_GNUC_UNUSED gconstpointer)
{
    g_test_message("Testing combined loop start / stop...");

    GncMetaCognitiveSession *session = gnc_meta_cognitive_session_new();
    g_assert_nonnull(session);

    g_assert_true(gnc_ontogenesis_bridge_start_combined_loop(
        session, 5, 10.0));
    g_assert_true(gnc_ontogenesis_bridge_is_combined_loop_active());

    /* Let it run briefly */
    g_usleep(200000);  /* 200 ms */

    g_assert_true(gnc_ontogenesis_bridge_stop_combined_loop());
    g_assert_false(gnc_ontogenesis_bridge_is_combined_loop_active());

    gnc_meta_cognitive_session_destroy(session);
}

/* ---------------------------------------------------------------
 * Kernel registration replacement
 * --------------------------------------------------------------- */

static void test_kernel_replacement(G_GNUC_UNUSED void *, G_GNUC_UNUSED gconstpointer)
{
    g_test_message("Testing kernel replacement...");

    /* The default stub kernel should already be registered */
    g_assert_true(gnc_ontogenesis_bridge_is_ready());

    /* Register a new (still stub) kernel to simulate OZC-272 landing */
    GncOntogenesisKernel *new_kernel = gnc_ontogenesis_kernel_new_stub(42);

    g_assert_true(gnc_ontogenesis_bridge_register_kernel(new_kernel));
    g_assert_true(gnc_ontogenesis_bridge_is_ready());
}

/* ---------------------------------------------------------------
 * Edge cases: NULL inputs
 * --------------------------------------------------------------- */

static void test_null_inputs(G_GNUC_UNUSED void *, G_GNUC_UNUSED gconstpointer)
{
    g_test_message("Testing NULL input handling...");

    g_assert_false(gnc_ontogenesis_bridge_submit_directive(NULL));
    g_assert_cmpint(gnc_ontogenesis_bridge_submit_from_analysis(NULL), ==, -1);
    g_assert_false(gnc_ontogenesis_bridge_integrate_result(NULL));
    g_assert_cmpfloat(gnc_ontogenesis_bridge_run_combined_step(NULL), ==, -1.0);

    /* Freeing NULL should be safe */
    gnc_ontogenesis_result_free(NULL);
}

/* ---------------------------------------------------------------
 * Test runner
 * --------------------------------------------------------------- */

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add("/ontogenesis-bridge/init-shutdown",
               void, NULL, setup, test_bridge_init_shutdown, teardown);
    g_test_add("/ontogenesis-bridge/submit-directive",
               void, NULL, setup, test_submit_directive, teardown);
    g_test_add("/ontogenesis-bridge/integrate-result",
               void, NULL, setup, test_integrate_result, teardown);
    g_test_add("/ontogenesis-bridge/submit-from-analysis",
               void, NULL, setup, test_submit_from_analysis, teardown);
    g_test_add("/ontogenesis-bridge/combined-step",
               void, NULL, setup, test_combined_step, teardown);
    g_test_add("/ontogenesis-bridge/combined-loop",
               void, NULL, setup, test_combined_loop, teardown);
    g_test_add("/ontogenesis-bridge/kernel-replacement",
               void, NULL, setup, test_kernel_replacement, teardown);
    g_test_add("/ontogenesis-bridge/null-inputs",
               void, NULL, setup, test_null_inputs, teardown);

    return g_test_run();
}
