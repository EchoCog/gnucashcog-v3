/********************************************************************\
 * gnc-ko6ml-translation.h -- ko6ml <-> Hypergraph Translation     *
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
 * @file gnc-ko6ml-translation.h
 * @brief Phase 1: ko6ml Primitives & Foundational Hypergraph Encoding
 * @author GnuCash Cognitive Engine
 *
 * Atomic vocabulary and bidirectional translation mechanisms between
 * ko6ml agentic primitives and AtomSpace-style hypergraph patterns.
 *
 * Components:
 *  - ko6ml primitive vocabulary (agent / state / action / percept /
 *    memory / goal / constraint / relation)
 *  - Tensor fragment architecture with canonical shape
 *    [modality, depth, context, salience, autonomy_index]
 *  - Prime-factorization tensor signatures (Goedel encoding over the
 *    primes 2, 3, 5, 7, 11 -- unique by the fundamental theorem of
 *    arithmetic)
 *  - Bidirectional ko6ml <-> hypergraph fragment translation with
 *    round-trip integrity verification
 *  - Scheme cognitive grammar adapter: s-expression encoder plus a
 *    full recursive-descent parser for lossless round-trips
 *
 * This module depends only on GLib so that the translation layer can
 * be exercised by the standalone verification pipeline
 * (test-phase1-ko6ml-roundtrip.sh) as well as by the engine test
 * suite.  Instantiation of fragments into the live AtomSpace is
 * provided by gnc-ko6ml-atomspace.h.
 */

#ifndef GNC_KO6ML_TRANSLATION_H
#define GNC_KO6ML_TRANSLATION_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup Ko6mlTranslation
 * @{
 */

/**
 * @name ko6ml Primitive Vocabulary
 * The atomic agentic vocabulary of the cognitive grammar network.
 */

/** ko6ml primitive types -- the atomic agentic vocabulary */
typedef enum {
    GNC_KO6ML_AGENT = 0,   /**< Autonomous agentic entity */
    GNC_KO6ML_STATE,       /**< Agent state snapshot */
    GNC_KO6ML_ACTION,      /**< Agentic transformation */
    GNC_KO6ML_PERCEPT,     /**< Sensory input primitive */
    GNC_KO6ML_MEMORY,      /**< Persistent memory element */
    GNC_KO6ML_GOAL,        /**< Intentional target */
    GNC_KO6ML_CONSTRAINT,  /**< Boundary condition */
    GNC_KO6ML_RELATION,    /**< Inter-primitive relation */
    GNC_KO6ML_N_PRIMITIVE_TYPES /**< Number of primitive types */
} GncKo6mlPrimitiveType;

/** Return the canonical name of a ko6ml primitive type
 * @param type Primitive type
 * @return Static string name (e.g. "AGENT"), or NULL if invalid */
const gchar* gnc_ko6ml_primitive_type_name(GncKo6mlPrimitiveType type);

/** Parse a canonical primitive type name back to its enum value
 * @param name Canonical name as returned by
 *        gnc_ko6ml_primitive_type_name()
 * @param type Output parameter for the parsed type
 * @return TRUE if the name is a valid primitive type name */
gboolean gnc_ko6ml_primitive_type_from_name(const gchar *name,
                                            GncKo6mlPrimitiveType *type);

/**
 * @name Tensor Fragment Architecture
 * Agent/state encoded with canonical tensor shape
 * [modality, depth, context, salience, autonomy_index].
 */

/** Number of dimensions in the canonical tensor fragment shape */
#define GNC_KO6ML_TENSOR_DIMS 5

/** Canonical per-dimension bounds.  The bounds guarantee that the
 * prime-factorization signature 2^m * 3^d * 5^c * 7^s * 11^a fits in
 * 128 bits (maximum signature ~2.0e36 < 2^128). */
#define GNC_KO6ML_MAX_MODALITY        8
#define GNC_KO6ML_MAX_DEPTH          16
#define GNC_KO6ML_MAX_CONTEXT        16
#define GNC_KO6ML_MAX_SALIENCE        8
#define GNC_KO6ML_MAX_AUTONOMY_INDEX  8

/** Tensor fragment shape: [modality, depth, context, salience,
 *  autonomy_index].  Every dimension must be >= 1 and within its
 *  canonical bound. */
typedef struct {
    guint modality;        /**< Sensory/data modality count (prime 2) */
    guint depth;           /**< Hypergraph recursion depth (prime 3) */
    guint context;         /**< Contextual embedding width (prime 5) */
    guint salience;        /**< Attention salience resolution (prime 7) */
    guint autonomy_index;  /**< Agent autonomy resolution (prime 11) */
} GncKo6mlTensorShape;

/** 128-bit prime-factorization tensor signature, split into two
 *  64-bit halves.  signature = 2^modality * 3^depth * 5^context *
 *  7^salience * 11^autonomy_index.  Unique per shape by the
 *  fundamental theorem of arithmetic. */
typedef struct {
    guint64 hi;            /**< Upper 64 bits of the signature */
    guint64 lo;            /**< Lower 64 bits of the signature */
} GncKo6mlTensorSignature;

/** Tensor fragment: shape plus a dense element buffer of
 *  modality * depth * context * salience * autonomy_index doubles. */
typedef struct {
    GncKo6mlTensorShape shape;  /**< Canonical 5-dimensional shape */
    gdouble *elements;          /**< Dense element buffer */
    gsize element_count;        /**< Number of elements in buffer */
} GncKo6mlTensorFragment;

/** Validate a tensor shape against the canonical bounds
 * @param shape Shape to validate
 * @param error_msg Optional output for a newly-allocated diagnostic
 *        message when validation fails (caller frees with g_free)
 * @return TRUE if every dimension is >= 1 and within bounds */
gboolean gnc_ko6ml_tensor_shape_validate(const GncKo6mlTensorShape *shape,
                                         gchar **error_msg);

/** Compute the prime-factorization signature of a validated shape
 * @param shape Validated tensor shape
 * @param signature Output signature
 * @return TRUE on success, FALSE if the shape is invalid */
gboolean gnc_ko6ml_tensor_shape_signature(const GncKo6mlTensorShape *shape,
                                          GncKo6mlTensorSignature *signature);

/** Recover a tensor shape from its prime-factorization signature by
 *  repeated prime division (inverse Goedel mapping)
 * @param signature Signature to factorize
 * @param shape Output shape
 * @return TRUE if the signature factorizes exactly into
 *         2^m 3^d 5^c 7^s 11^a within canonical bounds */
gboolean gnc_ko6ml_tensor_shape_from_signature(
    const GncKo6mlTensorSignature *signature,
    GncKo6mlTensorShape *shape);

/** Render a signature as a decimal string
 * @param signature Signature to render
 * @return Newly-allocated decimal string (caller frees with g_free) */
gchar* gnc_ko6ml_tensor_signature_to_string(
    const GncKo6mlTensorSignature *signature);

/** Create a tensor fragment with a zero-initialized element buffer
 * @param shape Validated tensor shape
 * @return New fragment, or NULL if the shape is invalid */
GncKo6mlTensorFragment* gnc_ko6ml_tensor_fragment_create(
    const GncKo6mlTensorShape *shape);

/** Free a tensor fragment */
void gnc_ko6ml_tensor_fragment_free(GncKo6mlTensorFragment *fragment);

/** Element accessor using canonical dimension order
 *  [modality, depth, context, salience, autonomy_index]
 * @return TRUE if the indices are in range */
gboolean gnc_ko6ml_tensor_fragment_set(GncKo6mlTensorFragment *fragment,
                                       guint m, guint d, guint c,
                                       guint s, guint a, gdouble value);

/** Element getter using canonical dimension order
 * @return TRUE if the indices are in range */
gboolean gnc_ko6ml_tensor_fragment_get(const GncKo6mlTensorFragment *fragment,
                                       guint m, guint d, guint c,
                                       guint s, guint a, gdouble *value);

/**
 * @name ko6ml Primitive Instances
 */

/** A ko6ml primitive instance: the unit of translation */
typedef struct {
    GncKo6mlPrimitiveType type;   /**< Primitive type */
    gchar *name;                  /**< Unique primitive name */
    GncKo6mlTensorShape shape;    /**< Tensor fragment shape */
    gdouble strength;             /**< Truth strength [0,1] */
    gdouble confidence;           /**< Truth confidence [0,1] */
    GHashTable *properties;       /**< string -> string properties */
} GncKo6mlPrimitive;

/** Create a ko6ml primitive
 * @param type Primitive type
 * @param name Unique primitive name (must be non-empty)
 * @param shape Validated tensor shape
 * @param strength Truth strength in [0,1]
 * @param confidence Truth confidence in [0,1]
 * @return New primitive, or NULL on invalid arguments */
GncKo6mlPrimitive* gnc_ko6ml_primitive_create(GncKo6mlPrimitiveType type,
                                              const gchar *name,
                                              const GncKo6mlTensorShape *shape,
                                              gdouble strength,
                                              gdouble confidence);

/** Free a ko6ml primitive */
void gnc_ko6ml_primitive_free(GncKo6mlPrimitive *primitive);

/** Set a string property on a primitive (replaces existing value) */
void gnc_ko6ml_primitive_set_property(GncKo6mlPrimitive *primitive,
                                      const gchar *key,
                                      const gchar *value);

/** Get a string property from a primitive
 * @return Property value owned by the primitive, or NULL */
const gchar* gnc_ko6ml_primitive_get_property(
    const GncKo6mlPrimitive *primitive, const gchar *key);

/** Structural equality of two primitives (type, name, shape, truth
 *  values within 1e-9, and all properties)
 * @return TRUE if the primitives are structurally identical */
gboolean gnc_ko6ml_primitive_equal(const GncKo6mlPrimitive *a,
                                   const GncKo6mlPrimitive *b);

/**
 * @name Hypergraph Fragment Encoding
 * AtomSpace-compatible hypergraph representation of ko6ml
 * primitives.  Node/link types mirror the OpenCog-aligned
 * GncAtomType vocabulary; gnc-ko6ml-atomspace.h instantiates these
 * fragments as real atoms.
 */

/** Hypergraph atom types used by ko6ml fragment encoding */
typedef enum {
    GNC_KO6ML_NODE_CONCEPT = 0,     /**< ConceptNode */
    GNC_KO6ML_NODE_PREDICATE,       /**< PredicateNode */
    GNC_KO6ML_NODE_NUMBER,          /**< NumberNode */
    GNC_KO6ML_LINK_INHERITANCE,     /**< InheritanceLink */
    GNC_KO6ML_LINK_EVALUATION       /**< EvaluationLink */
} GncKo6mlAtomType;

/** A node or link within a hypergraph fragment */
typedef struct {
    GncKo6mlAtomType atom_type;   /**< Atom type */
    gchar *name;                  /**< Node name / link label */
    gdouble strength;             /**< Truth strength [0,1] */
    gdouble confidence;           /**< Truth confidence [0,1] */
    guint *outgoing;              /**< Indices of outgoing atoms (links) */
    guint outgoing_count;         /**< Outgoing set size (0 for nodes) */
} GncKo6mlHypergraphAtom;

/** A hypergraph fragment: ordered atom list where links reference
 *  earlier atoms by index (a valid hypergraph topological order) */
typedef struct {
    GPtrArray *atoms;             /**< Array of GncKo6mlHypergraphAtom* */
} GncKo6mlHypergraphFragment;

/** Free a hypergraph fragment and all contained atoms */
void gnc_ko6ml_hypergraph_fragment_free(GncKo6mlHypergraphFragment *fragment);

/** Translate a ko6ml primitive into a hypergraph fragment.
 *  Encoding (see PHASE1_KO6ML_TRANSLATION.md for the flowchart):
 *   - ConceptNode "Ko6ml:<TYPE>:<name>" carrying the truth value
 *   - InheritanceLink to ConceptNode "Ko6mlType:<TYPE>"
 *   - EvaluationLink hasTensorShape -> NumberNode of the
 *     prime-factorization signature
 *   - EvaluationLink hasProperty:<key> -> ConceptNode of the value
 * @param primitive Primitive to translate
 * @return New hypergraph fragment, or NULL on invalid input */
GncKo6mlHypergraphFragment* gnc_ko6ml_primitive_to_hypergraph(
    const GncKo6mlPrimitive *primitive);

/** Translate a hypergraph fragment produced by
 *  gnc_ko6ml_primitive_to_hypergraph() back into a ko6ml primitive
 * @param fragment Fragment to translate
 * @return Reconstructed primitive, or NULL if the fragment does not
 *         encode a valid ko6ml primitive */
GncKo6mlPrimitive* gnc_ko6ml_hypergraph_to_primitive(
    const GncKo6mlHypergraphFragment *fragment);

/** Verify round-trip integrity: primitive -> hypergraph -> primitive
 *  must reproduce a structurally identical primitive
 * @param primitive Primitive to verify
 * @return TRUE if the round trip preserves all information */
gboolean gnc_ko6ml_verify_hypergraph_roundtrip(
    const GncKo6mlPrimitive *primitive);

/**
 * @name Scheme Cognitive Grammar Adapter
 * Lossless s-expression encoding of ko6ml primitives with a full
 * recursive-descent parser for the reverse direction.
 */

/** Encode a primitive as a Scheme s-expression of the form
 *  (Ko6mlPrimitive (type "AGENT") (name "...")
 *    (tensor-shape (modality M) (depth D) (context C)
 *                  (salience S) (autonomy-index A))
 *    (truth (strength X) (confidence Y))
 *    (properties (prop "key" "value") ...))
 * @param primitive Primitive to encode
 * @return Newly-allocated s-expression (caller frees with g_free) */
gchar* gnc_ko6ml_primitive_to_scheme(const GncKo6mlPrimitive *primitive);

/** Parse a Ko6mlPrimitive s-expression back into a primitive
 * @param scheme_expr S-expression as produced by
 *        gnc_ko6ml_primitive_to_scheme()
 * @return Reconstructed primitive, or NULL on parse error */
GncKo6mlPrimitive* gnc_ko6ml_primitive_from_scheme(const gchar *scheme_expr);

/** Verify round-trip integrity: primitive -> scheme -> primitive
 * @param primitive Primitive to verify
 * @return TRUE if the round trip preserves all information */
gboolean gnc_ko6ml_verify_scheme_roundtrip(
    const GncKo6mlPrimitive *primitive);

/**
 * @name Verification Protocol
 */

/** Round-trip benchmark results (real translations, no simulation) */
typedef struct {
    guint iterations;             /**< Number of round trips performed */
    gdouble hypergraph_us_per_op; /**< Mean microseconds per hypergraph
                                       round trip */
    gdouble scheme_us_per_op;     /**< Mean microseconds per scheme
                                       round trip */
    gboolean all_roundtrips_ok;   /**< TRUE if every round trip
                                       preserved integrity */
} GncKo6mlBenchmarkResult;

/** Benchmark bidirectional translation over every primitive type
 * @param iterations Round trips per primitive type (>= 1)
 * @param result Output benchmark result
 * @return TRUE if the benchmark ran and all round trips succeeded */
gboolean gnc_ko6ml_benchmark_roundtrip(guint iterations,
                                       GncKo6mlBenchmarkResult *result);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* GNC_KO6ML_TRANSLATION_H */
