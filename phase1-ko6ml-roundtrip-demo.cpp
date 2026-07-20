/********************************************************************\
 * phase1-ko6ml-roundtrip-demo.cpp -- Phase 1 Verification Demo    *
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
 * @file phase1-ko6ml-roundtrip-demo.cpp
 * @brief Phase 1 verification pipeline: exercises the bidirectional
 *        ko6ml <-> hypergraph and ko6ml <-> Scheme translations for
 *        every primitive type with real data, benchmarks the
 *        translations, and emits Graphviz DOT flowcharts of the
 *        generated hypergraph fragments.
 *
 * Build (depends only on GLib, see test-phase1-ko6ml-roundtrip.sh):
 *   g++ -std=c++17 -I libgnucash/engine $(pkg-config --cflags glib-2.0) \
 *       phase1-ko6ml-roundtrip-demo.cpp \
 *       libgnucash/engine/gnc-ko6ml-translation.cpp \
 *       $(pkg-config --libs glib-2.0) -o phase1-ko6ml-roundtrip-demo
 *
 * Exit status is non-zero when any verification step fails, so the
 * program doubles as an automated verification gate.
 */

#include <cstdio>
#include <cstring>

#include "gnc-ko6ml-translation.h"

static guint failures = 0;

static void check(gboolean condition, const gchar *description)
{
    if (condition) {
        printf("  ✅ %s\n", description);
    } else {
        printf("  ❌ %s\n", description);
        failures++;
    }
}

static const gchar* atom_type_label(GncKo6mlAtomType type)
{
    switch (type) {
    case GNC_KO6ML_NODE_CONCEPT: return "ConceptNode";
    case GNC_KO6ML_NODE_PREDICATE: return "PredicateNode";
    case GNC_KO6ML_NODE_NUMBER: return "NumberNode";
    case GNC_KO6ML_LINK_INHERITANCE: return "InheritanceLink";
    case GNC_KO6ML_LINK_EVALUATION: return "EvaluationLink";
    }
    return "?";
}

/** Emit a Graphviz DOT flowchart of a hypergraph fragment --
 *  the Phase 1 hypergraph visualization tool. */
static void emit_fragment_dot(const GncKo6mlHypergraphFragment *fragment,
                              const gchar *graph_name)
{
    printf("digraph \"%s\" {\n", graph_name);
    printf("  rankdir=LR;\n  node [shape=box, fontname=\"monospace\"];\n");
    for (guint i = 0; i < fragment->atoms->len; i++) {
        GncKo6mlHypergraphAtom *atom =
            (GncKo6mlHypergraphAtom*)g_ptr_array_index(fragment->atoms, i);
        gchar *escaped = g_strescape(atom->name, NULL);
        printf("  a%u [label=\"%s\\n%s\\n(%.2f, %.2f)\"%s];\n",
               i, atom_type_label(atom->atom_type), escaped,
               atom->strength, atom->confidence,
               atom->outgoing_count ? ", shape=ellipse" : "");
        g_free(escaped);
        for (guint j = 0; j < atom->outgoing_count; j++)
            printf("  a%u -> a%u [label=\"%s\"];\n",
                   i, atom->outgoing[j], j == 0 ? "source" : "target");
    }
    printf("}\n");
}

int main(void)
{
    printf("========================================================================\n");
    printf("  Phase 1: ko6ml Primitives & Foundational Hypergraph Encoding\n");
    printf("  Bidirectional Translation Verification Pipeline (real data, no mocks)\n");
    printf("========================================================================\n\n");

    /* ---- Tensor fragment architecture ---------------------------- */
    printf("🧮 Tensor Fragment Architecture "
           "[modality, depth, context, salience, autonomy_index]\n");

    GncKo6mlTensorShape shape = { 3, 4, 5, 2, 6 };
    check(gnc_ko6ml_tensor_shape_validate(&shape, NULL),
          "canonical shape [3,4,5,2,6] validates");

    GncKo6mlTensorSignature signature;
    check(gnc_ko6ml_tensor_shape_signature(&shape, &signature),
          "prime-factorization signature computed");
    gchar *sig_str = gnc_ko6ml_tensor_signature_to_string(&signature);
    printf("     signature 2^3 * 3^4 * 5^5 * 7^2 * 11^6 = %s\n", sig_str);
    g_free(sig_str);

    GncKo6mlTensorShape recovered_shape;
    check(gnc_ko6ml_tensor_shape_from_signature(&signature,
                                                &recovered_shape) &&
          memcmp(&shape, &recovered_shape, sizeof(shape)) == 0,
          "inverse prime factorization recovers the exact shape");

    GncKo6mlTensorShape invalid = { 0, 1, 1, 1, 1 };
    gchar *error_msg = NULL;
    check(!gnc_ko6ml_tensor_shape_validate(&invalid, &error_msg),
          "invalid shape rejected with diagnostic");
    if (error_msg) {
        printf("     diagnostic: %s\n", error_msg);
        g_free(error_msg);
    }

    GncKo6mlTensorFragment *tensor = gnc_ko6ml_tensor_fragment_create(&shape);
    check(tensor != NULL &&
          tensor->element_count == (gsize)3 * 4 * 5 * 2 * 6,
          "tensor fragment allocates 3*4*5*2*6 = 720 elements");
    check(gnc_ko6ml_tensor_fragment_set(tensor, 2, 3, 4, 1, 5, 0.42),
          "element write at the far corner succeeds");
    gdouble element = 0.0;
    check(gnc_ko6ml_tensor_fragment_get(tensor, 2, 3, 4, 1, 5, &element) &&
          element == 0.42,
          "element read returns the written value");
    gnc_ko6ml_tensor_fragment_free(tensor);
    printf("\n");

    /* ---- Round trips over the whole vocabulary ------------------- */
    printf("🔁 Bidirectional Translation: every ko6ml primitive type\n");
    for (gint t = 0; t < GNC_KO6ML_N_PRIMITIVE_TYPES; t++) {
        const gchar *type_name =
            gnc_ko6ml_primitive_type_name((GncKo6mlPrimitiveType)t);

        GncKo6mlTensorShape s = {
            (guint)(1 + t % GNC_KO6ML_MAX_MODALITY),
            (guint)(1 + t % GNC_KO6ML_MAX_DEPTH),
            (guint)(1 + (2 * t) % GNC_KO6ML_MAX_CONTEXT),
            (guint)(1 + t % GNC_KO6ML_MAX_SALIENCE),
            (guint)(1 + t % GNC_KO6ML_MAX_AUTONOMY_INDEX)
        };
        gchar *name = g_strdup_printf("ledger:%s:alpha", type_name);
        GncKo6mlPrimitive *primitive = gnc_ko6ml_primitive_create(
            (GncKo6mlPrimitiveType)t, name, &s, 0.6 + 0.04 * t,
            0.9 - 0.03 * t);
        g_free(name);
        if (!primitive) {
            check(FALSE, "primitive creation");
            continue;
        }
        gnc_ko6ml_primitive_set_property(primitive, "phase", "1");
        gnc_ko6ml_primitive_set_property(primitive, "note",
                                         "cognitive \"tapestry\"");

        gchar *hg_desc = g_strdup_printf(
            "%s: ko6ml -> hypergraph -> ko6ml preserves integrity",
            type_name);
        check(gnc_ko6ml_verify_hypergraph_roundtrip(primitive), hg_desc);
        g_free(hg_desc);

        gchar *scm_desc = g_strdup_printf(
            "%s: ko6ml -> scheme -> ko6ml preserves integrity", type_name);
        check(gnc_ko6ml_verify_scheme_roundtrip(primitive), scm_desc);
        g_free(scm_desc);

        gnc_ko6ml_primitive_free(primitive);
    }
    printf("\n");

    /* ---- Scheme adapter sample ----------------------------------- */
    printf("📜 Scheme Cognitive Grammar Adapter sample\n");
    GncKo6mlTensorShape agent_shape = { 2, 3, 4, 2, 5 };
    GncKo6mlPrimitive *agent = gnc_ko6ml_primitive_create(
        GNC_KO6ML_AGENT, "bookkeeper", &agent_shape, 0.87, 0.65);
    gnc_ko6ml_primitive_set_property(agent, "role", "double-entry");
    gchar *scheme = gnc_ko6ml_primitive_to_scheme(agent);
    printf("  %s\n\n", scheme);
    GncKo6mlPrimitive *parsed = gnc_ko6ml_primitive_from_scheme(scheme);
    check(parsed != NULL && gnc_ko6ml_primitive_equal(agent, parsed),
          "parsed s-expression is structurally identical");
    g_free(scheme);
    if (parsed)
        gnc_ko6ml_primitive_free(parsed);
    printf("\n");

    /* ---- Hypergraph visualization -------------------------------- */
    printf("🕸  Hypergraph Fragment Flowchart (Graphviz DOT)\n\n");
    GncKo6mlHypergraphFragment *fragment =
        gnc_ko6ml_primitive_to_hypergraph(agent);
    if (fragment) {
        emit_fragment_dot(fragment, "Ko6ml_AGENT_bookkeeper");
        gnc_ko6ml_hypergraph_fragment_free(fragment);
    } else {
        check(FALSE, "hypergraph fragment generation");
    }
    gnc_ko6ml_primitive_free(agent);
    printf("\n");

    /* ---- Performance benchmark ----------------------------------- */
    printf("⚡ Performance Benchmark (real round trips)\n");
    GncKo6mlBenchmarkResult result;
    check(gnc_ko6ml_benchmark_roundtrip(1000, &result),
          "benchmark completed with all round trips intact");
    printf("     %u round trips per direction\n", result.iterations);
    printf("     hypergraph round trip: %8.2f us/op\n",
           result.hypergraph_us_per_op);
    printf("     scheme round trip:     %8.2f us/op\n",
           result.scheme_us_per_op);
    check(result.hypergraph_us_per_op < 1000.0,
          "hypergraph round trip under 1 ms target");
    check(result.scheme_us_per_op < 1000.0,
          "scheme round trip under 1 ms target");
    printf("\n");

    printf("========================================================================\n");
    if (failures == 0) {
        printf("✅ Phase 1 verification pipeline PASSED: all ko6ml primitives\n");
        printf("   translate bidirectionally with full data integrity.\n");
    } else {
        printf("❌ Phase 1 verification pipeline FAILED: %u check(s) failed.\n",
               failures);
    }
    printf("========================================================================\n");

    return failures == 0 ? 0 : 1;
}
