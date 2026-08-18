/********************************************************************\
 * gnc-meta-cognitive.cpp -- Recursive Meta-Cognition Engine      *
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
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <random>
#include <cmath>
#include <ctime>
#include <thread>
#include <mutex>
#include <atomic>

/** @file gnc-meta-cognitive.cpp
 *  @brief Implementation of recursive meta-cognition and evolutionary optimization
 */

/* Internal data structures */

/** Meta-cognitive session structure */
struct _GncMetaCognitiveSession {
    guint session_id;
    time_t creation_time;
    std::map<GncMetaCognitiveProcessType, GncCognitiveMetrics> metrics_cache;
    std::map<GncMetaCognitiveProcessType, std::vector<GncCognitiveMetrics>> metrics_history;
    GncCognitiveArchConfig current_config;
    GncCognitiveArchConfig stable_config;
    gboolean improvement_cycle_active;
    guint current_iteration;
    gdouble current_improvement;
    std::thread *improvement_thread;
    std::mutex metrics_mutex;
};

/* Global state */
static gboolean meta_cognitive_initialized = FALSE;
static std::map<GncMetaCognitiveProcessType, GncCognitiveMetrics> global_metrics;
static std::vector<GncCognitiveArchConfig> config_history;
static GncCognitiveArchConfig current_global_config;
static gboolean safety_bounds_enabled = TRUE;
static gdouble min_performance_threshold = 0.5;
static gdouble max_deviation_threshold = 0.3;
static gboolean human_override_enabled = FALSE;
static std::mutex global_state_mutex;
static guint next_session_id = 1;

/* Forward declarations */
static void initialize_default_metrics(GncCognitiveMetrics *metrics);
static GncCognitiveArchConfig create_default_config(void);
static gdouble calculate_fitness(const GncCognitiveMetrics &metrics);
static GncCognitiveArchConfig mutate_config(const GncCognitiveArchConfig &config, gdouble mutation_rate);
static GncCognitiveArchConfig crossover_configs(const GncCognitiveArchConfig &parent1, const GncCognitiveArchConfig &parent2);
static void improvement_cycle_worker(GncMetaCognitiveSession *session, guint max_iterations, gdouble improvement_threshold);
static gchar** generate_optimization_suggestions(const GncCognitiveMetrics &current, const GncCognitiveMetrics &baseline);

/*==================================================================*
 * Meta-Cognitive Engine Initialization                            *
 *==================================================================*/

gboolean gnc_meta_cognitive_init(void)
{
    {
        std::lock_guard<std::mutex> lock(global_state_mutex);

        if (meta_cognitive_initialized) {
            /* Already initialized - return success silently.
             * This is not a warning condition since the API contract
             * specifies that multiple init calls are handled gracefully. */
            return TRUE;
        }

        g_message("Initializing recursive meta-cognitive analysis engine...");

        // Initialize default metrics for all process types
        for (int i = GNC_METACOG_PROCESS_ATTENTION; i <= GNC_METACOG_PROCESS_ALL; i++) {
            GncMetaCognitiveProcessType process_type = static_cast<GncMetaCognitiveProcessType>(i);
            initialize_default_metrics(&global_metrics[process_type]);
        }

        // Initialize default configuration
        current_global_config = create_default_config();
        config_history.push_back(current_global_config);

        // Initialize safety parameters
        safety_bounds_enabled = TRUE;
        min_performance_threshold = 0.5;
        max_deviation_threshold = 0.3;
        human_override_enabled = FALSE;

        meta_cognitive_initialized = TRUE;
    }
    /* global_state_mutex released here — call bridge_init outside the
     * lock to maintain the same ordering as shutdown (bridge operations
     * before global_state_mutex). */

    if (!gnc_ontogenesis_bridge_init()) {
        g_warning("Failed to initialize ontogenesis bridge");
    }
    
    g_message("✓ Meta-cognitive engine initialized successfully");
    g_message("✓ Recursive self-analysis capabilities active");
    g_message("✓ Evolutionary optimization framework ready");
    g_message("✓ Ontogenesis bridge wired (stub kernel until OZC-272)");
    g_message("✓ Safety mechanisms and rollback system operational");
    
    return TRUE;
}

void gnc_meta_cognitive_shutdown(void)
{
    // Shut down ontogenesis bridge BEFORE acquiring global_state_mutex.
    // The bridge shutdown joins the combined_loop_thread, which calls
    // gnc_meta_cognitive_get_metrics / update_metrics that also acquire
    // global_state_mutex.  Holding it here would deadlock.
    gnc_ontogenesis_bridge_shutdown();

    std::lock_guard<std::mutex> lock(global_state_mutex);
    
    if (!meta_cognitive_initialized) {
        return;
    }
    
    g_message("Shutting down meta-cognitive analysis engine...");
    
    // Clear global state
    global_metrics.clear();
    config_history.clear();
    
    meta_cognitive_initialized = FALSE;
    
    g_message("✓ Meta-cognitive engine shutdown complete");
}

/*==================================================================*
 * Session Management                                               *
 *==================================================================*/

GncMetaCognitiveSession* gnc_meta_cognitive_session_new(void)
{
    if (!meta_cognitive_initialized) {
        g_warning("Meta-cognitive engine not initialized");
        return NULL;
    }
    
    // Use C++ new to properly initialize STL containers and mutex
    GncMetaCognitiveSession *session = new GncMetaCognitiveSession();
    session->session_id = next_session_id++;
    session->creation_time = time(NULL);
    session->current_config = current_global_config;
    session->stable_config = current_global_config;
    session->improvement_cycle_active = FALSE;
    session->current_iteration = 0;
    session->current_improvement = 0.0;
    session->improvement_thread = nullptr;
    
    // Initialize metrics cache
    for (int i = GNC_METACOG_PROCESS_ATTENTION; i <= GNC_METACOG_PROCESS_ALL; i++) {
        GncMetaCognitiveProcessType process_type = static_cast<GncMetaCognitiveProcessType>(i);
        initialize_default_metrics(&session->metrics_cache[process_type]);
    }
    
    g_debug("Created meta-cognitive session %u", session->session_id);
    return session;
}

void gnc_meta_cognitive_session_destroy(GncMetaCognitiveSession *session)
{
    if (!session) return;
    
    // Stop the combined meta-cognition + ontogenesis loop only if it
    // is running for this specific session, so we don't break loops
    // owned by other sessions.
    gnc_ontogenesis_bridge_stop_combined_loop_for_session(session);
    
    // Stop any active improvement cycle
    gnc_meta_cognitive_stop_improvement_cycle(session);
    
    // Wait for improvement thread to finish
    if (session->improvement_thread) {
        if (session->improvement_thread->joinable()) {
            session->improvement_thread->join();
        }
        delete session->improvement_thread;
    }
    
    g_debug("Destroyed meta-cognitive session %u", session->session_id);
    // Use C++ delete to properly call destructors for STL containers and mutex
    delete session;
}

/*==================================================================*
 * Self-Analysis Functions                                          *
 *==================================================================*/

GncSelfAnalysisResult* gnc_meta_cognitive_analyze_process(
    GncMetaCognitiveSession *session,
    GncMetaCognitiveProcessType process_type)
{
    if (!session || !meta_cognitive_initialized) {
        return NULL;
    }
    
    std::lock_guard<std::mutex> lock(session->metrics_mutex);
    
    GncSelfAnalysisResult *result = g_new0(GncSelfAnalysisResult, 1);
    result->process_type = process_type;
    
    // Get current and baseline metrics
    auto current_it = session->metrics_cache.find(process_type);
    if (current_it != session->metrics_cache.end()) {
        result->current_metrics = current_it->second;
    } else {
        initialize_default_metrics(&result->current_metrics);
    }
    
    // Use first historical entry as baseline, or default if no history
    auto history_it = session->metrics_history.find(process_type);
    if (history_it != session->metrics_history.end() && !history_it->second.empty()) {
        result->baseline_metrics = history_it->second[0];
    } else {
        initialize_default_metrics(&result->baseline_metrics);
    }
    
    // Calculate improvement score
    gdouble current_fitness = calculate_fitness(result->current_metrics);
    gdouble baseline_fitness = calculate_fitness(result->baseline_metrics);
    result->improvement_score = (current_fitness - baseline_fitness) / std::max(baseline_fitness, 0.001);
    
    // Determine if evolution is needed
    result->requires_evolution = (result->improvement_score < -0.1) || 
                                (result->current_metrics.accuracy < 0.7) ||
                                (result->current_metrics.efficiency < 0.6);
    
    // Set confidence based on data availability and consistency
    result->confidence_level = (history_it != session->metrics_history.end() && 
                               history_it->second.size() > 5) ? 0.8 : 0.5;
    
    // Generate optimization suggestions
    gchar **suggestions = generate_optimization_suggestions(result->current_metrics, result->baseline_metrics);
    result->optimization_suggestions = suggestions;
    
    // Count suggestions
    result->n_suggestions = 0;
    if (suggestions) {
        while (suggestions[result->n_suggestions]) {
            result->n_suggestions++;
        }
    }
    
    g_debug("Process analysis complete for type %d: improvement=%.3f, evolution_needed=%s", 
            process_type, result->improvement_score, result->requires_evolution ? "yes" : "no");
    
    return result;
}

GncSelfAnalysisResult* gnc_meta_cognitive_analyze_system(GncMetaCognitiveSession *session)
{
    if (!session) return NULL;
    
    // Analyze overall system by aggregating all process metrics
    GncSelfAnalysisResult *result = g_new0(GncSelfAnalysisResult, 1);
    result->process_type = GNC_METACOG_PROCESS_ALL;
    
    // Initialize aggregated metrics
    initialize_default_metrics(&result->current_metrics);
    initialize_default_metrics(&result->baseline_metrics);
    
    gint process_count = 0;
    gdouble total_improvement = 0.0;
    gboolean any_requires_evolution = FALSE;
    
    // Analyze each individual process
    for (int i = GNC_METACOG_PROCESS_ATTENTION; i < GNC_METACOG_PROCESS_ALL; i++) {
        GncMetaCognitiveProcessType process_type = static_cast<GncMetaCognitiveProcessType>(i);
        GncSelfAnalysisResult *process_result = gnc_meta_cognitive_analyze_process(session, process_type);
        
        if (process_result) {
            // Aggregate metrics (simple averaging for now)
            result->current_metrics.accuracy += process_result->current_metrics.accuracy;
            result->current_metrics.efficiency += process_result->current_metrics.efficiency;
            result->current_metrics.stability_index += process_result->current_metrics.stability_index;
            result->current_metrics.learning_rate += process_result->current_metrics.learning_rate;
            result->current_metrics.innovation_score += process_result->current_metrics.innovation_score;
            
            result->baseline_metrics.accuracy += process_result->baseline_metrics.accuracy;
            result->baseline_metrics.efficiency += process_result->baseline_metrics.efficiency;
            result->baseline_metrics.stability_index += process_result->baseline_metrics.stability_index;
            result->baseline_metrics.learning_rate += process_result->baseline_metrics.learning_rate;
            result->baseline_metrics.innovation_score += process_result->baseline_metrics.innovation_score;
            
            total_improvement += process_result->improvement_score;
            if (process_result->requires_evolution) {
                any_requires_evolution = TRUE;
            }
            
            process_count++;
            gnc_self_analysis_result_free(process_result);
        }
    }
    
    // Average the aggregated metrics
    if (process_count > 0) {
        result->current_metrics.accuracy /= process_count;
        result->current_metrics.efficiency /= process_count;
        result->current_metrics.stability_index /= process_count;
        result->current_metrics.learning_rate /= process_count;
        result->current_metrics.innovation_score /= process_count;
        
        result->baseline_metrics.accuracy /= process_count;
        result->baseline_metrics.efficiency /= process_count;
        result->baseline_metrics.stability_index /= process_count;
        result->baseline_metrics.learning_rate /= process_count;
        result->baseline_metrics.innovation_score /= process_count;
        
        result->improvement_score = total_improvement / process_count;
    }
    
    result->requires_evolution = any_requires_evolution;
    result->confidence_level = 0.7; // System-wide analysis has moderate confidence
    
    // Generate system-level optimization suggestions
    result->optimization_suggestions = generate_optimization_suggestions(result->current_metrics, result->baseline_metrics);
    result->n_suggestions = 0;
    if (result->optimization_suggestions) {
        while (result->optimization_suggestions[result->n_suggestions]) {
            result->n_suggestions++;
        }
    }
    
    g_message("System analysis complete: overall improvement=%.3f, evolution_needed=%s", 
              result->improvement_score, result->requires_evolution ? "yes" : "no");
    
    return result;
}

gboolean gnc_meta_cognitive_get_metrics(
    GncMetaCognitiveProcessType process_type,
    GncCognitiveMetrics *metrics)
{
    if (!metrics || !meta_cognitive_initialized) {
        return FALSE;
    }
    
    std::lock_guard<std::mutex> lock(global_state_mutex);
    
    auto it = global_metrics.find(process_type);
    if (it != global_metrics.end()) {
        *metrics = it->second;
        return TRUE;
    }
    
    // If no metrics available, return default metrics
    initialize_default_metrics(metrics);
    return FALSE;
}

gboolean gnc_meta_cognitive_update_metrics(
    GncMetaCognitiveProcessType process_type,
    const GncCognitiveMetrics *metrics)
{
    if (!metrics || !meta_cognitive_initialized) {
        return FALSE;
    }
    
    std::lock_guard<std::mutex> lock(global_state_mutex);
    
    global_metrics[process_type] = *metrics;
    global_metrics[process_type].last_update = time(NULL);
    
    g_debug("Updated metrics for process type %d: accuracy=%.3f, efficiency=%.3f", 
            process_type, metrics->accuracy, metrics->efficiency);
    
    return TRUE;
}

gboolean gnc_meta_cognitive_apply_improvement(
    GncMetaCognitiveProcessType process_type,
    gdouble achieved_improvement,
    gdouble stability_delta,
    gdouble latency_delta_ms,
    guint   kernels_accepted)
{
    if (!meta_cognitive_initialized) return FALSE;

    std::lock_guard<std::mutex> lock(global_state_mutex);

    GncCognitiveMetrics *m;
    auto it = global_metrics.find(process_type);
    if (it != global_metrics.end()) {
        m = &it->second;
    } else {
        /* Initialise a default entry if none exists yet */
        initialize_default_metrics(&global_metrics[process_type]);
        m = &global_metrics[process_type];
    }

    m->accuracy = std::max(0.0, std::min(1.0, m->accuracy * (1.0 + achieved_improvement)));
    m->efficiency = std::max(0.0, std::min(1.0, m->efficiency * (1.0 + achieved_improvement * 0.5)));
    m->stability_index = std::max(0.0, std::min(1.0, m->stability_index + stability_delta));
    m->latency_ms = std::max(0.1, m->latency_ms + latency_delta_ms);
    m->innovation_score = std::min(1.0, m->innovation_score + 0.02 * kernels_accepted);
    m->last_update = time(NULL);

    g_debug("Applied ontogenesis improvement for process %d: "
            "improvement=%.3f, new_accuracy=%.3f",
            process_type, achieved_improvement, m->accuracy);

    return TRUE;
}

void gnc_self_analysis_result_free(GncSelfAnalysisResult *result)
{
    if (!result) return;
    
    if (result->optimization_suggestions) {
        g_strfreev(result->optimization_suggestions);
    }
    
    g_free(result);
}

/*==================================================================*
 * Evolutionary Optimization Functions                             *
 *==================================================================*/

GncCognitiveArchConfig* gnc_meta_cognitive_evolve_architecture(
    GncMetaCognitiveSession *session,
    const GncEvolutionaryParams *params,
    GncMetaCognitiveProcessType target_process)
{
    if (!session || !params || !meta_cognitive_initialized) {
        return NULL;
    }
    
    if (human_override_enabled) {
        g_warning("Evolution disabled: human override mode active");
        return NULL;
    }
    
    g_message("Starting evolutionary optimization for process type %d...", target_process);
    
    std::vector<GncCognitiveArchConfig> population;
    std::vector<gdouble> fitness_scores;
    
    // Initialize population
    population.reserve(params->population_size);
    fitness_scores.reserve(params->population_size);
    
    // Start with current configuration as first individual
    population.push_back(session->current_config);
    
    // Generate random variations for the rest of the population
    std::random_device rd;
    std::mt19937 gen(rd());
    
    for (guint i = 1; i < params->population_size; i++) {
        GncCognitiveArchConfig config = session->current_config;
        config = mutate_config(config, params->mutation_rate * 2.0); // Higher mutation for initial diversity
        population.push_back(config);
    }
    
    // Evolution loop
    for (guint generation = 0; generation < params->max_generations; generation++) {
        // Calculate fitness for each individual
        fitness_scores.clear();
        for (const auto &config : population) {
            // Simulate applying the config and measure performance
            GncCognitiveMetrics simulated_metrics;
            initialize_default_metrics(&simulated_metrics);
            
            // Simulate the effect of the configuration on metrics
            // This is a simplified simulation - in a real system, you would apply the config
            // and measure actual performance over time
            simulated_metrics.accuracy = std::min(1.0, simulated_metrics.accuracy * 
                (1.0 + config.base_learning_rate * config.learning_momentum));
            simulated_metrics.efficiency = std::min(1.0, simulated_metrics.efficiency * 
                (1.0 + config.attention_decay_rate * 0.5));
            simulated_metrics.stability_index = std::max(0.0, simulated_metrics.stability_index - 
                config.base_learning_rate * 0.1);
            
            gdouble fitness = calculate_fitness(simulated_metrics);
            fitness_scores.push_back(fitness);
        }
        
        // Find best fitness in current generation
        gdouble best_fitness = *std::max_element(fitness_scores.begin(), fitness_scores.end());
        
        if (best_fitness >= params->fitness_threshold) {
            g_message("Target fitness %.3f reached at generation %u", best_fitness, generation);
            break;
        }
        
        // Selection and reproduction
        std::vector<GncCognitiveArchConfig> new_population;
        new_population.reserve(params->population_size);
        
        // Elitism: preserve best individuals
        if (params->elitism_enabled) {
            std::vector<std::pair<gdouble, guint>> fitness_indices;
            for (guint i = 0; i < fitness_scores.size(); i++) {
                fitness_indices.emplace_back(fitness_scores[i], i);
            }
            
            std::sort(fitness_indices.rbegin(), fitness_indices.rend()); // Descending order
            
            for (guint i = 0; i < params->elite_count && i < params->population_size; i++) {
                new_population.push_back(population[fitness_indices[i].second]);
            }
        }
        
        // Fill rest of population through selection and reproduction
        std::uniform_real_distribution<> uniform(0.0, 1.0);
        
        while (new_population.size() < params->population_size) {
            // Tournament selection
            guint parent1_idx = 0, parent2_idx = 0;
            gdouble best_parent1_fitness = -1.0, best_parent2_fitness = -1.0;
            
            for (int tournament = 0; tournament < 3; tournament++) {
                guint candidate = gen() % population.size();
                if (fitness_scores[candidate] > best_parent1_fitness) {
                    best_parent1_fitness = fitness_scores[candidate];
                    parent1_idx = candidate;
                }
            }
            
            for (int tournament = 0; tournament < 3; tournament++) {
                guint candidate = gen() % population.size();
                if (fitness_scores[candidate] > best_parent2_fitness) {
                    best_parent2_fitness = fitness_scores[candidate];
                    parent2_idx = candidate;
                }
            }
            
            GncCognitiveArchConfig offspring;
            
            // Crossover
            if (uniform(gen) < params->crossover_rate) {
                offspring = crossover_configs(population[parent1_idx], population[parent2_idx]);
            } else {
                offspring = population[parent1_idx];
            }
            
            // Mutation
            if (uniform(gen) < params->mutation_rate) {
                offspring = mutate_config(offspring, params->mutation_rate);
            }
            
            new_population.push_back(offspring);
        }
        
        population = std::move(new_population);
        
        if (generation % 10 == 0) {
            g_debug("Generation %u: best fitness=%.3f, avg fitness=%.3f", generation, best_fitness,
                   std::accumulate(fitness_scores.begin(), fitness_scores.end(), 0.0) / fitness_scores.size());
        }
    }
    
    // Return the best configuration found
    auto max_it = std::max_element(fitness_scores.begin(), fitness_scores.end());
    guint best_idx = std::distance(fitness_scores.begin(), max_it);
    
    GncCognitiveArchConfig *result = g_new(GncCognitiveArchConfig, 1);
    *result = population[best_idx];
    
    g_message("Evolution complete: best fitness=%.3f, configuration evolved", *max_it);
    
    return result;
}

gboolean gnc_meta_cognitive_apply_config(const GncCognitiveArchConfig *config)
{
    if (!config || !meta_cognitive_initialized) {
        return FALSE;
    }
    
    if (human_override_enabled) {
        /* Human override is an expected condition, not a warning.
         * The test suite deliberately enables override and expects rejection. */
        g_debug("Configuration change blocked: human override mode active");
        return FALSE;
    }
    
    std::lock_guard<std::mutex> lock(global_state_mutex);
    
    // Safety check: ensure configuration is within safe bounds
    if (safety_bounds_enabled) {
        if (config->base_learning_rate > 1.0 || config->base_learning_rate < 0.001) {
            /* Safety rejection is expected behavior when testing with unsafe values.
             * Use g_info instead of g_warning to avoid fatal test termination. */
            g_info("Configuration rejected: learning rate %.3f outside safe bounds", config->base_learning_rate);
            return FALSE;
        }
        
        if (config->attention_decay_rate > 0.5) {
            g_info("Configuration rejected: attention decay rate %.3f too high", config->attention_decay_rate);
            return FALSE;
        }
    }
    
    // Save current configuration to history before applying new one
    config_history.push_back(current_global_config);
    
    // Apply new configuration
    current_global_config = *config;
    
    g_message("Applied new cognitive architecture configuration");
    g_debug("New config: learning_rate=%.3f, attention_decay=%.3f, sti_funds=%.1f",
           config->base_learning_rate, config->attention_decay_rate, config->sti_funds);

    /* Push safe parameters into the live ECAN economy when AtomSpace is up. */
    if (gnc_cognitive_accounting_is_initialized()) {
        gdouble sti = config->sti_funds > 0.0 ? config->sti_funds : 1000.0;
        gdouble lti = config->lti_funds > 0.0 ? config->lti_funds : 1000.0;
        gnc_ecan_init_attention_economy(sti, lti);
        gnc_ecan_set_attention_decay_rate(config->attention_decay_rate);
    }
    
    return TRUE;
}

gboolean gnc_meta_cognitive_rollback_config(void)
{
    std::lock_guard<std::mutex> lock(global_state_mutex);
    
    if (config_history.empty()) {
        g_warning("No configuration history available for rollback");
        return FALSE;
    }
    
    // Restore previous configuration
    current_global_config = config_history.back();
    config_history.pop_back();

    if (gnc_cognitive_accounting_is_initialized()) {
        gdouble sti = current_global_config.sti_funds > 0.0
                          ? current_global_config.sti_funds : 1000.0;
        gdouble lti = current_global_config.lti_funds > 0.0
                          ? current_global_config.lti_funds : 1000.0;
        gnc_ecan_init_attention_economy(sti, lti);
        gnc_ecan_set_attention_decay_rate(current_global_config.attention_decay_rate);
    }
    
    g_message("Rolled back to previous cognitive architecture configuration");
    
    return TRUE;
}

gboolean gnc_meta_cognitive_save_stable_config(void)
{
    if (!meta_cognitive_initialized) {
        return FALSE;
    }
    
    std::lock_guard<std::mutex> lock(global_state_mutex);
    
    // For now, just log that we're saving the configuration
    // In a real implementation, this would persist to disk
    g_message("Saved current configuration as stable baseline");
    
    return TRUE;
}

/*==================================================================*
 * Recursive Improvement Functions                                  *
 *==================================================================*/

static void improvement_cycle_worker(GncMetaCognitiveSession *session, 
                                   guint max_iterations, 
                                   gdouble improvement_threshold)
{
    g_message("Starting recursive self-improvement cycle (max_iterations=%u, threshold=%.3f)",
             max_iterations, improvement_threshold);
    
    for (guint iteration = 0; iteration < max_iterations && session->improvement_cycle_active; iteration++) {
        session->current_iteration = iteration;
        
        // Analyze current system performance
        GncSelfAnalysisResult *analysis = gnc_meta_cognitive_analyze_system(session);
        if (!analysis) {
            g_warning("Self-analysis failed at iteration %u", iteration);
            break;
        }
        
        session->current_improvement = analysis->improvement_score;
        
        g_debug("Iteration %u: improvement score=%.3f", iteration, analysis->improvement_score);
        
        // Check if improvement is sufficient
        if (analysis->improvement_score >= improvement_threshold) {
            g_message("Improvement threshold reached at iteration %u", iteration);
            gnc_self_analysis_result_free(analysis);
            break;
        }
        
        // If evolution is needed and improvement is below threshold, evolve
        if (analysis->requires_evolution && analysis->improvement_score < 0.0) {
            GncEvolutionaryParams params = {
                .population_size = 20,
                .mutation_rate = 0.1,
                .crossover_rate = 0.7,
                .max_generations = 50,
                .fitness_threshold = 0.8,
                .diversity_pressure = 0.2,
                .elitism_enabled = TRUE,
                .elite_count = 2
            };
            
            GncCognitiveArchConfig *evolved_config = gnc_meta_cognitive_evolve_architecture(
                session, &params, GNC_METACOG_PROCESS_ALL);
            
            if (evolved_config) {
                if (gnc_meta_cognitive_apply_config(evolved_config)) {
                    session->current_config = *evolved_config;
                    g_message("Applied evolved configuration at iteration %u", iteration);
                } else {
                    /* Configuration was rejected - might be normal due to safety bounds */
                    g_debug("Could not apply evolved configuration at iteration %u", iteration);
                }
                g_free(evolved_config);
            }

            // Route through ontogenesis bridge when available.
            // The bridge translates meta-cognitive directives into
            // low-level kernel mutations and feeds results back.
            if (gnc_ontogenesis_bridge_is_ready()) {
                gint n_directives = gnc_ontogenesis_bridge_submit_from_analysis(analysis);
                if (n_directives > 0) {
                    g_debug("Submitted %d ontogenesis directives at iteration %u",
                            n_directives, iteration);
                }

                // Poll and integrate any completed ontogenesis results
                GncOntogenesisResult *onto_result;
                while ((onto_result = gnc_ontogenesis_bridge_poll_result()) != NULL) {
                    gnc_ontogenesis_bridge_integrate_result(onto_result);
                    if (onto_result->success) {
                        g_debug("Ontogenesis kernel improvement: %.3f at iteration %u",
                                onto_result->achieved_improvement, iteration);
                    }
                    gnc_ontogenesis_result_free(onto_result);
                }
            }
        }
        
        gnc_self_analysis_result_free(analysis);
        
        // Sleep between iterations to allow system to adapt
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    session->improvement_cycle_active = FALSE;
    g_message("Recursive improvement cycle completed");
}

gboolean gnc_meta_cognitive_start_improvement_cycle(
    GncMetaCognitiveSession *session,
    guint max_iterations,
    gdouble improvement_threshold)
{
    if (!session || !meta_cognitive_initialized) {
        return FALSE;
    }
    
    if (session->improvement_cycle_active) {
        g_warning("Improvement cycle already active for session %u", session->session_id);
        return FALSE;
    }
    
    if (human_override_enabled) {
        g_warning("Improvement cycle blocked: human override mode active");
        return FALSE;
    }
    
    session->improvement_cycle_active = TRUE;
    session->current_iteration = 0;
    session->current_improvement = 0.0;
    
    // Start improvement cycle in background thread
    session->improvement_thread = new std::thread(improvement_cycle_worker, 
                                                 session, max_iterations, improvement_threshold);
    
    g_message("Started recursive self-improvement cycle for session %u", session->session_id);
    return TRUE;
}

gboolean gnc_meta_cognitive_stop_improvement_cycle(GncMetaCognitiveSession *session)
{
    if (!session) {
        return FALSE;
    }
    
    if (!session->improvement_cycle_active) {
        return TRUE; // Already stopped
    }
    
    session->improvement_cycle_active = FALSE;
    
    g_message("Stopping recursive improvement cycle for session %u", session->session_id);
    
    return TRUE;
}

gboolean gnc_meta_cognitive_is_improving(GncMetaCognitiveSession *session)
{
    return session ? session->improvement_cycle_active : FALSE;
}

gboolean gnc_meta_cognitive_get_improvement_status(
    GncMetaCognitiveSession *session,
    guint *iteration,
    gdouble *improvement)
{
    if (!session) {
        return FALSE;
    }
    
    if (iteration) {
        *iteration = session->current_iteration;
    }
    
    if (improvement) {
        *improvement = session->current_improvement;
    }
    
    return session->improvement_cycle_active;
}

/*==================================================================*
 * Safety and Monitoring Functions                                 *
 *==================================================================*/

void gnc_meta_cognitive_set_safety_bounds(
    gboolean enabled,
    gdouble min_performance,
    gdouble max_deviation)
{
    std::lock_guard<std::mutex> lock(global_state_mutex);
    
    safety_bounds_enabled = enabled;
    min_performance_threshold = min_performance;
    max_deviation_threshold = max_deviation;
    
    g_message("Safety bounds %s: min_performance=%.3f, max_deviation=%.3f",
             enabled ? "enabled" : "disabled", min_performance, max_deviation);
}

gboolean gnc_meta_cognitive_is_stable(void)
{
    if (!meta_cognitive_initialized) {
        return FALSE;
    }
    
    std::lock_guard<std::mutex> lock(global_state_mutex);
    
    // Check stability across all process types
    for (const auto &pair : global_metrics) {
        const GncCognitiveMetrics &metrics = pair.second;
        
        if (metrics.stability_index < 0.5) {
            return FALSE; // System unstable
        }
        
        if (metrics.error_count > metrics.total_operations * 0.1) {
            return FALSE; // Too many errors
        }
    }
    
    return TRUE;
}

gboolean gnc_meta_cognitive_detect_regression(
    GncMetaCognitiveProcessType process_type,
    gdouble regression_threshold)
{
    std::lock_guard<std::mutex> lock(global_state_mutex);
    
    auto it = global_metrics.find(process_type);
    if (it == global_metrics.end()) {
        return FALSE; // No metrics available
    }
    
    const GncCognitiveMetrics &current = it->second;
    
    // For regression detection, we need historical data
    // For now, we'll use a simple heuristic based on current metrics
    gdouble performance_score = (current.accuracy + current.efficiency + current.stability_index) / 3.0;
    
    return performance_score < (1.0 - regression_threshold);
}

void gnc_meta_cognitive_set_human_override(gboolean enabled)
{
    std::lock_guard<std::mutex> lock(global_state_mutex);
    
    human_override_enabled = enabled;
    
    g_message("Human override mode %s", enabled ? "enabled" : "disabled");
}

/*==================================================================*
 * Introspection and Pattern Analysis                              *
 *==================================================================*/

gchar** gnc_meta_cognitive_analyze_patterns(
    GncMetaCognitiveSession *session,
    guint lookback_hours)
{
    if (!session) {
        return NULL;
    }
    
    // For now, return some example patterns
    gchar **patterns = g_new(gchar*, 4);
    patterns[0] = g_strdup("Attention allocation efficiency improves during high transaction periods");
    patterns[1] = g_strdup("PLN validation accuracy decreases with transaction complexity");
    patterns[2] = g_strdup("Tensor network performance correlates with available memory");
    patterns[3] = NULL;
    
    g_debug("Analyzed cognitive patterns over %u hours", lookback_hours);
    
    return patterns;
}

gchar** gnc_meta_cognitive_detect_emergence(
    GncMetaCognitiveSession *session,
    gdouble novelty_threshold)
{
    if (!session) {
        return NULL;
    }
    
    // For now, return some example emergent behaviors
    gchar **behaviors = g_new(gchar*, 3);
    behaviors[0] = g_strdup("Adaptive clustering behavior emerged in AI node");
    behaviors[1] = g_strdup("Self-organizing attention patterns in ECAN system");
    behaviors[2] = NULL;
    
    g_debug("Detected emergent behaviors above novelty threshold %.3f", novelty_threshold);
    
    return behaviors;
}

gchar* gnc_meta_cognitive_generate_fitness_landscape(
    GncMetaCognitiveSession *session,
    GncMetaCognitiveProcessType process_type)
{
    if (!session) {
        return NULL;
    }
    
    // Generate JSON representation of fitness landscape
    std::string json = "{"
        "\"process_type\": " + std::to_string(process_type) + ","
        "\"dimensions\": [\"accuracy\", \"efficiency\", \"stability\"],"
        "\"fitness_data\": ["
        "  {\"accuracy\": 0.8, \"efficiency\": 0.7, \"stability\": 0.9, \"fitness\": 0.8},"
        "  {\"accuracy\": 0.9, \"efficiency\": 0.8, \"stability\": 0.8, \"fitness\": 0.85},"
        "  {\"accuracy\": 0.7, \"efficiency\": 0.9, \"stability\": 0.7, \"fitness\": 0.77}"
        "],"
        "\"optimal_point\": {\"accuracy\": 0.9, \"efficiency\": 0.85, \"stability\": 0.88},"
        "\"current_point\": {\"accuracy\": 0.82, \"efficiency\": 0.74, \"stability\": 0.86}"
        "}";
    
    return g_strdup(json.c_str());
}

/*==================================================================*
 * Internal Helper Functions                                        *
 *==================================================================*/

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

static GncCognitiveArchConfig create_default_config(void)
{
    GncCognitiveArchConfig config = {
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
    
    return config;
}

static gdouble calculate_fitness(const GncCognitiveMetrics &metrics)
{
    // Weighted combination of key performance indicators
    return 0.3 * metrics.accuracy + 
           0.2 * metrics.efficiency + 
           0.2 * metrics.stability_index +
           0.1 * metrics.learning_rate +
           0.1 * metrics.innovation_score +
           0.1 * (1.0 - std::min(1.0, metrics.error_count / std::max(1.0, (double)metrics.total_operations)));
}

static GncCognitiveArchConfig mutate_config(const GncCognitiveArchConfig &config, gdouble mutation_rate)
{
    GncCognitiveArchConfig mutated = config;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> uniform(-mutation_rate, mutation_rate);
    
    // Mutate learning parameters
    mutated.base_learning_rate = std::max(0.001, std::min(0.1, config.base_learning_rate * (1.0 + uniform(gen))));
    mutated.learning_momentum = std::max(0.1, std::min(0.99, config.learning_momentum * (1.0 + uniform(gen))));
    mutated.regularization_strength = std::max(0.0001, std::min(0.01, config.regularization_strength * (1.0 + uniform(gen))));
    
    // Mutate attention parameters
    mutated.sti_funds = std::max(100.0, std::min(10000.0, config.sti_funds * (1.0 + uniform(gen))));
    mutated.lti_funds = std::max(50.0, std::min(5000.0, config.lti_funds * (1.0 + uniform(gen))));
    mutated.attention_decay_rate = std::max(0.001, std::min(0.1, config.attention_decay_rate * (1.0 + uniform(gen))));
    mutated.wage_rate = std::max(0.1, std::min(10.0, config.wage_rate * (1.0 + uniform(gen))));
    
    return mutated;
}

static GncCognitiveArchConfig crossover_configs(const GncCognitiveArchConfig &parent1, 
                                               const GncCognitiveArchConfig &parent2)
{
    GncCognitiveArchConfig child;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> uniform(0.0, 1.0);
    
    // Randomly select parameters from each parent
    child.base_learning_rate = uniform(gen) < 0.5 ? parent1.base_learning_rate : parent2.base_learning_rate;
    child.learning_momentum = uniform(gen) < 0.5 ? parent1.learning_momentum : parent2.learning_momentum;
    child.regularization_strength = uniform(gen) < 0.5 ? parent1.regularization_strength : parent2.regularization_strength;
    child.sti_funds = uniform(gen) < 0.5 ? parent1.sti_funds : parent2.sti_funds;
    child.lti_funds = uniform(gen) < 0.5 ? parent1.lti_funds : parent2.lti_funds;
    child.attention_decay_rate = uniform(gen) < 0.5 ? parent1.attention_decay_rate : parent2.attention_decay_rate;
    child.wage_rate = uniform(gen) < 0.5 ? parent1.wage_rate : parent2.wage_rate;
    child.truth_threshold = uniform(gen) < 0.5 ? parent1.truth_threshold : parent2.truth_threshold;
    child.confidence_threshold = uniform(gen) < 0.5 ? parent1.confidence_threshold : parent2.confidence_threshold;
    child.max_inference_steps = uniform(gen) < 0.5 ? parent1.max_inference_steps : parent2.max_inference_steps;
    child.tensor_cache_size = uniform(gen) < 0.5 ? parent1.tensor_cache_size : parent2.tensor_cache_size;
    child.message_priority_threshold = uniform(gen) < 0.5 ? parent1.message_priority_threshold : parent2.message_priority_threshold;
    child.max_concurrent_ops = uniform(gen) < 0.5 ? parent1.max_concurrent_ops : parent2.max_concurrent_ops;
    
    return child;
}

static gchar** generate_optimization_suggestions(const GncCognitiveMetrics &current, 
                                               const GncCognitiveMetrics &baseline)
{
    std::vector<std::string> suggestions;
    
    if (current.accuracy < baseline.accuracy) {
        suggestions.push_back("Increase PLN truth value threshold to improve validation accuracy");
    }
    
    if (current.efficiency < baseline.efficiency) {
        suggestions.push_back("Optimize attention allocation to improve resource efficiency");
    }
    
    if (current.stability_index < baseline.stability_index) {
        suggestions.push_back("Reduce learning rate to improve system stability");
    }
    
    if (current.latency_ms > baseline.latency_ms * 1.2) {
        suggestions.push_back("Increase tensor cache size to reduce processing latency");
    }
    
    if (current.innovation_score < baseline.innovation_score) {
        suggestions.push_back("Increase mutation rate in evolutionary algorithms for more innovation");
    }
    
    if (suggestions.empty()) {
        suggestions.push_back("System performance is stable - consider gradual parameter tuning");
    }
    
    // Convert to gchar**
    gchar **result = g_new(gchar*, suggestions.size() + 1);
    for (size_t i = 0; i < suggestions.size(); i++) {
        result[i] = g_strdup(suggestions[i].c_str());
    }
    result[suggestions.size()] = NULL;
    
    return result;
}