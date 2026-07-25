/********************************************************************\
 * test-cognitive-fincosys-loader.cpp -- Test the cognitive_atoms.json*
 *                                        loader                       *
 * Copyright (C) 2026 GnuCash Cognitive Engine                        *
 *                                                                    *
 * This program is free software; you can redistribute it and/or      *
 * modify it under the terms of the GNU General Public License as     *
 * published by the Free Software Foundation; either version 2 of     *
 * the License, or (at your option) any later version.                *
 *                                                                    *
 * This program is distributed in the hope that it will be useful,    *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      *
 * GNU General Public License for more details.                       *
 ********************************************************************/

#include <glib.h>
#include <gtest/gtest.h>
#include <cmath>
#include <string>

#include "gnc-cognitive-accounting.h"
#include "gnc-cognitive-fincosys-loader.h"
#include "qof.h"

class CognitiveFincosysLoaderTest : public ::testing::Test
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

TEST_F(CognitiveFincosysLoaderTest, NullJsonReturnsFalse)
{
    GncCognitiveFincosysLoadResult result;
    EXPECT_FALSE(gnc_cognitive_load_fincosys_atoms(nullptr, &result));
}

TEST_F(CognitiveFincosysLoaderTest, InvalidJsonReturnsFalse)
{
    GncCognitiveFincosysLoadResult result;
    EXPECT_FALSE(gnc_cognitive_load_fincosys_atoms("not json", &result));
}

TEST_F(CognitiveFincosysLoaderTest, UninitializedAtomSpaceReturnsFalse)
{
    gnc_cognitive_accounting_shutdown();
    GncCognitiveFincosysLoadResult result;
    EXPECT_FALSE(gnc_cognitive_load_fincosys_atoms("{}", &result));
    gnc_cognitive_accounting_init();
}

TEST_F(CognitiveFincosysLoaderTest, EmptyDocumentSucceedsWithZeroCounts)
{
    GncCognitiveFincosysLoadResult result;
    EXPECT_TRUE(gnc_cognitive_load_fincosys_atoms("{\"schema_version\": \"1.0\"}", &result));
    EXPECT_EQ(0U, result.atoms_created);
    EXPECT_EQ(0U, result.links_created);
    EXPECT_EQ(0U, result.evaluations_created);
}

TEST_F(CognitiveFincosysLoaderTest, ResultParameterIsOptional)
{
    /* NULL result must not crash. */
    EXPECT_TRUE(gnc_cognitive_load_fincosys_atoms("{}", nullptr));
}

TEST_F(CognitiveFincosysLoaderTest, LoadsConceptNodeWithAttributes)
{
    const gchar* json = R"JSON(
    {
      "schema_version": "1.0",
      "atoms": [
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "entity:RST",
         "label": "RegimA Skin Treatments CC",
         "attributes": {"entity_code": "RST"}}
      ]
    }
    )JSON";

    GncCognitiveFincosysLoadResult result;
    EXPECT_TRUE(gnc_cognitive_load_fincosys_atoms(json, &result));
    EXPECT_EQ(1U, result.atoms_created);
    EXPECT_EQ(0U, result.atoms_skipped);
}

TEST_F(CognitiveFincosysLoaderTest, ConceptNodeMissingIdIsSkipped)
{
    const gchar* json = R"JSON(
    {"atoms": [{"atom_type": "GNC_ATOM_CONCEPT_NODE", "label": "no id here"}]}
    )JSON";

    GncCognitiveFincosysLoadResult result;
    EXPECT_TRUE(gnc_cognitive_load_fincosys_atoms(json, &result));
    EXPECT_EQ(0U, result.atoms_created);
    EXPECT_EQ(1U, result.atoms_skipped);
}

TEST_F(CognitiveFincosysLoaderTest, UnsupportedAtomTypeIsSkipped)
{
    const gchar* json = R"JSON(
    {"atoms": [{"atom_type": "GNC_ATOM_SCHEMA_NODE", "id": "x", "label": "x"}]}
    )JSON";

    GncCognitiveFincosysLoadResult result;
    EXPECT_TRUE(gnc_cognitive_load_fincosys_atoms(json, &result));
    EXPECT_EQ(0U, result.atoms_created);
    EXPECT_EQ(1U, result.atoms_skipped);
}

TEST_F(CognitiveFincosysLoaderTest, LoadsInheritanceLinkBetweenPriorAtoms)
{
    const gchar* json = R"JSON(
    {
      "atoms": [
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "entity:RST", "label": "RST"},
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "account:BANK-111", "label": "RST Current Account"},
        {"atom_type": "GNC_ATOM_INHERITANCE_LINK", "id": "inherit:account:BANK-111->entity:RST",
         "source": "account:BANK-111", "target": "entity:RST"}
      ]
    }
    )JSON";

    GncCognitiveFincosysLoadResult result;
    EXPECT_TRUE(gnc_cognitive_load_fincosys_atoms(json, &result));
    EXPECT_EQ(2U, result.atoms_created);
    EXPECT_EQ(1U, result.links_created);
    EXPECT_EQ(0U, result.links_skipped);
}

TEST_F(CognitiveFincosysLoaderTest, InheritanceLinkWithUnresolvedSourceIsSkippedNotCrashed)
{
    const gchar* json = R"JSON(
    {
      "atoms": [
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "entity:RST", "label": "RST"},
        {"atom_type": "GNC_ATOM_INHERITANCE_LINK", "id": "bad-link",
         "source": "account:DOES-NOT-EXIST", "target": "entity:RST"}
      ]
    }
    )JSON";

    GncCognitiveFincosysLoadResult result;
    EXPECT_TRUE(gnc_cognitive_load_fincosys_atoms(json, &result));
    EXPECT_EQ(1U, result.atoms_created);
    EXPECT_EQ(0U, result.links_created);
    EXPECT_EQ(1U, result.links_skipped);
}

/* This fixture matches the real output of
 * bindings/python/example_scripts/fincosys_sync/cognitive_bridge.py's
 * build_cognitive_atoms_doc(), captured by running it against a plan built
 * from the same fixture data as
 * bindings/python/example_scripts/fincosys_sync/tests/test_cognitive_bridge.py's
 * make_plan() (one entity, three accounts, a balanced/verified transaction
 * and an unbalanced/flagged one) -- i.e. this is not a hand-invented shape,
 * it is what the Python bridge actually emits for that input. */
TEST_F(CognitiveFincosysLoaderTest, LoadsRealCognitiveBridgeOutputEndToEnd)
{
    const gchar* json = R"JSON(
    {
      "schema_version": "1.0",
      "source_plan": "test-source",
      "atoms": [
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "entity:RST",
         "label": "RegimA Skin Treatments CC", "attributes": {"entity_code": "RST"}},
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "account:ENTITY-RST",
         "label": "RegimA Skin Treatments CC",
         "attributes": {"account_type": "ASSET", "currency": "ZAR", "parent_code": null}},
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "account:BANK-111",
         "label": "RST Current Account",
         "attributes": {"account_type": "BANK", "currency": "ZAR", "parent_code": "ENTITY-RST"}},
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "account:IMBALANCE-RST-PAYMENT",
         "label": "Imbalance-PAYMENT",
         "attributes": {"account_type": "BANK", "currency": "ZAR", "parent_code": "ENTITY-RST"}},
        {"atom_type": "GNC_ATOM_INHERITANCE_LINK", "id": "inherit:account:BANK-111->entity:RST",
         "source": "account:BANK-111", "target": "entity:RST"},
        {"atom_type": "GNC_ATOM_INHERITANCE_LINK",
         "id": "inherit:account:BANK-111->account:ENTITY-RST",
         "source": "account:BANK-111", "target": "account:ENTITY-RST"},
        {"atom_type": "GNC_ATOM_INHERITANCE_LINK",
         "id": "inherit:account:IMBALANCE-RST-PAYMENT->entity:RST",
         "source": "account:IMBALANCE-RST-PAYMENT", "target": "entity:RST"},
        {"atom_type": "GNC_ATOM_INHERITANCE_LINK",
         "id": "inherit:account:IMBALANCE-RST-PAYMENT->account:ENTITY-RST",
         "source": "account:IMBALANCE-RST-PAYMENT", "target": "account:ENTITY-RST"}
      ],
      "evaluations": [
        {"atom_type": "GNC_ATOM_EVALUATION_LINK", "id": "tx:tx-1",
         "predicate": "balanced_transaction",
         "participants": ["account:BANK-111", "account:IMBALANCE-RST-PAYMENT"],
         "truth_value": {"strength": 1.0, "confidence": 0.9}},
        {"atom_type": "GNC_ATOM_EVALUATION_LINK", "id": "tx:tx-2",
         "predicate": "balanced_transaction",
         "participants": ["account:BANK-111", "account:IMBALANCE-RST-PAYMENT"],
         "truth_value": {"strength": 0.571429, "confidence": 0.4}}
      ]
    }
    )JSON";

    GncCognitiveFincosysLoadResult result;
    ASSERT_TRUE(gnc_cognitive_load_fincosys_atoms(json, &result));

    EXPECT_EQ(4U, result.atoms_created);
    EXPECT_EQ(0U, result.atoms_skipped);
    EXPECT_EQ(4U, result.links_created);
    EXPECT_EQ(0U, result.links_skipped);
    /* 2 evaluation entries x 2 participants each = 4 EvaluationLink atoms,
     * since gnc_atomspace_create_evaluation_link() only takes one account
     * per call (see the header comment on gnc_cognitive_load_fincosys_atoms()). */
    EXPECT_EQ(4U, result.evaluations_created);
    EXPECT_EQ(0U, result.evaluations_skipped);
}

TEST_F(CognitiveFincosysLoaderTest, EvaluationLinkAppliesJsonTruthValueNotHardcodedOne)
{
    const gchar* json = R"JSON(
    {
      "atoms": [
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "account:A", "label": "A"},
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "account:B", "label": "B"}
      ],
      "evaluations": [
        {"atom_type": "GNC_ATOM_EVALUATION_LINK", "id": "tx:tx-1",
         "predicate": "balanced_transaction",
         "participants": ["account:A", "account:B"],
         "truth_value": {"strength": 0.571429, "confidence": 0.4}}
      ]
    }
    )JSON";

    GncCognitiveFincosysLoadResult result;
    ASSERT_TRUE(gnc_cognitive_load_fincosys_atoms(json, &result));
    ASSERT_EQ(2U, result.evaluations_created);

    /* gnc_atomspace_create_evaluation_link() hard-codes confidence 0.9
     * internally; the loader must overwrite it with the document's own
     * 0.4 via gnc_atomspace_set_truth_value(). Walk every atom and check
     * that at least one EvaluationLink carries the document's truth value
     * rather than the hard-coded one. */
    bool found_correct_truth_value = false;
    gnc_atomspace_foreach_atom(
        [](GncAtomHandle handle, GncAtomType type, const char* name, gdouble strength,
           gdouble confidence, gpointer user_data)
        {
            if (type != GNC_ATOM_EVALUATION_LINK)
                return;
            auto* found = static_cast<bool*>(user_data);
            if (std::abs(strength - 0.571429) < 1e-5 && std::abs(confidence - 0.4) < 1e-9)
                *found = true;
        },
        &found_correct_truth_value);

    EXPECT_TRUE(found_correct_truth_value);
}

TEST_F(CognitiveFincosysLoaderTest, EvaluationSharesOnePredicateNodeAcrossEntries)
{
    const gchar* json = R"JSON(
    {
      "atoms": [
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "account:A", "label": "A"},
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "account:B", "label": "B"}
      ],
      "evaluations": [
        {"atom_type": "GNC_ATOM_EVALUATION_LINK", "id": "tx:tx-1",
         "predicate": "balanced_transaction", "participants": ["account:A"],
         "truth_value": {"strength": 1.0, "confidence": 0.9}},
        {"atom_type": "GNC_ATOM_EVALUATION_LINK", "id": "tx:tx-2",
         "predicate": "balanced_transaction", "participants": ["account:B"],
         "truth_value": {"strength": 1.0, "confidence": 0.9}}
      ]
    }
    )JSON";

    GncCognitiveFincosysLoadResult result;
    ASSERT_TRUE(gnc_cognitive_load_fincosys_atoms(json, &result));

    /* 2 concept nodes + 1 shared PredicateNode + 2 EvaluationLinks = 5
     * atoms total, i.e. only one PredicateNode was created for the two
     * entries sharing the same predicate name. */
    int predicate_node_count = 0;
    gnc_atomspace_foreach_atom(
        [](GncAtomHandle, GncAtomType type, const char*, gdouble, gdouble, gpointer user_data)
        {
            if (type == GNC_ATOM_PREDICATE_NODE)
                ++*static_cast<int*>(user_data);
        },
        &predicate_node_count);
    EXPECT_EQ(1, predicate_node_count);
}

TEST_F(CognitiveFincosysLoaderTest, EvaluationWithUnresolvedParticipantIsSkippedNotCrashed)
{
    const gchar* json = R"JSON(
    {
      "atoms": [
        {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "account:A", "label": "A"}
      ],
      "evaluations": [
        {"atom_type": "GNC_ATOM_EVALUATION_LINK", "id": "tx:tx-1",
         "predicate": "balanced_transaction",
         "participants": ["account:DOES-NOT-EXIST"],
         "truth_value": {"strength": 1.0, "confidence": 0.9}}
      ]
    }
    )JSON";

    GncCognitiveFincosysLoadResult result;
    ASSERT_TRUE(gnc_cognitive_load_fincosys_atoms(json, &result));
    EXPECT_EQ(0U, result.evaluations_created);
    EXPECT_EQ(1U, result.evaluations_skipped);
}

TEST_F(CognitiveFincosysLoaderTest, AttributeGetterReturnsNullForUnknownHandleOrKey)
{
    EXPECT_EQ(nullptr, gnc_cognitive_fincosys_loader_get_atom_attribute(0, "entity_code"));

    const gchar* json = R"JSON(
    {"atoms": [{"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "entity:RST", "label": "RST",
                "attributes": {"entity_code": "RST"}}]}
    )JSON";
    GncCognitiveFincosysLoadResult result;
    ASSERT_TRUE(gnc_cognitive_load_fincosys_atoms(json, &result));
    ASSERT_EQ(1U, result.atoms_created);

    /* Find the handle that was created (walk the AtomSpace since the test
     * doesn't have it returned directly). */
    GncAtomHandle rst_handle = 0;
    gnc_atomspace_foreach_atom(
        [](GncAtomHandle handle, GncAtomType type, const char* name, gdouble, gdouble,
           gpointer user_data)
        {
            if (type == GNC_ATOM_CONCEPT_NODE && name != nullptr &&
                std::string(name) == "RST")
                *static_cast<GncAtomHandle*>(user_data) = handle;
        },
        &rst_handle);
    ASSERT_NE(0U, rst_handle);

    EXPECT_STREQ("RST", gnc_cognitive_fincosys_loader_get_atom_attribute(rst_handle, "entity_code"));
    EXPECT_EQ(nullptr, gnc_cognitive_fincosys_loader_get_atom_attribute(rst_handle, "no_such_key"));
}
