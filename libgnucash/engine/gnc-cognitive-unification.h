/********************************************************************\
 * gnc-cognitive-unification.h -- Unified Cognitive Architecture    *
 * Copyright (C) 2024 GnuCash Cognitive Development Team           *
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
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
\********************************************************************/

/**
 * @file gnc-cognitive-unification.h
 * @brief Unified Cognitive Architecture Framework for GnuCashCog Phase 6
 * @author GnuCash Cognitive Development Team
 * 
 * Phase 6: Rigorous Testing, Documentation, and Cognitive Unification
 * 
 * This module provides comprehensive unification of all cognitive components
 * into a coherent, unified tensor field with holistic validation capabilities.
 * 
 * Key Features:
 * - Unified cognitive architecture synthesis
 * - Cross-phase integration validation
 * - Holistic system coherence verification
 * - Comprehensive test coverage analysis
 * - Emergent behavior pattern documentation
 * - Third-party development enablement
 * 
 * Deep Testing Protocols:
 * - Real implementation verification for every function
 * - Comprehensive integration testing across all cognitive modules
 * - Performance regression testing with measurable benchmarks
 * - Stress testing for cognitive system limits and edge cases
 * - Complete technical documentation with API coverage validation
 * 
 * Architecture Integration:
 * - Phase 1: Cognitive Primitives & Hypergraph Encoding
 * - Phase 2: ECAN Attention Allocation & Resource Kernel
 * - Phase 3: Neural-Symbolic Synthesis via ggml Kernels
 * - Phase 4: Distributed Cognitive Mesh API & Embodiment
 * - Phase 5: Recursive Meta-Cognition & Evolutionary Optimization
 * - Phase 6: Rigorous Testing, Documentation, and Unification
 */

#ifndef GNC_COGNITIVE_UNIFICATION_H
#define GNC_COGNITIVE_UNIFICATION_H

#include <glib.h>
#include "qof.h"
#include "Account.h"
#include "Transaction.h"
#include "gnc-cognitive-accounting.h"
#include "gnc-meta-cognitive.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup CognitiveUnification
 * @{
 */

/**
 * @name Unified Cognitive System State
 * Represents the complete state of all cognitive components
 */

/** Unified cognitive system handle */
typedef struct _GncUnifiedCognitiveSystem GncUnifiedCognitiveSystem;

/** Cognitive component types for unified validation */
typedef enum {
    GNC_COGNITIVE_COMPONENT_ATOMSPACE = 1,
    GNC_COGNITIVE_COMPONENT_PLN,
    GNC_COGNITIVE_COMPONENT_ECAN,
    GNC_COGNITIVE_COMPONENT_MOSES,
    GNC_COGNITIVE_COMPONENT_URE,
    GNC_COGNITIVE_COMPONENT_TENSOR_NETWORK,
    GNC_COGNITIVE_COMPONENT_META_COGNITIVE,
    GNC_COGNITIVE_COMPONENT_ALL = 0xFF
} GncCognitiveComponentType;

/** System coherence validation parameters */
typedef struct {
    gdouble coherence_threshold;        /** Minimum coherence score for validation */
    gdouble integration_threshold;      /** Cross-component integration requirement */
    gdouble performance_threshold;      /** Minimum performance requirement */
    gdouble emergent_complexity;       /** Complexity level for emergent behavior detection */
    gboolean validate_all_phases;      /** Whether to validate all phase interactions */
    gboolean include_stress_testing;   /** Include stress and edge case testing */
} GncCognitiveCoherenceParams;

/** Comprehensive system validation result */
typedef struct {
    gdouble overall_coherence_score;    /** Overall system coherence (0.0-1.0) */
    gdouble phase_integration_score;    /** Cross-phase integration quality */
    gdouble performance_efficiency;     /** System performance efficiency */
    gdouble test_coverage_percentage;   /** Test coverage across all components */
    gdouble documentation_completeness; /** Documentation coverage percentage */
    gint total_components_tested;       /** Number of components validated */
    gint emergent_patterns_detected;    /** Number of emergent behaviors found */
    GHashTable* component_scores;       /** Per-component validation scores */
    GHashTable* emergent_behaviors;     /** Detected emergent behavior patterns */
    gchar* validation_summary;          /** Human-readable validation summary */
    gchar* recommendations;             /** Improvement recommendations */
    gboolean system_unified;            /** TRUE if system achieves cognitive unity */
} GncUnifiedValidationResult;

/** Test coverage analysis result */
typedef struct {
    gdouble overall_coverage;           /** Overall test coverage percentage */
    GHashTable* per_component_coverage; /** Coverage by cognitive component */
    GHashTable* per_phase_coverage;     /** Coverage by development phase */
    gint total_functions;               /** Total number of functions */
    gint tested_functions;              /** Number of functions with tests */
    gint untested_functions;            /** Number of functions without tests */
    GList* untested_function_list;      /** List of untested function names */
    GList* critical_gaps;               /** Critical testing gaps */
    gchar* coverage_report;             /** Detailed coverage report */
} GncTestCoverageResult;

/** Documentation completeness analysis */
typedef struct {
    gdouble api_documentation_coverage;       /** API documentation percentage */
    gdouble architecture_documentation;       /** Architecture documentation quality */
    gdouble user_guide_completeness;         /** User guide completeness */
    gdouble developer_guide_completeness;    /** Developer guide completeness */
    gint total_public_functions;             /** Total public functions */
    gint documented_functions;               /** Documented public functions */
    GList* undocumented_functions;           /** List of undocumented functions */
    GHashTable* documentation_quality;       /** Per-section documentation quality */
    gchar* documentation_report;             /** Comprehensive documentation report */
    gboolean third_party_ready;              /** Ready for third-party development */
} GncDocumentationAnalysis;

/**
 * @name Unified Cognitive System Management
 * Functions for initializing, managing, and validating the unified cognitive system
 */

/**
 * Initialize the unified cognitive architecture framework
 * 
 * Sets up comprehensive validation infrastructure for all cognitive components.
 * Must be called after all individual cognitive systems are initialized.
 * 
 * @return TRUE if initialization successful, FALSE otherwise
 */
gboolean gnc_cognitive_unification_init(void);

/**
 * Shutdown the unified cognitive architecture framework
 * 
 * Cleans up all unified validation infrastructure and releases resources.
 */
void gnc_cognitive_unification_shutdown(void);

/**
 * Create a new unified cognitive system instance
 * 
 * @return New unified cognitive system handle, or NULL on failure
 */
GncUnifiedCognitiveSystem* gnc_unified_cognitive_system_new(void);

/**
 * Destroy a unified cognitive system instance
 * 
 * @param system The unified cognitive system to destroy
 */
void gnc_unified_cognitive_system_destroy(GncUnifiedCognitiveSystem* system);

/**
 * @name System Coherence Validation
 * Functions for validating overall system coherence and integration
 */

/**
 * Perform comprehensive system coherence validation
 * 
 * Validates integration, performance, and coherence across all cognitive components.
 * This is the primary function for Phase 6 holistic system validation.
 * 
 * @param system The unified cognitive system to validate
 * @param params Coherence validation parameters
 * @return Comprehensive validation result (must be freed with gnc_unified_validation_result_free)
 */
GncUnifiedValidationResult* gnc_validate_system_coherence(
    GncUnifiedCognitiveSystem* system,
    const GncCognitiveCoherenceParams* params);

/**
 * Validate cross-phase integration
 * 
 * Specifically tests integration between different development phases.
 * 
 * @param system The unified cognitive system
 * @param phase_mask Bitmask of phases to validate (use GNC_COGNITIVE_COMPONENT_ALL for all)
 * @return Integration validation score (0.0-1.0)
 */
gdouble gnc_validate_cross_phase_integration(
    GncUnifiedCognitiveSystem* system,
    guint phase_mask);

/**
 * Detect emergent behavioral patterns
 * 
 * Identifies complex behaviors that emerge from component interactions.
 * 
 * @param system The unified cognitive system
 * @param complexity_threshold Minimum complexity for pattern detection
 * @return Hash table of detected emergent patterns (pattern_name -> description)
 */
GHashTable* gnc_detect_emergent_patterns(
    GncUnifiedCognitiveSystem* system,
    gdouble complexity_threshold);

/**
 * @name Test Coverage Analysis
 * Functions for analyzing and reporting test coverage across all components
 */

/**
 * Perform comprehensive test coverage analysis
 * 
 * Analyzes test coverage across all cognitive components and phases.
 * 
 * @param system The unified cognitive system
 * @return Test coverage analysis result (must be freed with gnc_test_coverage_result_free)
 */
GncTestCoverageResult* gnc_analyze_test_coverage(GncUnifiedCognitiveSystem* system);

/**
 * Run all cognitive system tests
 * 
 * Executes comprehensive testing across all components with performance monitoring.
 * 
 * @param system The unified cognitive system
 * @param include_stress_tests Include stress and edge case testing
 * @param include_regression_tests Include performance regression testing
 * @return Overall test pass rate (0.0-1.0), or -1.0 on error
 */
gdouble gnc_run_comprehensive_tests(
    GncUnifiedCognitiveSystem* system,
    gboolean include_stress_tests,
    gboolean include_regression_tests);

/**
 * Generate test coverage report
 * 
 * @param coverage_result Test coverage analysis result
 * @return Formatted HTML test coverage report (must be freed with g_free)
 */
gchar* gnc_generate_test_coverage_report(const GncTestCoverageResult* coverage_result);

/**
 * @name Documentation Validation
 * Functions for validating documentation completeness and quality
 */

/**
 * Analyze documentation completeness
 * 
 * Comprehensively analyzes all system documentation for completeness and quality.
 * 
 * @param system The unified cognitive system
 * @return Documentation analysis result (must be freed with gnc_documentation_analysis_free)
 */
GncDocumentationAnalysis* gnc_analyze_documentation_completeness(
    GncUnifiedCognitiveSystem* system);

/**
 * Generate architectural documentation
 * 
 * Auto-generates comprehensive architectural documentation with diagrams.
 * 
 * @param system The unified cognitive system
 * @param output_format Output format ("html", "markdown", "pdf")
 * @return Path to generated documentation file (must be freed with g_free)
 */
gchar* gnc_generate_architectural_documentation(
    GncUnifiedCognitiveSystem* system,
    const gchar* output_format);

/**
 * Generate API reference documentation
 * 
 * Creates comprehensive API documentation with cognitive context.
 * 
 * @param system The unified cognitive system
 * @param include_examples Include usage examples in documentation
 * @return Path to generated API reference (must be freed with g_free)
 */
gchar* gnc_generate_api_documentation(
    GncUnifiedCognitiveSystem* system,
    gboolean include_examples);

/**
 * @name Performance Validation
 * Functions for performance monitoring and regression testing
 */

/**
 * Run performance benchmark suite
 * 
 * Executes comprehensive performance benchmarks across all cognitive components.
 * 
 * @param system The unified cognitive system
 * @param benchmark_iterations Number of benchmark iterations
 * @return Hash table of performance metrics (component_name -> benchmark_result)
 */
GHashTable* gnc_run_performance_benchmarks(
    GncUnifiedCognitiveSystem* system,
    gint benchmark_iterations);

/**
 * Validate performance regression
 * 
 * Compares current performance against baseline to detect regressions.
 * 
 * @param system The unified cognitive system
 * @param baseline_results Baseline performance results to compare against
 * @return Regression analysis result with performance differences
 */
GHashTable* gnc_validate_performance_regression(
    GncUnifiedCognitiveSystem* system,
    const GHashTable* baseline_results);

/**
 * @name System Integration Validation
 * Functions for validating system-wide integration and workflow
 */

/**
 * Validate end-to-end cognitive workflow
 * 
 * Tests complete cognitive accounting workflows from data input to insights.
 * 
 * @param system The unified cognitive system
 * @param test_account_hierarchy Test account structure to use
 * @param test_transactions Array of test transactions
 * @param n_transactions Number of test transactions
 * @return Workflow validation success rate (0.0-1.0)
 */
gdouble gnc_validate_end_to_end_workflow(
    GncUnifiedCognitiveSystem* system,
    Account* test_account_hierarchy,
    Transaction** test_transactions,
    gint n_transactions);

/**
 * Validate cognitive resource management
 * 
 * Tests attention allocation, memory usage, and resource optimization.
 * 
 * @param system The unified cognitive system
 * @return Resource management efficiency score (0.0-1.0)
 */
gdouble gnc_validate_resource_management(GncUnifiedCognitiveSystem* system);

/**
 * @name Utility Functions
 * Helper functions for result management and reporting
 */

/**
 * Free unified validation result
 * 
 * @param result Validation result to free
 */
void gnc_unified_validation_result_free(GncUnifiedValidationResult* result);

/**
 * Free test coverage result
 * 
 * @param result Test coverage result to free
 */
void gnc_test_coverage_result_free(GncTestCoverageResult* result);

/**
 * Free documentation analysis result
 * 
 * @param result Documentation analysis result to free
 */
void gnc_documentation_analysis_free(GncDocumentationAnalysis* result);

/**
 * Generate unified system report
 * 
 * Creates a comprehensive report covering all aspects of system validation.
 * 
 * @param system The unified cognitive system
 * @param validation_result System validation results
 * @param coverage_result Test coverage results
 * @param doc_analysis Documentation analysis results
 * @return Comprehensive system report in HTML format (must be freed with g_free)
 */
gchar* gnc_generate_unified_system_report(
    GncUnifiedCognitiveSystem* system,
    const GncUnifiedValidationResult* validation_result,
    const GncTestCoverageResult* coverage_result,
    const GncDocumentationAnalysis* doc_analysis);

/**
 * Check if system achieves cognitive unity
 * 
 * Determines if the system meets all Phase 6 success criteria for cognitive unification.
 * 
 * @param system The unified cognitive system
 * @return TRUE if system achieves complete cognitive unity, FALSE otherwise
 */
gboolean gnc_system_achieves_cognitive_unity(GncUnifiedCognitiveSystem* system);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* GNC_COGNITIVE_UNIFICATION_H */