/********************************************************************\
 * gnc-cognitive-comms.h -- Inter-module communication protocols   *
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
/** @addtogroup CognitiveCommunications
    Inter-module communication protocols for distributed cognition
    and emergent cognitive architectures in the accounting system.
    @{ */

/** @file gnc-cognitive-comms.h
    @brief Inter-module communication for distributed cognitive processing
    @author Copyright (C) 2024 GnuCash Cognitive Engine
*/

#ifndef GNC_COGNITIVE_COMMS_H
#define GNC_COGNITIVE_COMMS_H

#include "gnc-cognitive-accounting.h"
#include "gnc-engine.h"
#include <glib.h>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

/** @name Cognitive Module Definitions */
/** @{ */

/** OpenCog modules in the cognitive architecture */
typedef enum {
    GNC_MODULE_ATOMSPACE = 0,     /**< AtomSpace hypergraph database */
    GNC_MODULE_PLN,               /**< Probabilistic Logic Networks */
    GNC_MODULE_ECAN,              /**< Economic Attention Allocation */
    GNC_MODULE_MOSES,             /**< Meta-Optimizing Semantic Evolutionary Search */
    GNC_MODULE_URE,               /**< Uncertain Reasoning Engine */
    GNC_MODULE_COGSERVER,         /**< Cognitive Server for network access */
    GNC_MODULE_SCHEME             /**< Scheme-based representations */
} GncCognitiveModule;

/** Message types for inter-module communication */
typedef enum {
    GNC_MSG_DATA_UPDATE = 0,      /**< Data update notification */
    GNC_MSG_ATTENTION_REQUEST,    /**< Request for attention allocation */
    GNC_MSG_ATTENTION_REALLOCATION, /**< Attention reallocation signal */
    GNC_MSG_PATTERN_MATCH,        /**< Pattern matching request */
    GNC_MSG_PATTERN_RESONANCE,    /**< Pattern resonance notification */
    GNC_MSG_INFERENCE_REQUEST,    /**< Inference processing request */
    GNC_MSG_EMERGENCE_ACTIVATION, /**< Emergent cognition activation */
    GNC_MSG_SYNCHRONIZATION       /**< Module synchronization signal */
} GncCognitiveMessageType;

/** Cognitive pattern types for emergent processing */
typedef enum {
    GNC_PATTERN_ACTIVATION = 0,   /**< Module activation pattern */
    GNC_PATTERN_RESONANCE,        /**< Cross-module resonance pattern */
    GNC_PATTERN_EMERGENCE,        /**< Emergent insight pattern */
    GNC_PATTERN_ATTENTION         /**< Attention flow pattern */
} GncCognitivePatternType;

/** @} */

/** @name Communication Structures */
/** @{ */

/** Message structure for inter-module communication */
typedef struct {
    GncCognitiveModule from_module;     /**< Source module */
    GncCognitiveModule to_module;       /**< Destination module */
    GncCognitiveMessageType message_type; /**< Type of message */
    gpointer data;                      /**< Message data payload */
    gint64 timestamp;                   /**< Message timestamp */
} GncCognitiveMessage;

/** Cognitive pattern for emergent processing */
typedef struct {
    GncCognitiveModule trigger_module;  /**< Module that triggered pattern */
    GncCognitivePatternType pattern_type; /**< Type of cognitive pattern */
    gdouble strength;                   /**< Pattern strength (0.0-1.0) */
    gint64 timestamp;                   /**< Pattern creation timestamp */
} GncCognitivePattern;

/** @} */

/** @name Communication Hub Functions */
/** @{ */

/** Initialize cognitive communication hub
 * @return TRUE on success, FALSE on failure
 */
gboolean gnc_cognitive_comms_init(void);

/** Shutdown cognitive communication hub */
void gnc_cognitive_comms_shutdown(void);

/** Register a cognitive module with the communication hub
 * @param module Module to register
 * @return TRUE on success, FALSE on failure
 */
gboolean gnc_cognitive_register_module(GncCognitiveModule module);

/** @} */

/** @name Message Passing Functions */
/** @{ */

/** Send message between cognitive modules
 * @param from_module Source module
 * @param to_module Destination module
 * @param msg_type Message type
 * @param data Message data
 */
void gnc_cognitive_send_message(GncCognitiveModule from_module, 
                               GncCognitiveModule to_module,
                               GncCognitiveMessageType msg_type,
                               gpointer data);

/** Receive messages for a cognitive module
 * @param module Module to receive messages for
 * @return Vector of received messages
 *
 * Declared with C++ linkage (outside the extern "C" block below) since
 * std::vector<GncCognitiveMessage> is not a type C linkage can express.
 */
#ifdef __cplusplus
}
#endif
std::vector<GncCognitiveMessage> gnc_cognitive_receive_messages(GncCognitiveModule module);
#ifdef __cplusplus
extern "C" {
#endif

/** Broadcast message to all active modules
 * @param from_module Source module
 * @param msg_type Message type
 * @param data Message data
 */
void gnc_cognitive_broadcast_message(GncCognitiveModule from_module,
                                    GncCognitiveMessageType msg_type,
                                    gpointer data);

/** @} */

/** @name Emergent Cognition Functions */
/** @{ */

/** Trigger emergent cognitive processing from module activation
 * @param trigger_module Module that triggered emergence
 */
void gnc_cognitive_trigger_emergence(GncCognitiveModule trigger_module);

/** Process active emergent cognitive patterns */
void gnc_cognitive_process_emergent_patterns(void);

/** Amplify cognitive pattern across distributed modules
 * @param pattern Pattern to amplify
 */
void gnc_cognitive_amplify_pattern(const GncCognitivePattern& pattern);

/** Detect emergent cognitive insights from pattern interactions */
void gnc_cognitive_detect_emergent_insights(void);

/** Generate cognitive insight from pattern
 * @param pattern Pattern that generated insight
 */
void gnc_cognitive_generate_insight(const GncCognitivePattern& pattern);

/** @} */

/** @name Adaptive Attention Allocation */
/** @{ */

/** Optimize attention flow across cognitive modules */
void gnc_cognitive_optimize_attention_flow(void);

/** Synchronize cognitive modules for distributed processing */
void gnc_cognitive_synchronize_modules(void);

/** @} */

/** @name Utility Functions */
/** @{ */

/** Get human-readable name for cognitive module
 * @param module Module to get name for
 * @return Module name string
 */
const gchar* gnc_cognitive_module_name(GncCognitiveModule module);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* GNC_COGNITIVE_COMMS_H */
/** @} */
/** @} */