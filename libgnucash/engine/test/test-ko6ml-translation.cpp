/********************************************************************\
 * test-ko6ml-translation.cpp -- Test ko6ml <-> Hypergraph         *
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
 * @file test-ko6ml-translation.cpp
 * @brief Phase 1 verification protocol: exhaustive test patterns for
 *        every ko6ml primitive and transformation.  All round trips
 *        are performed for real -- no mocks, no simulation.
 */

#include <glib.h>
#include <gtest/gtest.h>

#include "gnc-ko6ml-translation.h"
#include "gnc-ko6ml-atomspace.h"
#include "gnc-cognitive-accounting.h"
#include "qof.h"
#include "gnc-engine.h"

/* ==================================================================
 * ko6ml Primitive Vocabulary
 * ================================================================== */

TEST(Ko6mlVocabulary, TypeNamesRoundTripExhaustively)
{
    for (gint t = 0; t < GNC_KO6ML_N_PRIMITIVE_TYPES; t++) {
        const gchar *name =
            gnc_ko6ml_primitive_type_name((GncKo6mlPrimitiveType)t);
        ASSERT_NE(name, nullptr) << "type " << t;

        GncKo6mlPrimitiveType parsed;
        ASSERT_TRUE(gnc_ko6ml_primitive_type_from_name(name, &parsed));
        EXPECT_EQ(parsed, (GncKo6mlPrimitiveType)t);
    }
}

TEST(Ko6mlVocabulary, InvalidTypeNamesRejected)
{
    GncKo6mlPrimitiveType parsed;
    EXPECT_FALSE(gnc_ko6ml_primitive_type_from_name("NOT_A_TYPE", &parsed));
    EXPECT_FALSE(gnc_ko6ml_primitive_type_from_name("agent", &parsed));
    EXPECT_FALSE(gnc_ko6ml_primitive_type_from_name("", &parsed));
    EXPECT_EQ(gnc_ko6ml_primitive_type_name(GNC_KO6ML_N_PRIMITIVE_TYPES),
              nullptr);
}

/* ==================================================================
 * Tensor Fragment Architecture
 * ================================================================== */

TEST(Ko6mlTensorShape, ValidShapesAccepted)
{
    GncKo6mlTensorShape minimal = { 1, 1, 1, 1, 1 };
    GncKo6mlTensorShape maximal = {
        GNC_KO6ML_MAX_MODALITY, GNC_KO6ML_MAX_DEPTH, GNC_KO6ML_MAX_CONTEXT,
        GNC_KO6ML_MAX_SALIENCE, GNC_KO6ML_MAX_AUTONOMY_INDEX };

    EXPECT_TRUE(gnc_ko6ml_tensor_shape_validate(&minimal, NULL));
    EXPECT_TRUE(gnc_ko6ml_tensor_shape_validate(&maximal, NULL));
}

TEST(Ko6mlTensorShape, EachDimensionBoundEnforced)
{
    const GncKo6mlTensorShape base = { 2, 3, 4, 2, 3 };

    struct { guint GncKo6mlTensorShape::*member; guint max; } dims[] = {
        { &GncKo6mlTensorShape::modality, GNC_KO6ML_MAX_MODALITY },
        { &GncKo6mlTensorShape::depth, GNC_KO6ML_MAX_DEPTH },
        { &GncKo6mlTensorShape::context, GNC_KO6ML_MAX_CONTEXT },
        { &GncKo6mlTensorShape::salience, GNC_KO6ML_MAX_SALIENCE },
        { &GncKo6mlTensorShape::autonomy_index,
          GNC_KO6ML_MAX_AUTONOMY_INDEX }
    };

    for (auto &dim : dims) {
        GncKo6mlTensorShape shape = base;
        shape.*(dim.member) = 0;
        gchar *error_msg = NULL;
        EXPECT_FALSE(gnc_ko6ml_tensor_shape_validate(&shape, &error_msg));
        ASSERT_NE(error_msg, nullptr);
        g_free(error_msg);

        shape.*(dim.member) = dim.max + 1;
        EXPECT_FALSE(gnc_ko6ml_tensor_shape_validate(&shape, NULL));
    }
}

TEST(Ko6mlTensorSignature, KnownValue)
{
    /* 2^3 * 3^4 * 5^5 * 7^2 * 11^6 = 175783140225000 */
    GncKo6mlTensorShape shape = { 3, 4, 5, 2, 6 };
    GncKo6mlTensorSignature signature;
    ASSERT_TRUE(gnc_ko6ml_tensor_shape_signature(&shape, &signature));
    EXPECT_EQ(signature.hi, 0u);
    EXPECT_EQ(signature.lo, 175783140225000u);

    gchar *str = gnc_ko6ml_tensor_signature_to_string(&signature);
    EXPECT_STREQ(str, "175783140225000");
    g_free(str);
}

TEST(Ko6mlTensorSignature, InversePrimeFactorizationRoundTrip)
{
    /* Exhaustive corner sweep plus interior samples of the shape
     * lattice -- every signature must factorize back exactly */
    const guint modalities[] = { 1, 4, GNC_KO6ML_MAX_MODALITY };
    const guint depths[] = { 1, 8, GNC_KO6ML_MAX_DEPTH };
    const guint contexts[] = { 1, 8, GNC_KO6ML_MAX_CONTEXT };
    const guint saliences[] = { 1, 4, GNC_KO6ML_MAX_SALIENCE };
    const guint autonomies[] = { 1, 4, GNC_KO6ML_MAX_AUTONOMY_INDEX };

    for (guint m : modalities)
        for (guint d : depths)
            for (guint c : contexts)
                for (guint s : saliences)
                    for (guint a : autonomies) {
                        GncKo6mlTensorShape shape = { m, d, c, s, a };
                        GncKo6mlTensorSignature signature;
                        ASSERT_TRUE(gnc_ko6ml_tensor_shape_signature(
                            &shape, &signature));

                        GncKo6mlTensorShape back;
                        ASSERT_TRUE(gnc_ko6ml_tensor_shape_from_signature(
                            &signature, &back));
                        EXPECT_EQ(back.modality, m);
                        EXPECT_EQ(back.depth, d);
                        EXPECT_EQ(back.context, c);
                        EXPECT_EQ(back.salience, s);
                        EXPECT_EQ(back.autonomy_index, a);
                    }
}

TEST(Ko6mlTensorSignature, NonCanonicalSignaturesRejected)
{
    GncKo6mlTensorShape shape;

    /* 13 is outside the prime basis {2,3,5,7,11} */
    GncKo6mlTensorSignature alien = { 0, 13 * 2 * 3 * 5 * 7 * 11 };
    EXPECT_FALSE(gnc_ko6ml_tensor_shape_from_signature(&alien, &shape));

    /* 0 and 1 encode no valid shape (every dimension must be >= 1) */
    GncKo6mlTensorSignature zero = { 0, 0 };
    GncKo6mlTensorSignature one = { 0, 1 };
    EXPECT_FALSE(gnc_ko6ml_tensor_shape_from_signature(&zero, &shape));
    EXPECT_FALSE(gnc_ko6ml_tensor_shape_from_signature(&one, &shape));

    /* 2^9 exceeds the modality bound of 8; 3,5,7,11 present once */
    GncKo6mlTensorSignature oversized = { 0, 0 };
    guint64 value = 3ull * 5 * 7 * 11;
    for (int i = 0; i < GNC_KO6ML_MAX_MODALITY + 1; i++)
        value *= 2;
    oversized.lo = value;
    EXPECT_FALSE(gnc_ko6ml_tensor_shape_from_signature(&oversized, &shape));
}

TEST(Ko6mlTensorFragment, ElementAccessAndBounds)
{
    GncKo6mlTensorShape shape = { 2, 3, 2, 2, 2 };
    GncKo6mlTensorFragment *fragment =
        gnc_ko6ml_tensor_fragment_create(&shape);
    ASSERT_NE(fragment, nullptr);
    EXPECT_EQ(fragment->element_count, (gsize)(2 * 3 * 2 * 2 * 2));

    /* Write a distinct value to every element and read it back */
    gdouble counter = 0.0;
    for (guint m = 0; m < 2; m++)
        for (guint d = 0; d < 3; d++)
            for (guint c = 0; c < 2; c++)
                for (guint s = 0; s < 2; s++)
                    for (guint a = 0; a < 2; a++)
                        ASSERT_TRUE(gnc_ko6ml_tensor_fragment_set(
                            fragment, m, d, c, s, a, counter++));

    counter = 0.0;
    for (guint m = 0; m < 2; m++)
        for (guint d = 0; d < 3; d++)
            for (guint c = 0; c < 2; c++)
                for (guint s = 0; s < 2; s++)
                    for (guint a = 0; a < 2; a++) {
                        gdouble value = -1.0;
                        ASSERT_TRUE(gnc_ko6ml_tensor_fragment_get(
                            fragment, m, d, c, s, a, &value));
                        EXPECT_DOUBLE_EQ(value, counter++);
                    }

    /* Out-of-range indices must be rejected per dimension */
    gdouble value;
    EXPECT_FALSE(gnc_ko6ml_tensor_fragment_set(fragment, 2, 0, 0, 0, 0, 1.0));
    EXPECT_FALSE(gnc_ko6ml_tensor_fragment_set(fragment, 0, 3, 0, 0, 0, 1.0));
    EXPECT_FALSE(gnc_ko6ml_tensor_fragment_get(fragment, 0, 0, 2, 0, 0, &value));
    EXPECT_FALSE(gnc_ko6ml_tensor_fragment_get(fragment, 0, 0, 0, 2, 0, &value));
    EXPECT_FALSE(gnc_ko6ml_tensor_fragment_get(fragment, 0, 0, 0, 0, 2, &value));

    gnc_ko6ml_tensor_fragment_free(fragment);
}

TEST(Ko6mlTensorFragment, InvalidShapeRejected)
{
    GncKo6mlTensorShape invalid = { 0, 1, 1, 1, 1 };
    EXPECT_EQ(gnc_ko6ml_tensor_fragment_create(&invalid), nullptr);
}

/* ==================================================================
 * ko6ml Primitive Instances
 * ================================================================== */

static GncKo6mlPrimitive* make_test_primitive(GncKo6mlPrimitiveType type)
{
    GncKo6mlTensorShape shape = { 2, 5, 3, 4, 2 };
    gchar *name = g_strdup_printf("test-%s",
        gnc_ko6ml_primitive_type_name(type));
    GncKo6mlPrimitive *primitive = gnc_ko6ml_primitive_create(
        type, name, &shape, 0.75, 0.6);
    g_free(name);
    return primitive;
}

TEST(Ko6mlPrimitive, CreationValidation)
{
    GncKo6mlTensorShape shape = { 1, 2, 3, 4, 5 };

    EXPECT_EQ(gnc_ko6ml_primitive_create(GNC_KO6ML_AGENT, "", &shape,
                                         0.5, 0.5), nullptr);
    EXPECT_EQ(gnc_ko6ml_primitive_create(GNC_KO6ML_AGENT, "x", &shape,
                                         1.5, 0.5), nullptr);
    EXPECT_EQ(gnc_ko6ml_primitive_create(GNC_KO6ML_AGENT, "x", &shape,
                                         0.5, -0.1), nullptr);
    EXPECT_EQ(gnc_ko6ml_primitive_create(GNC_KO6ML_N_PRIMITIVE_TYPES, "x",
                                         &shape, 0.5, 0.5), nullptr);

    GncKo6mlTensorShape bad_shape = { 0, 2, 3, 4, 5 };
    EXPECT_EQ(gnc_ko6ml_primitive_create(GNC_KO6ML_AGENT, "x", &bad_shape,
                                         0.5, 0.5), nullptr);

    GncKo6mlPrimitive *primitive = gnc_ko6ml_primitive_create(
        GNC_KO6ML_AGENT, "x", &shape, 0.5, 0.5);
    ASSERT_NE(primitive, nullptr);
    gnc_ko6ml_primitive_free(primitive);
}

TEST(Ko6mlPrimitive, PropertiesAndEquality)
{
    GncKo6mlPrimitive *a = make_test_primitive(GNC_KO6ML_GOAL);
    GncKo6mlPrimitive *b = make_test_primitive(GNC_KO6ML_GOAL);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_TRUE(gnc_ko6ml_primitive_equal(a, b));

    gnc_ko6ml_primitive_set_property(a, "target", "solvency");
    EXPECT_FALSE(gnc_ko6ml_primitive_equal(a, b));
    EXPECT_STREQ(gnc_ko6ml_primitive_get_property(a, "target"), "solvency");
    EXPECT_EQ(gnc_ko6ml_primitive_get_property(a, "missing"), nullptr);

    gnc_ko6ml_primitive_set_property(b, "target", "solvency");
    EXPECT_TRUE(gnc_ko6ml_primitive_equal(a, b));

    /* Replacing a property value must be reflected in equality */
    gnc_ko6ml_primitive_set_property(b, "target", "liquidity");
    EXPECT_FALSE(gnc_ko6ml_primitive_equal(a, b));

    gnc_ko6ml_primitive_free(a);
    gnc_ko6ml_primitive_free(b);
}

/* ==================================================================
 * Hypergraph Fragment Translation (bidirectional, no mocks)
 * ================================================================== */

TEST(Ko6mlHypergraph, RoundTripExhaustiveOverAllPrimitiveTypes)
{
    for (gint t = 0; t < GNC_KO6ML_N_PRIMITIVE_TYPES; t++) {
        GncKo6mlPrimitive *primitive =
            make_test_primitive((GncKo6mlPrimitiveType)t);
        ASSERT_NE(primitive, nullptr);
        gnc_ko6ml_primitive_set_property(primitive, "phase", "1");
        gnc_ko6ml_primitive_set_property(primitive, "network",
                                         "agentic-grammar");

        EXPECT_TRUE(gnc_ko6ml_verify_hypergraph_roundtrip(primitive))
            << "type " << t;
        gnc_ko6ml_primitive_free(primitive);
    }
}

TEST(Ko6mlHypergraph, FragmentStructureMatchesEncodingContract)
{
    GncKo6mlPrimitive *primitive = make_test_primitive(GNC_KO6ML_AGENT);
    ASSERT_NE(primitive, nullptr);
    gnc_ko6ml_primitive_set_property(primitive, "role", "ledger");

    GncKo6mlHypergraphFragment *fragment =
        gnc_ko6ml_primitive_to_hypergraph(primitive);
    ASSERT_NE(fragment, nullptr);

    /* Contract: root concept, type concept, inheritance link,
     * shape predicate, shape number, shape evaluation, plus
     * (predicate, value, evaluation) per property = 6 + 3 * 1 */
    EXPECT_EQ(fragment->atoms->len, 9u);

    GncKo6mlHypergraphAtom *root =
        (GncKo6mlHypergraphAtom*)g_ptr_array_index(fragment->atoms, 0);
    EXPECT_EQ(root->atom_type, GNC_KO6ML_NODE_CONCEPT);
    EXPECT_STREQ(root->name, "Ko6ml:AGENT:test-AGENT");
    EXPECT_DOUBLE_EQ(root->strength, 0.75);
    EXPECT_DOUBLE_EQ(root->confidence, 0.6);

    gnc_ko6ml_hypergraph_fragment_free(fragment);
    gnc_ko6ml_primitive_free(primitive);
}

TEST(Ko6mlHypergraph, RoundTripPreservesEdgeCaseNames)
{
    GncKo6mlTensorShape shape = { 1, 1, 1, 1, 1 };

    /* Names containing colons -- the root-name separator -- and
     * property values that resemble encoding markers */
    GncKo6mlPrimitive *primitive = gnc_ko6ml_primitive_create(
        GNC_KO6ML_RELATION, "ns:sub:entity:42", &shape, 0.0, 1.0);
    ASSERT_NE(primitive, nullptr);
    gnc_ko6ml_primitive_set_property(primitive, "note", "Ko6mlType:AGENT");
    gnc_ko6ml_primitive_set_property(primitive, "empty", "");

    EXPECT_TRUE(gnc_ko6ml_verify_hypergraph_roundtrip(primitive));
    gnc_ko6ml_primitive_free(primitive);
}

TEST(Ko6mlHypergraph, InvalidFragmentsRejected)
{
    /* An empty fragment has no root concept */
    GncKo6mlHypergraphFragment *empty =
        g_new0(GncKo6mlHypergraphFragment, 1);
    empty->atoms = g_ptr_array_new_with_free_func(NULL);
    EXPECT_EQ(gnc_ko6ml_hypergraph_to_primitive(empty), nullptr);
    g_ptr_array_free(empty->atoms, TRUE);
    g_free(empty);
}

/* ==================================================================
 * Scheme Cognitive Grammar Adapter (bidirectional, no mocks)
 * ================================================================== */

TEST(Ko6mlScheme, RoundTripExhaustiveOverAllPrimitiveTypes)
{
    for (gint t = 0; t < GNC_KO6ML_N_PRIMITIVE_TYPES; t++) {
        GncKo6mlPrimitive *primitive =
            make_test_primitive((GncKo6mlPrimitiveType)t);
        ASSERT_NE(primitive, nullptr);
        gnc_ko6ml_primitive_set_property(primitive, "phase", "1");

        EXPECT_TRUE(gnc_ko6ml_verify_scheme_roundtrip(primitive))
            << "type " << t;
        gnc_ko6ml_primitive_free(primitive);
    }
}

TEST(Ko6mlScheme, EncodingIsWellFormedSExpression)
{
    GncKo6mlPrimitive *primitive = make_test_primitive(GNC_KO6ML_PERCEPT);
    ASSERT_NE(primitive, nullptr);

    gchar *expr = gnc_ko6ml_primitive_to_scheme(primitive);
    ASSERT_NE(expr, nullptr);
    EXPECT_TRUE(g_str_has_prefix(expr, "(Ko6mlPrimitive"));
    EXPECT_NE(strstr(expr, "(type \"PERCEPT\")"), nullptr);
    EXPECT_NE(strstr(expr, "(modality 2)"), nullptr);
    EXPECT_NE(strstr(expr, "(autonomy-index 2)"), nullptr);

    g_free(expr);
    gnc_ko6ml_primitive_free(primitive);
}

TEST(Ko6mlScheme, RoundTripPreservesQuotesBackslashesAndParens)
{
    GncKo6mlTensorShape shape = { 3, 2, 4, 1, 2 };
    GncKo6mlPrimitive *primitive = gnc_ko6ml_primitive_create(
        GNC_KO6ML_MEMORY, "memo \"quoted\" (parens) \\slash", &shape,
        0.123456789, 0.987654321);
    ASSERT_NE(primitive, nullptr);
    gnc_ko6ml_primitive_set_property(primitive, "expr",
                                     "(lambda (x) \"y\\z\")");

    EXPECT_TRUE(gnc_ko6ml_verify_scheme_roundtrip(primitive));
    gnc_ko6ml_primitive_free(primitive);
}

TEST(Ko6mlScheme, MalformedExpressionsRejected)
{
    EXPECT_EQ(gnc_ko6ml_primitive_from_scheme(""), nullptr);
    EXPECT_EQ(gnc_ko6ml_primitive_from_scheme("garbage"), nullptr);
    EXPECT_EQ(gnc_ko6ml_primitive_from_scheme("(WrongHead (type \"AGENT\"))"),
              nullptr);
    EXPECT_EQ(gnc_ko6ml_primitive_from_scheme("(Ko6mlPrimitive"), nullptr);
    EXPECT_EQ(gnc_ko6ml_primitive_from_scheme(
        "(Ko6mlPrimitive (type \"AGENT\") (name \"x\"))"), nullptr);
    /* Invalid tensor shape must be rejected by shape validation */
    EXPECT_EQ(gnc_ko6ml_primitive_from_scheme(
        "(Ko6mlPrimitive (type \"AGENT\") (name \"x\")"
        " (tensor-shape (modality 0) (depth 1) (context 1)"
        " (salience 1) (autonomy-index 1))"
        " (truth (strength 0.5) (confidence 0.5)) (properties))"),
        nullptr);
}

/* ==================================================================
 * AtomSpace Instantiation (real atoms in the cognitive AtomSpace)
 * ================================================================== */

class Ko6mlAtomSpaceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        qof_init();
        gnc_cognitive_accounting_init();
    }

    void TearDown() override
    {
        gnc_cognitive_accounting_shutdown();
        qof_close();
    }
};

TEST_F(Ko6mlAtomSpaceTest, PrimitiveInstantiatesAsRealAtoms)
{
    GncKo6mlPrimitive *primitive = make_test_primitive(GNC_KO6ML_STATE);
    ASSERT_NE(primitive, nullptr);
    gnc_ko6ml_primitive_set_property(primitive, "epoch", "genesis");

    GncKo6mlHypergraphFragment *fragment =
        gnc_ko6ml_primitive_to_hypergraph(primitive);
    ASSERT_NE(fragment, nullptr);

    GncAtomHandle *handles = NULL;
    guint handle_count = 0;
    GncAtomHandle root = gnc_ko6ml_fragment_to_atomspace(
        fragment, &handles, &handle_count);

    ASSERT_NE(root, 0u);
    ASSERT_EQ(handle_count, fragment->atoms->len);
    for (guint i = 0; i < handle_count; i++)
        EXPECT_NE(handles[i], 0u) << "atom " << i;

    /* The root atom must carry the primitive's truth value */
    gdouble strength = 0.0, confidence = 0.0;
    ASSERT_TRUE(gnc_atomspace_get_truth_value(root, &strength, &confidence));
    EXPECT_NEAR(strength, 0.75, 1e-9);
    EXPECT_NEAR(confidence, 0.6, 1e-9);

    g_free(handles);
    gnc_ko6ml_hypergraph_fragment_free(fragment);
    gnc_ko6ml_primitive_free(primitive);
}

TEST_F(Ko6mlAtomSpaceTest, ConvenienceWrapperCreatesRoot)
{
    GncKo6mlPrimitive *primitive = make_test_primitive(GNC_KO6ML_ACTION);
    ASSERT_NE(primitive, nullptr);

    GncAtomHandle root = gnc_ko6ml_primitive_to_atomspace(primitive);
    EXPECT_NE(root, 0u);

    gnc_ko6ml_primitive_free(primitive);
}

/* ==================================================================
 * Verification Protocol: benchmark with real translations
 * ================================================================== */

TEST(Ko6mlBenchmark, RoundTripBenchmarkRunsCleanly)
{
    GncKo6mlBenchmarkResult result;
    ASSERT_TRUE(gnc_ko6ml_benchmark_roundtrip(50, &result));

    EXPECT_TRUE(result.all_roundtrips_ok);
    EXPECT_EQ(result.iterations, 50u * GNC_KO6ML_N_PRIMITIVE_TYPES);
    EXPECT_GT(result.hypergraph_us_per_op, 0.0);
    EXPECT_GT(result.scheme_us_per_op, 0.0);

    /* Performance target: each real round trip completes within
     * 1 millisecond on commodity hardware */
    EXPECT_LT(result.hypergraph_us_per_op, 1000.0);
    EXPECT_LT(result.scheme_us_per_op, 1000.0);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
