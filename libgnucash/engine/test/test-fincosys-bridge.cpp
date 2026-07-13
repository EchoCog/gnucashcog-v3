/********************************************************************\
 * test-fincosys-bridge.cpp -- Test the fincosys ecosystem sync      *
 *                             bridge                                 *
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
#include <string>

#include "gnc-cognitive-accounting.h"
#include "gnc-fincosys-bridge.h"
#include "qof.h"

class FincosysBridgeTest : public ::testing::Test
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

TEST_F(FincosysBridgeTest, ExportBeforeInitReturnsNull)
{
    gnc_cognitive_accounting_shutdown();
    gchar* json = gnc_cognitive_export_fincosys_json();
    EXPECT_EQ(nullptr, json);
    gnc_cognitive_accounting_init();
}

TEST_F(FincosysBridgeTest, ExportHasSchemaHeader)
{
    gchar* json = gnc_cognitive_export_fincosys_json();
    ASSERT_NE(nullptr, json);

    std::string text(json);
    EXPECT_NE(std::string::npos,
              text.find("\"schema\": \"fincosys-ecosystem-sync/v1\""));
    EXPECT_NE(std::string::npos, text.find("\"source\": \"gnucashcog-v3\""));

    g_free(json);
}

TEST_F(FincosysBridgeTest, ExportsConceptNodeAsAtom)
{
    GncAtomHandle concept_atom = gnc_atomspace_create_concept_node("RST:Bank");
    ASSERT_NE(0U, concept_atom);

    gchar* json = gnc_cognitive_export_fincosys_json();
    ASSERT_NE(nullptr, json);

    std::string text(json);
    std::string id_field = "\"id\": \"" + std::to_string(concept_atom) + "\"";
    EXPECT_NE(std::string::npos, text.find(id_field));
    EXPECT_NE(std::string::npos, text.find("\"atom_type\": \"ConceptNode\""));
    EXPECT_NE(std::string::npos, text.find("\"label\": \"RST:Bank\""));

    g_free(json);
}

TEST_F(FincosysBridgeTest, ExportsInheritanceLinkWithParticipants)
{
    GncAtomHandle child = gnc_atomspace_create_concept_node("Bank");
    GncAtomHandle parent = gnc_atomspace_create_concept_node("Asset");
    GncAtomHandle link = gnc_atomspace_create_inheritance_link(child, parent);
    ASSERT_NE(0U, link);

    gchar* json = gnc_cognitive_export_fincosys_json();
    ASSERT_NE(nullptr, json);

    std::string text(json);
    std::string link_id_field = "\"id\": \"" + std::to_string(link) + "\"";
    EXPECT_NE(std::string::npos, text.find(link_id_field));
    EXPECT_NE(std::string::npos, text.find("\"link_type\": \"InheritanceLink\""));
    EXPECT_NE(std::string::npos,
              text.find("\"atoms\": [\"" + std::to_string(child) + "\", \"" +
                         std::to_string(parent) + "\"]"));

    g_free(json);
}

TEST_F(FincosysBridgeTest, ExportsHierarchyLinkWithCorrectRoleOrder)
{
    /* gnc_atomspace_create_hierarchy_link() encodes "parent->child" -- the
     * reverse order of gnc_atomspace_create_inheritance_link()'s
     * "child->parent" -- even though both share the same underlying
     * GncAtomType (GNC_ATOM_ACCOUNT_HIERARCHY == GNC_ATOM_INHERITANCE_LINK).
     * The export must not conflate the two. */
    GncAtomHandle parent = gnc_atomspace_create_concept_node("Asset");
    GncAtomHandle child = gnc_atomspace_create_concept_node("Bank");
    GncAtomHandle link = gnc_atomspace_create_hierarchy_link(parent, child);
    ASSERT_NE(0U, link);

    gchar* json = gnc_cognitive_export_fincosys_json();
    ASSERT_NE(nullptr, json);

    std::string text(json);
    EXPECT_NE(std::string::npos, text.find("\"link_type\": \"HierarchyLink\""));
    EXPECT_NE(std::string::npos,
              text.find("\"atoms\": [\"" + std::to_string(parent) + "\", \"" +
                         std::to_string(child) + "\"]"));
    EXPECT_NE(std::string::npos,
              text.find("\"" + std::to_string(parent) + "\": \"parent\""));
    EXPECT_NE(std::string::npos,
              text.find("\"" + std::to_string(child) + "\": \"child\""));

    g_free(json);
}

TEST_F(FincosysBridgeTest, ExportsEvaluationLinkWithPredicateAccountRoles)
{
    GncAtomHandle predicate = gnc_atomspace_create_predicate_node("HasBalance");
    GncAtomHandle account = gnc_atomspace_create_concept_node("Bank");
    GncAtomHandle link =
        gnc_atomspace_create_evaluation_link(predicate, account, 0.9);
    ASSERT_NE(0U, link);

    gchar* json = gnc_cognitive_export_fincosys_json();
    ASSERT_NE(nullptr, json);

    std::string text(json);
    EXPECT_NE(std::string::npos, text.find("\"link_type\": \"EvaluationLink\""));
    EXPECT_NE(std::string::npos, text.find("\"predicate\""));
    EXPECT_NE(std::string::npos, text.find("\"account\""));

    g_free(json);
}

TEST_F(FincosysBridgeTest, TruthValueRoundTripsIntoExport)
{
    GncAtomHandle concept_atom = gnc_atomspace_create_concept_node("Income");
    gnc_atomspace_set_truth_value(concept_atom, 0.75, 0.6);

    gchar* json = gnc_cognitive_export_fincosys_json();
    ASSERT_NE(nullptr, json);

    std::string text(json);
    EXPECT_NE(std::string::npos, text.find("\"strength\": 0.75"));
    EXPECT_NE(std::string::npos, text.find("\"confidence\": 0.6"));

    g_free(json);
}
